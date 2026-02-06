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

    int num_cmds = argc - 1;
    int fd[2];
    int prev_fd = -1;
    int exit_status = 0;

    for (int i = 0; i < num_cmds; i++) {
        // create pipe for all except last command
        if (i < num_cmds - 1) {
            if (pipe(fd) == -1) {
                perror("pipe");
                exit(errno);
            }
        }

        pid_t child_pid = fork();
        if (child_pid == -1) {
            perror("fork");
            exit(errno);
        }

        if (child_pid == 0) {
            // child process

            // read from previous pipe if not first command
            if (prev_fd != -1) {
                if (dup2(prev_fd, STDIN_FILENO) == -1) {
                    perror("dup2");
                    exit(errno);
                }
                if (close(prev_fd) == -1) {
                    perror("close");
                    exit(errno);
                }
            }

            // write to current pipe if not last command
            if (i < num_cmds - 1) {
                if (dup2(fd[1], STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(errno);
                }
                if (close(fd[0]) == -1) {
                    perror("close");
                    exit(errno);
                }
                if (close(fd[1]) == -1) {
                    perror("close");
                    exit(errno);
                }
            }

            // exec the command
            execlp(argv[i + 1], argv[i + 1], NULL);
            perror(argv[i + 1]);
            exit(errno);
        }

        // parent process

        // close prev_fd since child inherited it
        if (prev_fd != -1) {
            if (close(prev_fd) == -1) {
                perror("close");
                exit(errno);
            }
        }

        // close write end, save read end for next iteration
        if (i < num_cmds - 1) {
            if (close(fd[1]) == -1) {
                perror("close");
                exit(errno);
            }
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
