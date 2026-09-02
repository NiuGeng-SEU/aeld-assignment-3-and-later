#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    openlog("writer", LOG_PID, LOG_USER);

    if (argc != 3) {
        syslog(LOG_ERR, "Invalid number of arguments: expected 2, got %d", argc - 1);
        fprintf(stderr, "Error: Two arguments required: <writefile> <writestr>\n");
        closelog();
        return 1;
    }

    const char *writefile = argv[1];
    const char *writestr = argv[2];

    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);

    int fd = open(writefile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        syslog(LOG_ERR, "Failed to open file %s: %s", writefile, strerror(errno));
        perror("Error opening file");
        closelog();
        return 1;
    }

    size_t len = strlen(writestr);
    ssize_t bytes_written = write(fd, writestr, len);
    if (bytes_written == -1 || (size_t)bytes_written != len) {
        syslog(LOG_ERR, "Failed to write to file %s: %s", writefile, strerror(errno));
        perror("Error writing to file");
        close(fd);
        closelog();
        return 1;
    }

    close(fd);
    closelog();
    return 0;
}
