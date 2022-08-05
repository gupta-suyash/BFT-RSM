#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "nng/nng.h"
#include <nng/protocol/pipeline0/pull.h>
#include <nng/protocol/pipeline0/push.h>

using namespace std;

#define NODE0 "node0"
#define NODE1 "node1"

int node0(const char *url_snd, const char *url_rcv);
int node1(const char *url_snd, const char *url_rcv);
