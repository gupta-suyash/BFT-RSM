#include <string>
#include <pwd.h>
#include <filesystem>

#include "pipeline.h"

using std::filesystem::current_path;

int main(int argc, char **argv)
{
	int myuid;
	passwd *mypasswd;
	string TestFileName;
	myuid = getuid();
	mypasswd = getpwuid(myuid);
	TestFileName= mypasswd->pw_dir;
	cout << "My uid is " << myuid << "Path: " << TestFileName << " :: yo:" << getenv("HOME") << "\n\n" <<flush;

	cout << "Current working directory: " << current_path() << endl;
	filesystem::path p = current_path();
	string sss = p.string();
	cout << "Path in string: " << sss << endl;

	Pipeline pp;
	pp.ReadIfconfig(pp.GetPath());

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
