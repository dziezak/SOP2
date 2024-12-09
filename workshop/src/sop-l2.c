#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define MAX_CHILDREN 10

ssize_t bulk_read(int fd, char* buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c;
        if (c == 0)
            return len;  // EOF
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

ssize_t bulk_write(int fd, char* buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (c < 0)
            return c;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void usage(int argc, char* argv[])
{
    printf("%s n f \n", argv[0]);
    printf("\tf - file to be processed\n");
    printf("\t0 < n < 10 - number of child processes\n");
    exit(EXIT_FAILURE);
}

void child_work()
{
	printf("Child PID: %d\n", getpid());
	exit(EXIT_SUCCESS);
}

void parent_work(int n)
{
	for(int i=0; i<n; i++)
	{
		pid_t pid = fork();
		if(pid < 0) ERR("fork");
		if(pid == 0) child_work();
	}

	for(int i=0; i<n; i++)
	{
		if(wait(NULL) < 0)
		{
			if(errno == ECHILD) break;
		       	ERR("wait");
		}
	}
	printf("Parent: All children have finished\n");
}

int main(int argc, char* argv[])
{
	if(argc != 2)
	{
		//fprintf(stderr, "USAGE: %s, n\n", argv[0]);
		usage(argc, argv);
		exit(EXIT_FAILURE);
	}
	int n = atoi(argv[1]);
	if(n <= 0 || n >= MAX_CHILDREN)
	{
		fprintf(stderr, "n must be betweeen 1 and %d\n", MAX_CHILDREN -1);
		exit(EXIT_FAILURE);
	}

	sethandler(SIG_IGN, SIGCHLD);
	parent_work(n);

	return EXIT_SUCCESS;
}
