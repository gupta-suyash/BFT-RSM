//int main(int argc, char *argv[])
//{
	//int myuid;
	//passwd *mypasswd;
	//string TestFileName;
	//myuid = getuid();
	//mypasswd = getpwuid(myuid);
	//TestFileName= mypasswd->pw_dir;
	//cout << "My uid is " << myuid << "Path: " << TestFileName << " :: yo:" << getenv("HOME") << "\n\n" <<flush;
	
	//cout << "Current working directory: " << current_path() << endl;
	//filesystem::path p = current_path();
	//string sss = p.string();
	//cout << "Path in string: " << sss << endl;
	
	//unique_ptr<IOThreads> ipp = make_unique<IOThreads>();
	//auto pt = ipp.get();
	//pt->SetIThreads();



	//if(argc != 3) {
	//	cout << "Incorrect arguments \n" << flush;
	//	exit(1);
	//}
	
	//string myurl = "tcp://" + pp->getIP(0) + ":3000";
	//const char *url = myurl.c_str();

	//

	//cout << "Node id: " << g_node_id << endl;

        //if (g_node_id == 0)
        //        return (pp->NodeReceive(url));

        //if (g_node_id == 1)
        //        return (pp->NodeSend(url));
//}
//

/*
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
*/

