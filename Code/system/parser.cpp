#include "global.h"

void parser(int argc, char *argv[])
{
	for (int i = 1; i < argc; i++)
	{
		if (argv[i][1] == 'n' && argv[i][2] == 'i' && argv[i][3] == 'd'){
			cout << "inside:" << i << endl;
			g_node_id = atoi(&argv[i][4]);
			g_rsm_id = g_node_id / g_nodes_rsm;
		}	
	}
}
