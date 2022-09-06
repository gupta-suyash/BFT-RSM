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

	unique_ptr<Pipeline> pipe_obj = make_unique<Pipeline>();
	pipe_ptr = pipe_obj.get();
	pipe_ptr->SetSockets();

	//// Setting up queues for sender threads.
	//unique_ptr<SendPipeQueue> sp_queue = make_unique<SendPipeQueue>();
	//sp_qptr = sp_queue.get();
	//sp_qptr->Init();

	//Creating and starting Sender IOThreads.
	unique_ptr<SendThread> snd_obj = make_unique<SendThread>();
	snd_obj->Init(0);

	// Creating and starting Receiver IOThreads.
	unique_ptr<RecvThread> rcv_obj = make_unique<RecvThread>();
	rcv_obj->Init(1);

	snd_obj->thd_.join();
	rcv_obj->thd_.join();


	//Pipeline *pp = new Pipeline();
	//pp->SetSockets();
	//pp->InitThreads();

        fprintf(stderr, "Usage: pipeline %s|%s <URL> <ARG> ...'\n",
                NODE0, NODE1);
        return (1);
}
