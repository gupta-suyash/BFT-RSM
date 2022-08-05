#include <string>

#include "pipeline.h"

int main(int argc, char **argv)
{
	if(argc != 4) {
		cout << "Incorrect arguments \n" << flush;
		exit(1);
	}
        if (strcmp(NODE0, argv[1]) == 0)
                return (node0(argv[2], argv[3]));

        if (strcmp(NODE1, argv[1]) == 0)
                return (node1(argv[2], argv[3]));

        fprintf(stderr, "Usage: pipeline %s|%s <URL> <ARG> ...'\n",
                NODE0, NODE1);
        return (1);
}
