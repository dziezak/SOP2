#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) \
		(fprintf(stderr, "%s:%d", __FILE__, __LINE__), perror(source), exit(EXIT_FAILURE))

#define MAX_CHILDREN 30

volatile sig_atomic_t sick_children[MAX_CHILDREN] = {0};
volatile sig_atomic_t terminate_simulation = 0;

void sethandler(void (*f)(int, siginfo_t *, void *), int sigNo)
{
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_sigaction = f;
	act.sa_flags = SA_SIGINFO;
	if( -1 == sigaction(sigNo, &act, NULL))
		ERR("sigaction");
}

void child_sigusr1_handler(int sig, siginfo_t *info, void *context)
{
	printf("Child[%d]: %d has coughed at me!\n", getpid(), info->si_pid);
	if(rand() % 100 < 50)
	{
		printf("Child[%d] get sick!\n", getpid());
		sick_children[getpid() % MAX_CHILDREN] = 1;
	}
}

void child_sigterm_handler(int sig, siginfo_t *info, void *context)
{
	terminate_simulation = 1;
}

void child_work(int id, int k)
{
	srand(getpid());
	printf("Child[%d] strats day in the kindergarten, ill: %d", getpid(), sick_children[id]);

	struct timespec cough_interval = {0, (rand() % 150 + 50) * 1000000};

	sethandler(child_sigusr1_handler, SIGUSR1);
	sethandler(child_sigterm_handler, SIGTERM);

	int coughs = 0;
	while(!terminate_simulation)
	{
		if(sick_children[id])
		{
			printf("Child[%d] is coughing %d\n", getpid(), coughs);
			kill(-getpid(), SIGUSR1);
			coughs++;
		}
		nanosleep(&cough_interval, NULL);
	}
	printf("Child[%d] exits with %d\n", getpid(), coughs);
	exit(coughs > 255 ? 255 : coughs);
}

void kg_alarm_handler(int sig, siginfo_t *info, void *context)
{
	terminate_simulation = 1;
}

void parent_work(int t, int n)
{
	printf("KG[%d]: Alarm has been set for %d sec\n", getpid(), t);
	sethandler(kg_alarm_handler, SIGALRM);
	alarm(t);
	
	while(!terminate_simulation)
	{
		pause();
	}

	int status, stayed = 0;
	for(int i=0; i<n; i++)
	{
		wait(&status);
		if(WIFEXITED(status))
		{
			int coughs = WEXITSTATUS(status);
			printf("Child[%d] exited with %d\n", i + 1, coughs);
			if(coughs == 0) stayed++;
		}
	}
	printf("%d out of %d children stayed in the kindergarten!\n", stayed, n);
}

int main(int argc, char **argv)
{
	if(argc != 5)
	{
		fprintf(stderr, "USAGE: %s t k n p\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	int t = atoi(argv[1]);
	int k = atoi(argv[2]);
	int n = atoi(argv[3]);
	int p = atoi(argv[4]);

	if(t<1 || t>100 || k<1 || k>100 || n<1 || n>MAX_CHILDREN || p<1 || p>100)
	{
		fprintf(stderr, "Invalid arguments!\n");
		exit(EXIT_FAILURE);
	}

	srand(time(NULL));
	setpgid(0, getpid()); //"Ustawiam grupe procesow dla dzieci"
	
	for(int i=0; i<n; i++)
	{
		sick_children[i] = (i==0) ? 1 : 0; //"Piersze dziecko jest chorym frajerem"
		pid_t pid = fork();
		if(pid<0)
			ERR("fork()");
		if(pid==0)
		{
			setpgid(0, getppid());
			child_work(i, k);
			exit(EXIT_SUCCESS);
		}
	}

//	sethandler(kg_alarm_handler, SIGALRM);
	parent_work(t, n);
	return EXIT_SUCCESS;

}
