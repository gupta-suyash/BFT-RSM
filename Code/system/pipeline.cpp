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


string Pipeline::GetRecvUrl(UInt16 cnt) 
{
	UInt16 port_id = get_port_num() + (get_node_id() * get_nodes_rsm()) + cnt;
	string url = "tcp://" + getIP(get_node_id()) + ":" + to_string(port_id); 
	return url;
}	

string Pipeline::GetSendUrl(UInt16 cnt) 
{
	UInt16 port_id = get_port_num() + (cnt * get_nodes_rsm()) + get_node_id();
	string url = "tcp://" + getIP(cnt) + ":" + to_string(port_id); 
	return url;
}

void Pipeline::SetIThreads() 
{
	
	cout << "Recv URL:" << endl;
	for(UInt16 i=0; i<g_node_cnt; i++) {
		if(i != get_node_id()) {
			string rurl = GetRecvUrl(i);
			cout << "From " << i << " :: " << rurl << endl;
			//auto rptr = iopipe_.get();
			tcp_url.push_back(rurl);
			thread it = thread(&Pipeline::NodeReceive, this);
			athreads_.push_back(move(it));	
		}
	}	

	sleep(1);
	
	cout << "Send URL: " << endl;
	for(UInt16 i=0; i<g_node_cnt; i++) {
		if(i != get_node_id()) {
			string surl = GetSendUrl(i);
			cout << "To " << i << " :: " << surl << endl;
			//auto sptr = iopipe_.get();
			tcp_url.push_back(surl);
			thread ot(&Pipeline::NodeSend, this);
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

int Pipeline::NodeReceive()
{
	cout << "Inside" << endl;
	nng_socket sock;
	int rv;

	const char *url = tcp_url[0].c_str();
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

int Pipeline::NodeSend()
{
	//int sz_msg = strlen(msg) + 1;
	nng_socket sock;
	int rv, sz_msg;
	int bytes;
	
	const char *url = tcp_url[0].c_str();
	cout << "Con URL:" << url << endl;

	if ((rv = nng_push0_open(&sock)) != 0) {
        	fatal("nng_push0_open", rv);
	}

	if ((rv = nng_dial(sock, url, NULL, 0)) != 0) {
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


