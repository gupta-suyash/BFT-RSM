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

	// Setting up send queues.
	unique_ptr<SendPipeQueue> sp_queue = make_unique<SendPipeQueue>();
	sp_qptr = sp_queue.get();
	sp_qptr->CallThreads();

	// Setting up threads.
	unique_ptr<Pipeline> iop = make_unique<Pipeline>();
	auto pp = iop.get();
	pp->SetIThreads();

	

        fprintf(stderr, "Usage: pipeline %s|%s <URL> <ARG> ...'\n",
                NODE0, NODE1);
        return (1);
}
