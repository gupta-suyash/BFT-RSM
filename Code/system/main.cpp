#include <string>
#include <pwd.h>
#include <filesystem>
#include <memory>

#include "global.h"
#include "pipeline.h"
#include "iothread.h"
#include "pipe_queue.h"
#include "ack.h"
#include "message.h"

using std::filesystem::current_path;

void parser(int argc, char *argv[]);

int main(int argc, char *argv[])
{
	// Parsing the command line args.
	parser(argc, argv);

	// Setting up the Acknowledgment object.
	ack_obj = new Acknowledgment();
	ack_obj->Init();


	QuorumAcknowledgment *quack_obj = new QuorumAcknowledgment();
	//ack_obj->TestFunc();
	//quack_obj->TestFunc();
	
	cout << "done" << endl;

	crosschain_proto::CrossChainMessage* msg;
	cout << "done2" << endl;
	msg->set_sequence_id(5);
	cout << "done3" << endl;
	string str = "hello";
	char *cstr = &str[0];
	msg->set_transactions(cstr);
	msg->set_ack_id(10);


	SendMessage::TestFunc();

	unique_ptr<Pipeline> pipe_obj = make_unique<Pipeline>();
	pipe_ptr = pipe_obj.get();
	pipe_ptr->SetSockets();

	// Setting up the queue.
	unique_ptr<PipeQueue> sp_queue = make_unique<PipeQueue>();
	sp_qptr = sp_queue.get();
	sp_qptr->Init();

	// The next command is for testing the queue.
	// sp_qptr->CallThreads();

	//Creating and starting Sender IOThreads.
	unique_ptr<SendThread> snd_obj = make_unique<SendThread>();
	snd_obj->Init(0);

	// Creating and starting Receiver IOThreads.
	unique_ptr<RecvThread> rcv_obj = make_unique<RecvThread>();
	rcv_obj->Init(1);

	snd_obj->thd_.join();
	rcv_obj->thd_.join();

        return (1);
}
