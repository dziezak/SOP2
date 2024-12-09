#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <ctype.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d", __FILE__, __LINE__), perror(source), exit(EXIT_FAILURE))

#define MAX_CHILDREN 30

volatile sig_atomic_t terminate_simulation = 0;
volatile sig_atomic_t last_sig = 0;

void sethandler(void (*f)(int, siginfo_t *info, void *), int sigNo) {
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_sigaction = f;
    act.sa_flags = SA_SIGINFO;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void child_sigusr1_handler(int sig, siginfo_t *info, void *context) {
    printf("Child PID: %d received SIGUSR1\n", getpid());
}

void parent_work(int n, pid_t *child_pids) {
    printf("Parent: Sending SIGUSR1 to all children\n");
    for (int i = 0; i < n; i++) {
        if (kill(child_pids[i], SIGUSR1) < 0) {
            perror("kill");
        }
    }
}

void process_file(int n, char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) ERR("open");

    struct stat statbuf;
    if (fstat(fd, &statbuf) == -1) ERR("fstat");
    off_t filesize = statbuf.st_size;

    char *buffer = (char *)malloc(filesize + 1);
    if (buffer == NULL) ERR("malloc");
    read(fd, buffer, filesize);
    close(fd);

    int chunk_size = filesize / n;
    int remainder = filesize % n;

    pid_t *children = malloc(sizeof(pid_t) * n);
    if (children == NULL) ERR("malloc for children array");

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) ERR("fork");

        if (pid == 0) {
            int start = i * chunk_size;
            int end = start + chunk_size;
            if (i == n - 1) end += remainder;

            char *child_data = (char *)malloc(end - start + 1);
            if (!child_data) ERR("malloc for child_data");

            strncpy(child_data, buffer + start, end - start);
            child_data[end - start] = '\0';

            sethandler(child_sigusr1_handler, SIGUSR1);

            // Block SIGUSR1 signal and wait for it
            sigset_t mask, oldmask;
            sigemptyset(&mask);
            sigaddset(&mask, SIGUSR1);
            sigprocmask(SIG_BLOCK, &mask, &oldmask);  // Block SIGUSR1

            printf("Child PID: %d waiting for SIGUSR1\n", getpid());

            // Unblock SIGUSR1 and wait for it
            sigsuspend(&oldmask);

            // Process the assigned fragment
            char output_filename[256];
            snprintf(output_filename, sizeof(output_filename), "%s-%d", filename, i + 1);
            FILE *output_file = fopen(output_filename, "w");
            if (!output_file) ERR("fopen");

            for (int j = 0; j < end - start; j++) {
                if (j % 2 == 1) {
                    if (islower(child_data[j])) {
                        child_data[j] = toupper(child_data[j]);
                    } else if (isupper(child_data[j])) {
                        child_data[j] = tolower(child_data[j]);
                    }
                }
                fputc(child_data[j], output_file);
                fflush(output_file);
                usleep(250000);  // Sleep for 0.25 seconds
            }

            printf("Child PID: %d finished writing to file %s\n", getpid(), output_filename);

            fclose(output_file);
            free(child_data);
            exit(0);
        } else {
            children[i] = pid;
        }
    }

    free(buffer);

    // Wait until all children are set up and ready
    sleep(1);  // Give some time for children to set up handlers

    // Parent process waits for all children to finish
    parent_work(n, children);

    for (int i = 0; i < n; i++) {
        wait(NULL);  // Wait for all children to finish
    }

    free(children);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "USAGE: %s <filename> <number_of_children>\n", argv[0]);
        exit(0);
    }

    char *filename = argv[1];
    int n = atoi(argv[2]);

    if (n <= 0 || n >= MAX_CHILDREN) {
        fprintf(stderr, "Invalid number of children ( 0 < n < %d)\n", MAX_CHILDREN);
        exit(EXIT_FAILURE);
    }

    process_file(n, filename);
    return EXIT_SUCCESS;
}
