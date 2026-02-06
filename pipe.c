#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
    // need at least one command
    if (argc < 2) {
        fprintf(stderr, "Usage: %s cmd1 [cmd2 ...]\n", argv[0]);
        exit(EINVAL);
    }

    // single command, no pipes needed
    if (argc == 2) {
        execlp(argv[1], argv[1], NULL);
        perror("execlp");
        exit(errno);
    }

    // multiple commands, need pipes
    int num_cmds = argc - 1;
    int fd[2];
    int prev_fd = -1;
    int exit_status = 0;

    for (int i = 0; i < num_cmds; i++) {
        // create pipe for all except last command
        if (i < num_cmds - 1) {
            if (pipe(fd) == -1) {
                perror("pipe");
                exit(1);
            }
        }

        pid_t child_pid = fork();
        if (child_pid == -1) {
            perror("fork");
            exit(1);
        }

        if (child_pid == 0) {
            // child process

            // read from previous pipe if not first command
            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }

            // write to current pipe if not last command
            if (i < num_cmds - 1) {
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
            }

            // exec the command
            execlp(argv[i + 1], argv[i + 1], NULL);
            perror(argv[i + 1]);
            exit(errno);
        }

        // parent process

        // close prev_fd since child inherited it
        if (prev_fd != -1) {
            close(prev_fd);
        }

        // close write end, save read end for next iteration
        if (i < num_cmds - 1) {
            close(fd[1]);
            prev_fd = fd[0];
        }

    }

    // wait for all children after launching the full pipeline
    int status;
    for (int i = 0; i < num_cmds; i++) {
        waitpid(-1, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            exit_status = WEXITSTATUS(status);
        }
    }

    return exit_status;
}
