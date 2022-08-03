#include "pipeline.h"

void fatal(const char *func, int rv)
{
        fprintf(stderr, "%s: %s\n", func, nng_strerror(rv));
        exit(1);
}

int node0(const char *url)
{
        nng_socket sock;
        int rv;

        if ((rv = nng_pull0_open(&sock)) != 0) {
		//printf("issues");
                fatal("nng_pull0_open", rv);
        }
        if ((rv = nng_listen(sock, url, NULL, 0)) != 0) {
		//printf("issues");
                fatal("nng_listen", rv);
        }
        for (;;) {
                char *buf = NULL;
                size_t sz;
                if ((rv = nng_recv(sock, &buf, &sz, NNG_FLAG_ALLOC)) != 0) {
			//printf("issues");
                        fatal("nng_recv", rv);
                }
                printf("NODE0: RECEIVED \"%s\"\n", buf); 
                nng_free(buf, sz);
        }
	return 0;
}

int node1(const char *url)
{
        nng_socket sock;
        int rv, bytes, sz_msg;

        if ((rv = nng_push0_open(&sock)) != 0) {
                fatal("nng_push0_open", rv);
        }
        if ((rv = nng_dial(sock, url, NULL, 0)) != 0) {
                fatal("nng_dial", rv);
        }

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


