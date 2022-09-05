#include <string>
#include <pwd.h>
#include <filesystem>
#include <memory>

#include "global.h"
#include "pipeline.h"
#include "iothread.h"
#include "pipe_queue.h"

using std::filesystem::current_path;

void parser(int argc, char *argv[]);

int main(int argc, char *argv[])
{
	// Parsing the command line args.
	parser(argc, argv);

	//// Setting up queues for sender threads.
	//unique_ptr<SendPipeQueue> sp_queue = make_unique<SendPipeQueue>();
	//sp_qptr = sp_queue.get();
	//sp_qptr->Init();

	////Creating Inter RSM Threads
	//unique_ptr<InterSndThread> inter_snd = make_unique<InterSndThread>();
	//inter_snd->Init(0);

	//unique_ptr<InterRcvThread> inter_rcv = make_unique<InterRcvThread>();
	//inter_rcv->Init(1);

	//inter_snd->thd_.join();
	//inter_rcv->thd_.join();


	// Setting up threads.
	//unique_ptr<Pipeline> iop = make_unique<Pipeline>();
	//auto pp = iop.get();
	////pp->SetIThreads();
	//if(get_node_id() == 0) {
	//	pp->NodeReceive("tcp://172.31.24.55:7001");
	//} else {
	//	pp->NodeSend("tcp://172.31.24.55:7001");
	//}	
	
	Pipeline *pp = new Pipeline();
	pp->SetSockets();
	pp->InitThreads();

        fprintf(stderr, "Usage: pipeline %s|%s <URL> <ARG> ...'\n",
                NODE0, NODE1);
        return (1);
}
