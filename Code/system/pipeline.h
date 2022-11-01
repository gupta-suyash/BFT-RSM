#ifndef _PIPELINE_
#define _PIPELINE_

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nng/nng.h"
#include <nng/protocol/pipeline0/pull.h>
#include <nng/protocol/pipeline0/push.h>

#include "data_comm.h"
#include "global.h"

#include "crosschainmessage.pb.h"

using std::filesystem::current_path;

#define NODE0 "node0"
#define NODE1 "node1"

class Pipeline
{
    vector<string> ip_addr; // IP addresses of own RSM.
    // vector <string> tcp_url;
    vector<thread> athreads_; // Input (Receive) threads.

    std::map<uint16_t, nng_socket> send_sockets_;
    std::map<uint16_t, nng_socket> recv_sockets_;

  public:
    Pipeline();
    string GetPath();
    void ReadIfconfig(string if_path);
    string getIP(uint16_t id);

    string GetRecvUrl(uint16_t cnt);
    string GetSendUrl(uint16_t cnt);
    void SetSockets();

    bool SendToOtherRsm(uint16_t nid);
    void RecvFromOtherRsm();

    void SendToOwnRsm();
    void RecvFromOwnRsm();

    void DataSend(crosschain_proto::CrossChainMessage buf, uint16_t node_id);
    crosschain_proto::CrossChainMessage DataRecv(uint16_t node_id);

    char *DeepCopyMsg(char *msg);

    // char* DataToHost();
    // void DataFromHost(char *buf);
};

#endif
