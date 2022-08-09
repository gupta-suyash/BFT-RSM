#include <string>
#include <pwd.h>
#include <filesystem>

#include "global.h"
#include "pipeline.h"

using std::filesystem::current_path;

void parser(int argc, char *argv[]);

int main(int argc, char *argv[])
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

	parser(argc, argv);

	cout << "Node id: " << g_node_id << endl;

        if (g_node_id == 0)
                return (pp.NodeReceive(url));

        if (g_node_id == 1)
                return (pp.NodeSend(url));

        fprintf(stderr, "Usage: pipeline %s|%s <URL> <ARG> ...'\n",
                NODE0, NODE1);
        return (1);
}
