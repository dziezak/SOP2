#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

#define ERR(source) \
	(fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), exit(EXIT_FAILURE))

volatile sig_atomic_t sig_recived = 0;

void sig_handler(int sig)
{
	sig_recived++;
}

void sethandler(void (*f)(int), int sigNo)
{
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if(sigaction(sigNo, &act, NULL) < 0)
		ERR("sigaction");
}

void child_work(int n, int s)
{
	char filename[256];
	snprintf(filename, sizeof(filename), "PID_%d.TXT", getpid());

	int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if(fd < 0) ERR("open");

	char block[s];
	memset(block, '1', s);

	int block_count = 0;
	while(sig_recived < n)
	{
		pause();
		block_count++;
		if(write(fd, block, s) < 0)
			ERR("write");
		printf("[Child %d] Signal count: %d, writing block %d\n", getpid(), sig_recived, block_count);
	}

	close(fd);
	sleep(1);
	exit(EXIT_SUCCESS);
}

void parent_work(pid_t *children, int count)
{
	for(int i=0; i<10; i++)
	{
		for(int j=0; j<count; j++)
		{
			if(kill(children[j], SIGUSR1) < 0)
				ERR("kill");
		}
		usleep(100000);
	}
	while(wait(NULL) > 0);
}

int main(int argc, char **argv)
{
	if(argc < 2)
	{
		fprintf(stderr, "USAGE: %s <digits>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	sethandler(sig_handler, SIGUSR1);

	printf("argc = %d\n", argc);
	int child_count = argc - 1;
	pid_t children[child_count];

	srand(time(NULL));

	for(int i=0; i<child_count; i++)
	{
		int n = argv[i+1][0] - '0';
		if( n < 0 || n > 9)
		{
			fprintf(stderr, "Indalid digit: %s\n", argv[i+1]);
			exit(EXIT_FAILURE);
		}

		int s = (rand() % 91 + 10) * 1024;
		printf("Child %d: n=%d, s=%d  KB\n", i + 1, n, s/1024);

		pid_t pid = fork();
		if(pid < 0)
			ERR("fork");
		if(pid == 0)
			child_work(n, s);
		children[i] = pid;
	}
	parent_work(children, child_count);
	return EXIT_SUCCESS;
}



