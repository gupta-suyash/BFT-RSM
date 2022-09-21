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
	//cout << "Fatal " << endl;
        fprintf(stderr, "%s: %s\n", func, nng_strerror(rv));
        exit(1);
}


/* This function is used to just create sockets to other nodes. 
 * For each node, we add two sockets: one send_socket and other recv_socket. 
 *
 */ 
void Pipeline::SetSockets()
{
	//cout << "SetSockets" << endl;
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
 * @param buf is the outgoing message of protobuf type.
 * @param nid is the destination node id.
 */
void Pipeline::DataSend(crosschain_proto::CrossChainMessage buf, UInt16 node_id) 
{
	//cout << "DataSend" << endl;
	int rv;
	string buffer;

	// Socket to communicate.
	auto sock = send_sockets_[node_id];

	// Serialize to array for sending.
	//buf.SerializeToArray(buffer, sz);
	buf.SerializeToString(&buffer);
	size_t sz = buffer.size();

	// TODO: The next line should be removed.
	cout << get_node_id() << " :: @Sent: " << buf.sequence_id() << " :: Content: " << buf.transactions() << " :: Last Ack: " << buf.ack_id() << " :: Size: " << sz << " :: To: " << node_id << endl;

	if((rv = nng_send(sock, &buffer[0], sz, 0)) != 0) {
		fatal("nng_send", rv);
	}
}

/* Receving data from nodes of other RSM.
 *
 * @param nid is the sender node id.
 * @return the protobuf.
 */ 
crosschain_proto::CrossChainMessage Pipeline::DataRecv(UInt16 node_id)
{
	//cout << "DataRecv" << endl;
	int rv; 
	char* buffer;
	size_t sz;
	auto sock = recv_sockets_[node_id];
	
	// We want the nng_recv to be non-blocking and reduce copies. 
	// So, we use the two available bit masks.
	rv = nng_recv(sock, &buffer, &sz, NNG_FLAG_ALLOC | NNG_FLAG_NONBLOCK);

	crosschain_proto::CrossChainMessage buf;

	// nng_recv is non-blocking, if there is no data, return value is non-zero.
	if(rv != 0) {
		//cout << "One" << endl;
		buf.set_sequence_id(0);
		buf.set_ack_id(0);
		buf.set_transactions("hello");
	} else {
		//cout << "Two: " << sz << " :: " << buffer << endl;
		std::string str_buf(buffer);
		bool flag = buf.ParseFromString(str_buf);
		//cout << "Parsed: " << buf.sequence_id() << " :: " << buf.transactions() << " :: Ack: " << buf.ack_id() << endl;
	}
	
	//nng_free(msg->buf, msg->data_len);
	return buf;
}	


/* This function is used to send message to a specific node in other RSM.
 *
 * @param nid is the identifier of the node in the other RSM.
 */ 
bool Pipeline::SendToOtherRsm(UInt16 nid)
{
	//cout << "SendToOtherRSM" << endl;
	// Fetching the block to send, if any.
	crosschain_proto::CrossChainMessage msg = sp_qptr->EnqueueStore();
	if(msg.sequence_id() == 0)
		return false;

	// The id of the receiver node in the other RSM.
	UInt16 recvr_id = nid + (get_other_rsm_id() * get_nodes_rsm());
	//UInt16 recvr_id = 1;
	
	// Acking the messages received from the other RSM.
	UInt64 ack_msg = ack_obj->GetAckIterator();
	msg.set_ack_id(ack_msg);

	//cout << "Before: " << msg.sequence_id() << " :: Content: " << msg.transactions() << " :: Last Ack: " << msg.ack_id() << endl;

	DataSend(msg, recvr_id);

	return true;
}	


/* This function is used to receive messages from the other RSM.
 *
 */ 
void Pipeline::RecvFromOtherRsm()
{
	//cout << "RecvFromOtherRSM" << endl;
	
	// Starting id of the other RSM.
	UInt16 sendr_id_start = get_other_rsm_id() * get_nodes_rsm();

	for(UInt16 j=0; j<get_nodes_rsm(); j++) {
		// The id of the sender node.
		UInt16 sendr_id = j + sendr_id_start;

		crosschain_proto::CrossChainMessage msg = DataRecv(sendr_id);
		if(msg.sequence_id() != 0) {
			cout << get_node_id() << " :: @Recv: " <<msg.sequence_id() << " :: " << msg.transactions() << " :: " << msg.ack_id() << " :: From: " << sendr_id << endl;

			/* TODO
			 * Take the transaction id from the message and 
			 * call the objet of class Acknowledgment to add 
			 * it to the list msg_recv.
			 */ 

			// This message needs to broadcasted to other nodes
			// in the RSM, so enqueue in the queue for sender.
			//sp_qptr->Enqueue(std::move(msg));
		}
	}
}


/* This function is used to send messages to the nodes in own RSM.
 *
 */ 
void Pipeline::SendToOwnRsm()
{
	//cout << "SendToOwnRsm" << endl;
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
	//cout << "DeepCopyMsg" << endl;
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
	//cout << "RecvFromOwnRsm" << endl;
	// Starting id of each RSM.
	UInt16 rsm_id_start = get_rsm_id() * get_nodes_rsm();

	for(UInt16 j=0; j<get_nodes_rsm(); j++) {
		// The id of the sender node.
		UInt16 sendr_id = j + rsm_id_start;

		if(sendr_id == get_node_id())
			continue;
		
		unique_ptr<DataPack> msg;// = DataRecv(sendr_id);
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

