#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // Proces potomny
        printf("Child process (PID: %d) is running...\n", getpid());
        sleep(2);
        printf("Child process (PID: %d) exiting with code 42\n", getpid());
        exit(42); // Zwraca kod zakończenia 42
    } else {
        // Proces macierzysty
        int status;
        pid_t child_pid;

        printf("Parent process (PID: %d) waiting for child process...\n", getpid());
        child_pid = waitpid(pid, &status, 0);

        if (child_pid > 0) {
            printf("Child process (PID: %d) has terminated.\n", child_pid);
            if (WIFEXITED(status)) {
                printf("Exit status: %d\n", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                printf("Terminated by signal: %d\n", WTERMSIG(status));
            }
        } else {
            perror("waitpid failed");
        }
    }

    return 0;
}
