#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 9000
#define BACKLOG 10
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define CHUNK_SIZE 1024

static volatile sig_atomic_t g_exit_requested = 0;

static void handle_signal(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        g_exit_requested = 1;
    }
}

static int setup_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGINT, &sa, NULL) != 0) return -1;
    if (sigaction(SIGTERM, &sa, NULL) != 0) return -1;
    return 0;
}

static int daemonize(void) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) exit(EXIT_SUCCESS); // 父进程退出

    if (setsid() < 0) return -1;

    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) exit(EXIT_SUCCESS);

    if (chdir("/") < 0) return -1;

    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > 2) close(devnull);
    }
    return 0;
}

int main(int argc, char *argv[]) {
    int daemon_mode = 0;
    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = 1;
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);

    if (setup_signals() != 0) {
        syslog(LOG_ERR, "Failed to register signal handlers: %s", strerror(errno));
        closelog();
        return -1;
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        syslog(LOG_ERR, "Socket creation failed: %s", strerror(errno));
        closelog();
        return -1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        syslog(LOG_ERR, "setsockopt failed: %s", strerror(errno));
        close(server_fd);
        closelog();
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        syslog(LOG_ERR, "Bind failed on port %d: %s", PORT, strerror(errno));
        close(server_fd);
        closelog();
        return -1;
    }

    // 题目严格要求：bind 成功后再转为后台守护进程
    if (daemon_mode) {
        if (daemonize() != 0) {
            syslog(LOG_ERR, "Failed to daemonize: %s", strerror(errno));
            close(server_fd);
            closelog();
            return -1;
        }
    }

    if (listen(server_fd, BACKLOG) < 0) {
        syslog(LOG_ERR, "Listen failed: %s", strerror(errno));
        close(server_fd);
        closelog();
        return -1;
    }

    while (!g_exit_requested) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EINTR && g_exit_requested) {
                break;
            }
            syslog(LOG_ERR, "Accept failed: %s", strerror(errno));
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        // 动态接收报文并按 '\n' 分帧
        size_t buf_cap = CHUNK_SIZE;
        size_t buf_len = 0;
        char *recv_buf = malloc(buf_cap);
        if (!recv_buf) {
            close(client_fd);
            continue;
        }

        int file_fd = open(DATA_FILE, O_CREAT | O_WRONLY | O_APPEND, 0666);
        if (file_fd < 0) {
            syslog(LOG_ERR, "Failed to open data file: %s", strerror(errno));
            free(recv_buf);
            close(client_fd);
            continue;
        }

        ssize_t bytes_recv;
        int packet_complete = 0;
        while (!packet_complete && (bytes_recv = recv(client_fd, recv_buf + buf_len, buf_cap - buf_len - 1, 0)) > 0) {
            buf_len += bytes_recv;
            recv_buf[buf_len] = '\0';

            char *newline_pos = strchr(recv_buf, '\n');
            if (newline_pos) {
                size_t packet_size = (newline_pos - recv_buf) + 1;
                if (write(file_fd, recv_buf, packet_size) < 0) {
                    syslog(LOG_ERR, "Failed to write data file: %s", strerror(errno));
                }
                packet_complete = 1;
            } else if (buf_len >= buf_cap - 1) {
                buf_cap += CHUNK_SIZE;
                char *new_buf = realloc(recv_buf, buf_cap);
                if (!new_buf) {
                    syslog(LOG_ERR, "Memory allocation failed");
                    break;
                }
                recv_buf = new_buf;
            }
        }

        close(file_fd);
        free(recv_buf);

        // 报文接收完成，流式回传完整文件内容至客户端（防止大文件挤爆 RAM）
        if (packet_complete) {
            int read_fd = open(DATA_FILE, O_RDONLY);
            if (read_fd >= 0) {
                char send_buf[CHUNK_SIZE];
                ssize_t bytes_read;
                while ((bytes_read = read(read_fd, send_buf, sizeof(send_buf))) > 0) {
                    ssize_t sent_total = 0;
                    while (sent_total < bytes_read) {
                        ssize_t sent = send(client_fd, send_buf + sent_total, bytes_read - sent_total, 0);
                        if (sent < 0) break;
                        sent_total += sent;
                    }
                }
                close(read_fd);
            }
        }

        close(client_fd);
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    syslog(LOG_INFO, "Caught signal, exiting");
    close(server_fd);
    unlink(DATA_FILE);
    closelog();
    return 0;
}
