#include "pipeline.h"
#include "ack.h"
#include "pipe_queue.h"

Pipeline::Pipeline()
{
	string ifconfig_path = GetPath();
	ReadIfconfig(ifconfig_path);
}	

/* Generates the path to ifconfig.txt.
 * 
 * @return the path to ifconfig.txt.
 */
string Pipeline::GetPath() 
{
	filesystem::path p = current_path(); // Path of rundb location.

	// At present, we assume ifconfig.txt is located at this path.
	string if_path = p.string() + "/configuration/ifconfig.txt";
	cout << "Path in string: " << if_path << endl;
	return if_path;
}	

/* Collects all the ip addresses in the vector ip_addr.
 * 
 * @param if_path is the vector of ip addresses.
 */
void Pipeline::ReadIfconfig(string if_path)
{
	std::ifstream fin{if_path};
	if (!fin) {
		cout << "Error opening file for reading" << endl;
		exit(1);
	}

	// Line by line fetching each IP address and storing them in vector.
	string ips;
	while(getline(fin, ips)) {
		ip_addr.push_back(ips);
		cout << "IP: " << ips << endl;
	}

	//for(int i=0; i<ip_addr.size(); i++) {
	//	cout << "Stored IP: " << ip_addr[0] << endl;
	//}	
}	

/* Returns the required IP address from the vector ip_addr.
 *
 * @param id is the required IP.
 * @return the IP address.
 */ 
string Pipeline::getIP(UInt16 id)
{
	return ip_addr[id];
}	

/* Returns the connection URL for receiver threads.
 *
 * @param cnt is the Id the sender node.
 * @return the url.
 */
string Pipeline::GetRecvUrl(UInt16 cnt) 
{
	UInt16 port_id = get_port_num() + (get_node_id() * get_nodes_rsm()) + cnt;
	string url = "tcp://" + getIP(get_node_id()) + ":" + to_string(port_id); 
	return url;
}	

/* Returns the connection URL for the sender threads.
 *
 * @param cnt is the Id of the receiver node.
 * @return the url.
 */
string Pipeline::GetSendUrl(UInt16 cnt) 
{
	UInt16 port_id = get_port_num() + (cnt * get_nodes_rsm()) + get_node_id();
	string url = "tcp://" + getIP(cnt) + ":" + to_string(port_id); 
	return url;
}


void fatal(const char *func, int rv)
{
        fprintf(stderr, "%s: %s\n", func, nng_strerror(rv));
        exit(1);
}


/* This function is used to just create sockets to other nodes. 
 * For each node, we add two sockets: one send_socket and other recv_socket. 
 *
 */ 
void Pipeline::SetSockets()
{
	int rv;

	// Initializing sender sockets.
	for(UInt16 i=0; i<g_node_cnt; i++) {
		if(i != get_node_id()) {
			string url = GetSendUrl(i);
			const char *curl = url.c_str();
			cout << "To " << i << " :: " << curl << endl;

			nng_socket sock;

			// Attempt to open the socket; fatal if fails.
			if ((rv = nng_push0_open(&sock)) != 0) {
        			fatal("nng_push0_open", rv);
			}

			// Asynchronous wait for someone to listen to this socket.
			nng_dial(sock, curl, NULL, NNG_FLAG_NONBLOCK);
			send_sockets_.insert({i,sock});
		}
	}	

	sleep(1);
	
	// Initializing receiver sockets.
	for(UInt16 i=0; i<g_node_cnt; i++) {
		if(i != get_node_id()) {
			string url = GetRecvUrl(i);
			const char *curl = url.c_str();
			cout << "From " << i << " :: " << curl << endl;

			nng_socket sock;
			// Attempt to open the socket; fatal if fails.
			if ((rv = nng_pull0_open(&sock)) != 0) {
        			fatal("nng_pull0_open", rv);
			}

			// Wait for someone to dial up a socket.
			nng_listen(sock, curl, NULL, 0);
			recv_sockets_.insert({i,sock});
		}
	}
}	


/* Sending data to nodes of other RSM.
 *
 */
void Pipeline::DataSend(crosschain_proto::CrossChainMessage* buf, UInt16 node_id) 
{
	int rv;
	auto sock = send_sockets_[node_id];
	//size_t sz_msg = strlen(buf) + 1;
	if((rv = nng_send(sock, (void *)buf, 1500, 0)) != 0) {
		fatal("nng_send", rv);
	}
}

/* Receving data from nodes of other RSM.
 *
 */ 
unique_ptr<DataPack> Pipeline::DataRecv(UInt16 node_id)
{
	int rv;
	auto sock = recv_sockets_[node_id];
	unique_ptr<DataPack> msg = make_unique<DataPack>();
	
	// We want the nng_recv to be non-blocking and reduce copies. 
	// So, we use the two available bit masks.
	rv = nng_recv(sock, &msg->buf, &msg->data_len, NNG_FLAG_ALLOC | NNG_FLAG_NONBLOCK);

	// nng_recv is non-blocking, if there is no data, return value is non-zero.
	if(rv != 0)
		msg->data_len = 0;

	//nng_free(msg->buf, msg->data_len);
	return msg;
}	


