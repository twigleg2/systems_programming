#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // needed to define getpid(), apparently;

int main(int argc, char *argv[])
{
    int sleep_time;
    if (getenv("SLOWCAT_SLEEP_TIME"))
    {
        sleep_time = atoi(getenv("SLOWCAT_SLEEP_TIME"));
    }
    else
    {
        sleep_time = 1;
    }

    FILE *file;
    if (argc < 2 || strcmp(argv[1], "-") == 0)
    {
        // puts("reading from stdin");
        file = stdin;
    }
    else
    {
        // puts("Reading from file");
        file = fopen(argv[1], "r");
    }

    fprintf(stderr, "%d\n", getpid());
    char line[1000] = "test";
    while (fgets(line, 1000, file))
    {
        printf(line);
        sleep(sleep_time);
    }

    return 0;
}