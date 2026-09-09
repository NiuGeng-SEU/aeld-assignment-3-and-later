#include "systemcalls.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    if (cmd == NULL) {
	return false;
    }

    // 调用底层 system()
    int ret = system(cmd);
    if (ret == -1) {
        return false;
    }

    // 判断子进程是否正常退出且返回值为 0
    if (WIFEXITED(ret) && WEXITSTATUS(ret) == 0) {
        return true;
    }

    return false;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    va_end(args);

    // 1. 刷新标准输出，防止 fork 时复制缓冲区导致打印重复
    fflush(stdout);

    // 2. 创建子进程
    pid_t pid = fork();
    if (pid == -1) {
        return false; // fork 失败
    }

    if (pid == 0) {
        // 子进程：执行外部命令（注意 command[0] 必须是绝对路径）
        execv(command[0], command);
        // 如果 execv 执行失败才会到这里，必须退出子进程，防止其继续执行父进程逻辑
        exit(EXIT_FAILURE);
    }

    // 3. 父进程：等待子进程执行完毕
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        return false;
    }

    // 检查子进程是否正常退出，且退出码为 0
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    va_end(args);

    // 1. 打开（或创建）输出文件，权限设为 0644
    int fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        return false;
    }

    // 2. 清空输出缓冲区
    fflush(stdout);

    // 3. 创建子进程
    pid_t pid = fork();
    if (pid == -1) {
        close(fd);
        return false;
    }

    if (pid == 0) {
        // 子进程：将 stdout (文件描述符 1) 重定向到打开的文件 fd
        if (dup2(fd, STDOUT_FILENO) < 0) {
            close(fd);
            exit(EXIT_FAILURE);
        }
        close(fd); // 复制完毕后关闭原描述符

        // 执行命令
        execv(command[0], command);
        exit(EXIT_FAILURE);
    }

    // 4. 父进程：关闭打开的文件描述符（子进程已继承，父进程不再需要）
    close(fd);

    // 等待子进程退出并检查状态
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        return false;
    }

    return (WIFEXITED(status) && WEXITSTATUS(status) == 0);
}
