#include "systemcalls.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd) {
    // Use the system() function to execute the command
    int result = system(cmd);

    // Check the result of the system() call
    if (result == 0) {
        // The command completed successfully
        return true;
    } else {
        // The command returned a failure
        return false;
    }
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


bool do_exec(int count, ...) {
    va_list args;
    va_start(args, count);

    char *command[count + 1];
    int i;
    for (i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    pid_t child_pid = fork();

    if (child_pid == -1) {
        // Fork failed
        va_end(args);
        return false;
    }

    if (child_pid == 0) {
        // Child process
        execv(command[0], command + 1);
        // If execv() fails, the code below will be executed
        perror("execv"); // Print an error message
        _exit(EXIT_FAILURE); // Terminate the child process with an error status
    } else {
        // Parent process
        int status;
        waitpid(child_pid, &status, 0);

        va_end(args);

        // Check if the child process terminated successfully
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            return true;
        } else {
            return false;
        }
    }
}



bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i = 0; i < count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    int fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1) {
        perror("Error opening file");
        va_end(args);
        return false;
    }

    // Redirect standard output to the file descriptor
    if (dup2(fd, STDOUT_FILENO) == -1) {
        perror("Error redirecting stdout");
        close(fd);
        va_end(args);
        return false;
    }

    // Close the file descriptor after redirection
    close(fd);

    // Now call execv
    execv(command[0], command);

    // If execv fails, print an error message
    perror("Error in execv");

    va_end(args);

    return false;
}