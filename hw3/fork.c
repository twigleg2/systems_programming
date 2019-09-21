#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
	int pid;

	// printf("Starting program; process has pid %d\n", getpid());

	FILE *file = fopen("fork-output.txt", "w");
	fprintf(file, "BEFORE FORK\n");
	fflush(file);

	int p[2];
	if (pipe(p) < 0)
	{
		puts("bad pipe");
		exit(0);
	}

	if ((pid = fork()) < 0)
	{
		fprintf(stderr, "Could not fork()");
		exit(1);
	}

	/* BEGIN SECTION A */
	fprintf(file, "SECTION A\n");
	fflush(file);

	// printf("Section A;  pid %d\n", getpid());
	// sleep(30);

	/* END SECTION A */
	if (pid == 0)
	{
		/* BEGIN SECTION B */
		fprintf(file, "SECTION B\n");
		fflush(file);

		close(p[0]);
		FILE *write_pipe = fdopen(p[1], "w");
		fputs("Hello from section B\n", write_pipe);

		// printf("Section B\n");
		// sleep(30);
		// printf("Section B done sleeping\n");

		exit(0);

		/* END SECTION B */
	}
	else
	{
		/* BEGIN SECTION C */
		fprintf(file, "SECTION C\n");
		fflush(file);

		close(p[1]);
		FILE *read_pipe = fdopen(p[0], "r");
		char message[100];
		fgets(message, 100, read_pipe);
		printf("%s\n", message);

		// printf("Section C\n");
		// sleep(30);
		wait(NULL);
		// sleep(30);
		// printf("Section C done sleeping\n");

		exit(0);

		/* END SECTION C */
	}
	/* BEGIN SECTION D */
	fprintf(file, "SECTION D\n");
	fflush(file);

	printf("Section D\n");
	// sleep(30);

	/* END SECTION D */
}
