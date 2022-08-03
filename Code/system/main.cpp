#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "pipeline.h"

int main(int argc, char **argv)
{
        if ((argc > 1) && (strcmp(NODE0, argv[1]) == 0))
                return (node0(argv[2]));

        if ((argc > 2) && (strcmp(NODE1, argv[1]) == 0))
                return (node1(argv[2], argv[3]));

        fprintf(stderr, "Usage: pipeline %s|%s <URL> <ARG> ...'\n",
                NODE0, NODE1);
	printf("hello suyash\n");
        return (1);
}
