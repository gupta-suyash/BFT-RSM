#include "pipeline.h"

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


void fatal(const char *func, int rv)
{
        fprintf(stderr, "%s: %s\n", func, nng_strerror(rv));
        exit(1);
}

int node0(const char *url_snd, const char *url_rcv)
{
        nng_socket sock_send, sock_recv;
        int rv;

	// Receiver Sockets
        if ((rv = nng_pull0_open(&sock_recv)) != 0) {
                fatal("nng_pull0_open", rv);
        }
	cout << "node0: 1 \n" << flush;

        if ((rv = nng_listen(sock_recv, url_rcv, NULL, 0)) != 0) {
                fatal("nng_listen", rv);
        }
	cout << "node0: 2 \n" << flush;

	// Sender Sockets
	if ((rv = nng_push0_open(&sock_send)) != 0) {
                fatal("nng_push0_open", rv);
        }
	cout << "node0: 3 \n" << flush;

        if ((rv = nng_dial(sock_send, url_snd, NULL, 0)) != 0) {
                fatal("nng_dial", rv);
        }
	cout << "node0: 4 \n" << flush;

        for (;;) {
                char *buf = NULL;
                size_t sz;
                if ((rv = nng_recv(sock_recv, &buf, &sz, NNG_FLAG_ALLOC)) != 0) {
                        fatal("nng_recv", rv);
                }
                printf("NODE0: RECEIVED \"%s\"\n", buf); 
                nng_free(buf, sz);
        }
	return 0;
}

int node1(const char *url_snd, const char *url_rcv)
{
        nng_socket sock_send, sock_recv;
        int rv, bytes, sz_msg;

	// Sender Sockets
        if ((rv = nng_push0_open(&sock_send)) != 0) {
                fatal("nng_push0_open", rv);
        }
	cout << "node1: 1 \n" << flush;

        if ((rv = nng_dial(sock_send, url_snd, NULL, 0)) != 0) {
                fatal("nng_dial", rv);
        }
	cout << "node1: 2 \n" << flush;

	// Receiver Sockets
        if ((rv = nng_pull0_open(&sock_recv)) != 0) {
                fatal("nng_pull0_open", rv);
        }
	cout << "node1: 3 \n" << flush;

        if ((rv = nng_listen(sock_recv, url_rcv, NULL, 0)) != 0) {
                fatal("nng_listen", rv);
        }
	cout << "node1: 4 \n" << flush;	

	string str = "Hello ";
	char *msg;
	for(int i=0; i<10; i++) {
		str += to_string(i);
		msg = &str[0];
		sz_msg = strlen(msg) + 1; // '\0' too

        	printf("NODE1: SENDING \"%s\"\n", msg);
        	if ((rv = nng_send(sock_send, msg, sz_msg, 0)) != 0) {
        	        fatal("nng_send", rv);
        	}}

        sleep(1); // wait for messages to flush before shutting down
        nng_close(sock_send);
        return (0);
}


