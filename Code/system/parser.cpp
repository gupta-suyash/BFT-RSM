#include "global.h"

void parser(int argc, char *argv[])
{
	cout << "Argc: " << argc << endl;
	for (int i = 1; i < argc; i++)
	{
		if (argv[i][0] == 'n' && argv[i][1] == 'i' && argv[i][2] == 'd'){
			cout << "inside:" << i << endl;
			g_node_id = atoi(&argv[i][4]);
			g_rsm_id = g_node_id / g_nodes_rsm;
		}	
	}
}
