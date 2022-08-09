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

	//if(argc != 3) {
	//	cout << "Incorrect arguments \n" << flush;
	//	exit(1);
	//}
	
	string myurl = "tcp://" + pp.getIP(0) + ":3000";
	const char *url = myurl.c_str();
        if (strcmp(NODE0, argv[1]) == 0)
                return (pp.node0(url));

        if (strcmp(NODE1, argv[1]) == 0)
                return (pp.node1(url));

        fprintf(stderr, "Usage: pipeline %s|%s <URL> <ARG> ...'\n",
                NODE0, NODE1);
        return (1);
}
