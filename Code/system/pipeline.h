#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "nng/nng.h"
#include <nng/protocol/pipeline0/pull.h>
#include <nng/protocol/pipeline0/push.h>

#define NODE0 "node0"
#define NODE1 "node1"

int node0(const char *url);
int node1(const char *url, char *msg);
