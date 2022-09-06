#include "pipeline.h"


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
void Pipeline::DataToOtherRsm(char *buf, UInt16 node_id) 
{
	int rv;
	auto sock = send_sockets_[node_id];
	size_t sz_msg = strlen(buf) + 1;
	if((rv = nng_send(sock, buf, sz_msg, 0)) != 0) {
		fatal("nng_send", rv);
	}
}

/* Receving data from nodes of other RSM.
 *
 */ 
unique_ptr<DataPack> Pipeline::DataFromOtherRsm(UInt16 node_id)
{
	int rv;
	auto sock = recv_sockets_[node_id];
	unique_ptr<DataPack> msg = make_unique<DataPack>();
	//char *buf = NULL;
	//size_t sz;
	if((rv = nng_recv(sock, &msg->buf, &msg->data_len, NNG_FLAG_ALLOC)) != 0){
		fatal("nng_recv", rv);
	}	
	//nng_free(msg->buf, msg->data_len);
	//return std::move(msg);
	return msg;
}	


/* This function is used to send messages to the nodes in other RSM.
 *
 */ 
void Pipeline::SendToOtherRsm()
{
	int i=0;
	char *msg;
	while(i < 5) {
		// Iterating over nodes of every other RSM.
		for(int k=0; k<get_num_of_rsm(); k++) {
			// Skipping own RSM.
			if(k == get_rsm_id())
				continue;

			// Starting id of each RSM.
			UInt16 rsm_id_start = k*get_nodes_rsm();

			for(int j=0; j<get_nodes_rsm(); j++) {
				// The id of the receiver node.
				UInt16 recvr_id = j + rsm_id_start;

				// Constructing a message to send.
				string str = to_string(get_node_id()) + "x" + to_string(i);
				msg = &str[0];
				cout << get_node_id() << " :: @Sent: " << msg << " :: To: " << recvr_id << endl;  
				DataToOtherRsm(msg, recvr_id);
				i++;
				
			}
		}
	}	
}	


/* This function is used to receive messages from the other RSM.
 *
 */ 
void Pipeline::RecvFromOtherRsm()
{
	while(true) {
		// Iterating over nodes of every other RSM.
		for(int k=0; k<get_num_of_rsm(); k++) {
			// Skipping own RSM.
			if(k == get_rsm_id())
				continue;

			// Starting id of each RSM.
			UInt16 rsm_id_start = k*get_nodes_rsm();

			for(int j=0; j<get_nodes_rsm(); j++) {
				// The id of the sender node.
				UInt16 sendr_id = j + rsm_id_start;
				
				unique_ptr<DataPack> msg = DataFromOtherRsm(sendr_id);
				cout << get_node_id() << " :: @Recv: " <<msg->buf << " :: From: " << sendr_id << endl;
				
			}
		}
	}	
}


/* This function is used to send messages to the nodes in own RSM.
 *
 */ 
void Pipeline::SendToOwnRsm()
{
	int i=0;
	char *msg;
	while(i < 5) {
		// Starting node id of RSM.
		UInt16 rsm_id_start = get_rsm_id() * get_nodes_rsm();

		for(int j=0; j<get_nodes_rsm(); j++) {
			// The id of the receiver node.
			UInt16 recvr_id = j + rsm_id_start;
			
			if(recvr_id == get_node_id())
				continue;

			// Constructing a message to send.
			string str = to_string(get_node_id()) + "x" + to_string(i);
			msg = &str[0];
			cout << get_node_id() << " :: #Sent: " << msg << " :: To: " << recvr_id << endl;  
			DataToOtherRsm(msg, recvr_id);
			i++;
			
		}
	}	
}	


/* This function is used to receive messages from the nodes in own RSM.
 *
 */ 
void Pipeline::RecvFromOwnRsm()
{
	while(true) {
		// Starting id of each RSM.
		UInt16 rsm_id_start = get_rsm_id() * get_nodes_rsm();

		for(int j=0; j<get_nodes_rsm(); j++) {
			cout << "Inside" << endl;
			// The id of the sender node.
			UInt16 sendr_id = j + rsm_id_start;

			if(sendr_id == get_node_id())
				continue;
			
			unique_ptr<DataPack> msg = DataFromOtherRsm(sendr_id);
			cout << get_node_id() << " :: #Recv: " <<msg->buf << " :: From: " << sendr_id << endl;
			
		}
	}	
}

