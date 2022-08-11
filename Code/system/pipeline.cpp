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

void fatal(const char *func, int rv)
{
        fprintf(stderr, "%s: %s\n", func, nng_strerror(rv));
        exit(1);
}

int Pipeline::NodeReceive(const char *url)
{
	nng_socket sock;
	int rv;
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
		
		printf("NODE0: RECEIVED \"%s\"\n", buf); 
		nng_free(buf, sz);
	}


        //nng_socket sock;
        //int rv;

	//// Receiver Sockets
        //if ((rv = nng_pull0_open(&sock)) != 0) {
        //        fatal("nng_pull0_open", rv);
        //}
	//cout << "node0: 1 \n" << flush;

        //if ((rv = nng_listen(sock, url, NULL, 0)) != 0) {
        //        fatal("nng_listen", rv);
        //}
	//cout << "node0: 2 \n" << flush;

	////// Sender Sockets
	////if ((rv = nng_push0_open(&sock_send)) != 0) {
        ////        fatal("nng_push0_open", rv);
        ////}
	////cout << "node0: 3 \n" << flush;

        ////if ((rv = nng_dial(sock_send, url_snd, NULL, 0)) != 0) {
        ////        fatal("nng_dial", rv);
        ////}
	////cout << "node0: 4 \n" << flush;

        //for (;;) {
        //        char *buf = NULL;
        //        size_t sz;
        //        if ((rv = nng_recv(sock, &buf, &sz, NNG_FLAG_ALLOC)) != 0) {
        //                fatal("nng_recv", rv);
        //        }
        //        printf("NODE0: RECEIVED \"%s\"\n", buf); 
        //        nng_free(buf, sz);
        //}
	//return 0;
}

int Pipeline::NodeSend(const char *url)
{
	//int sz_msg = strlen(msg) + 1;
	nng_socket sock;
	int rv, sz_msg;
	int bytes;
	
	if ((rv = nng_push0_open(&sock)) != 0) {
        	fatal("nng_push0_open", rv);
	}

	if ((rv = nng_dial(sock, url, NULL, 0)) != 0) {
		fatal("nng_dial", rv);
	}
        //printf("NODE1: SENDING \"%s\"\n", msg);
	//if ((rv = nng_send(sock, msg, strlen(msg)+1, 0)) != 0) {
	//	fatal("nng_send", rv);
	//}
	//sleep(1); // wait for messages to flush before shutting down
	//nng_close(sock);
        //return (0);


	string str = "Hello ";
	char *msg;
	for(int i=0; i<10; i++) {
		str += to_string(i);
		msg = &str[0];
		sz_msg = strlen(msg) + 1; // '\0' too

        	printf("NODE1: SENDING \"%s\"\n", msg);
        	if ((rv = nng_send(sock, msg, sz_msg, 0)) != 0) {
        	        fatal("nng_send", rv);
        	}}

        sleep(1); // wait for messages to flush before shutting down
        nng_close(sock);
        return (0);
}


