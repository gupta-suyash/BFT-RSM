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

/* This function spawns the sender and receiver threads at each node. 
 * The sender threads send outgoing messages to other nodes, while 
 * the receiver threads wait for incoming messages from other nodes.
 * At present, we are creating one sender thread and one receiver thread for 
 * each node in the system.
 *
 */ 
void Pipeline::SetIThreads() 
{
	
	cout << "Recv URL:" << endl;
	for(UInt16 i=0; i<g_node_cnt; i++) {
		if(i != get_node_id()) {
			string rurl = GetRecvUrl(i);
			cout << "From " << i << " :: " << rurl << endl;
			//tcp_url.push_back(rurl);
			thread it = thread(&Pipeline::NodeReceive, this, rurl);
			athreads_.push_back(move(it));	
		}
	}	

	sleep(3);
	
	cout << "Send URL: " << endl;
	for(UInt16 i=0; i<g_node_cnt; i++) {
		if(i != get_node_id()) {
			string surl = GetSendUrl(i);
			cout << "To " << i << " :: " << surl << endl;
			//tcp_url.push_back(surl);
			thread ot(&Pipeline::NodeSend, this, surl);
			athreads_.push_back(move(ot));
		}
	}
	

	for(auto &th : athreads_) {
		th.join();
	}
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

	sleep(3);
	
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
	cout << get_node_id() << " :: " <<msg->buf << endl;
	//nng_free(msg->buf, msg->data_len);
	//return std::move(msg);
	return msg;
}	

void Pipeline::InitThreads()
{
	if(get_node_id() == 0) {
		recv_thd_ = thread(&Pipeline::RunRecv, this);
		recv_thd_.join();
	} else {
		send_thd_ = thread(&Pipeline::RunSend, this);
		send_thd_.join();
	}
}	


void Pipeline::RunSend()
{
	int i=0;
	char *msg;
	while(i < 5) {
		for(int j=0; j<g_node_cnt; j++) {
			if(j != get_node_id()) {
				string str = "abc" + to_string(i);
				msg = &str[0];
				cout << "Sent: " << msg << " :: to: " << j << endl;  
				DataToOtherRsm(msg, j);
				i++;
			}
		}	
	}	
}	

void Pipeline::RunRecv()
{
	while(true) {
		for(int j=0; j<g_node_cnt; j++) {
			if(j != get_node_id()) {
				unique_ptr<DataPack> msg = DataFromOtherRsm(j);
				//DataFromOtherRsm(j);
				cout << "RECO: " << get_node_id() << " :: " <<msg->buf << endl;
			}
		}
	}	
}

int Pipeline::NodeReceive(string tcp_url)
{
	cout << "Inside" << endl;
	nng_socket sock;
	int rv;

	const char *url = tcp_url.c_str();
	cout << "Con URL:" << url << endl;

	if ((rv = nng_pull0_open(&sock)) != 0) {
		fatal("nng_pull0_open", rv);
	}
        if ((rv = nng_listen(sock, url, NULL, 0)) != 0) {
		fatal("nng_listen", rv);
	}
	for (;;) {
		char *buf = NULL;
		size_t sz;
		if ((rv = nng_recv(sock, &buf, &sz, NNG_FLAG_ALLOC)) != 0) {
			fatal("nng_recv", rv);
		}
		
		cout << get_node_id() << " RECEIVED: " << buf << endl; 
		nng_free(buf, sz);
	}
}

int Pipeline::NodeSend(string tcp_url)
{
	//int sz_msg = strlen(msg) + 1;
	nng_socket sock;
	int rv, sz_msg;
	int bytes;
	
	const char *url = tcp_url.c_str();
	cout << "Con URL:" << url << endl;

	if ((rv = nng_push0_open(&sock)) != 0) {
        	fatal("nng_push0_open", rv);
	}

	if ((rv = nng_dial(sock, url, NULL, NNG_FLAG_NONBLOCK)) != 0) {
		fatal("nng_dial", rv);
	}

	string str = "From: " + to_string(get_node_id()) + " :: Hello ";
	char *msg;
	for(int i=0; i<3; i++) {
		str += to_string(i);
		msg = &str[0];
		sz_msg = strlen(msg) + 1; // '\0' too

        	cout << get_node_id() << " SENDING: " << msg << endl;
        	if ((rv = nng_send(sock, msg, sz_msg, 0)) != 0) {
        	        fatal("nng_send", rv);
        	}}

        sleep(1); // wait for messages to flush before shutting down
        nng_close(sock);
        return (0);
}


