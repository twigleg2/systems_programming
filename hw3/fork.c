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
		fprintf(stderr, "could not pipe()");
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

	/* END SECTION A */
	if (pid == 0)
	{
		/* BEGIN SECTION B */
		fprintf(file, "SECTION B\n");
		fflush(file);

		close(p[0]);
		FILE *write_pipe = fdopen(p[1], "w");
		fputs("Hello from section B\n", write_pipe);

		/* BEGIN EXECVE */
		char *newenviron[] = {NULL};

		printf("Program \"%s\" has pid %d.\n", argv[0], getpid());

		if (argc <= 1)
		{
			printf("No program to exec.  Exiting...\n");
			exit(0);
		}

		printf("Running exec of \"%s\"\n", argv[1]);
		dup2(fileno(file), 1);
		execve(argv[1], &argv[1], newenviron);
		printf("End of program \"%s\".\n", argv[0]);
		/* END EXECVE */

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

		wait(NULL);

		exit(0);

		/* END SECTION C */
	}
	/* BEGIN SECTION D */
	fprintf(file, "SECTION D\n");
	fflush(file);

	printf("Section D\n");

	/* END SECTION D */
}