/* This function is used to send message to a specific node in other RSM.
 *
 * @param nid is the identifier of the node in the other RSM.
 */ 
void Pipeline::SendToOtherRsm(UInt16 nid)
{
	// Fetching the block to send, if any.
	unique_ptr<ProtoMessage> bmsg = sp_qptr->EnqueueStore();
	if(bmsg->GetBlockId() == 0)
		return;


	// The id of the receiver node in the other RSM.
	UInt16 recvr_id = nid + (get_other_rsm_id() * get_nodes_rsm());
	
	// Acking the messages received from the other RSM.
	UInt64 ack_msg = ack_obj->GetAckIterator();
 
	crosschain_proto::CrossChainMessage* msg;
	msg->set_sequence_id(bmsg->GetBlockId());
	msg->set_transactions(bmsg->GetBlock());
	msg->set_ack_id(ack_msg);
	// TODO: At this point, we need to create a protobuf.
	// In the meantime, we are just treating the message to send as string.
	
	ProtoMessage *pmsg = bmsg.release();
	
	cout << get_node_id() << " :: @Sent: " << pmsg->GetBlock() << " :: Last Ack: " << ack_msg << " :: To: " << recvr_id << endl;  

	DataSend(msg, recvr_id);
}	


/* This function is used to receive messages from the other RSM.
 *
 */ 
void Pipeline::RecvFromOtherRsm()
{
	
	// Iterating over nodes of every other RSM.
	for(UInt16 k=0; k<get_num_of_rsm(); k++) {
		// Skipping own RSM.
		if(k == get_rsm_id())
			continue;

		// Starting id of each RSM.
		UInt16 rsm_id_start = k*get_nodes_rsm();

		for(UInt16 j=0; j<get_nodes_rsm(); j++) {
			// The id of the sender node.
			UInt16 sendr_id = j + rsm_id_start;
			
			unique_ptr<DataPack> msg = DataRecv(sendr_id);
			if(msg->data_len != 0) {
				cout << get_node_id() << " :: @Recv: " <<msg->buf << " :: From: " << sendr_id << endl;

				/* TODO
				 * Take the transaction id from the message and 
				 * call the objet of class Acknowledgment to add 
				 * it to the list msg_recv.
				 */ 

				// This message needs to broadcasted to other nodes
				// in the RSM, so enqueue in the queue for sender.
				sp_qptr->Enqueue(std::move(msg));
			}

			
		}
	}
		
}


/* This function is used to send messages to the nodes in own RSM.
 *
 */ 
void Pipeline::SendToOwnRsm()
{
	// Check the queue if there is any message.
	unique_ptr<DataPack> msg = sp_qptr->Dequeue();
	if(msg->data_len == 0)
		return;       
	
	// Starting node id of RSM.
	UInt16 rsm_id_start = get_rsm_id() * get_nodes_rsm();

	for(UInt16 j=0; j<get_nodes_rsm(); j++) {
		// The id of the receiver node.
		UInt16 recvr_id = j + rsm_id_start;
		
		if(recvr_id == get_node_id())
			continue;

		// Constructing a message to send.
		//string str = to_string(get_node_id()) + "x" + to_string(i);
		//buf = &str[0];
		  
		
		// Create a copy of the message as it is sent to all the nodes.
		// TODO: Maybe we do not need to as nng_send automatically copies?
		char *buf = DeepCopyMsg(msg->buf);
		cout << get_node_id() << " :: #Sent: " << buf << " :: To: " << recvr_id << endl;

		//DataSend(buf, recvr_id);
	}
		
}	


/* Creates a copy of the input message.
 *
 * @param buf to be copied
 */ 
char *Pipeline::DeepCopyMsg(char *buf)
{
	size_t data_len = strlen(buf) + 1;
	char *c_buf = new char[data_len];
	memcpy(c_buf, buf, data_len);
	return c_buf;
}	

/* This function is used to receive messages from the nodes in own RSM.
 *
 */ 
void Pipeline::RecvFromOwnRsm()
{
	// Starting id of each RSM.
	UInt16 rsm_id_start = get_rsm_id() * get_nodes_rsm();

	for(UInt16 j=0; j<get_nodes_rsm(); j++) {
		// The id of the sender node.
		UInt16 sendr_id = j + rsm_id_start;

		if(sendr_id == get_node_id())
			continue;
		
		unique_ptr<DataPack> msg = DataRecv(sendr_id);
		if(msg->data_len != 0) {
			cout << get_node_id() << " :: #Recv: " <<msg->buf << " :: From: " << sendr_id << endl;

			/* TODO
			 * Take the transaction id from the message and 
			 * call the objet of class Acknowledgment to add 
			 * it to the list msg_recv.
			 */
		}	
		
	}
		
}

