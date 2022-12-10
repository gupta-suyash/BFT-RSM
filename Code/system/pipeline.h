#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "nng/nng.h"
#include <nng/protocol/pipeline0/pull.h>
#include <nng/protocol/pipeline0/push.h>

#include "global.h"

#include "scrooge_message.pb.h"

using std::filesystem::current_path;

namespace pipeline
{
struct ReceivedCrossChainMessage
{
    scrooge::CrossChainMessage message;
    uint64_t senderId;
};
}; // namespace pipeline

class Pipeline
{
    std::vector<string> ip_addr; // url addresses of own RSM.

    std::unordered_map<uint64_t, nng_socket> send_sockets_;
    std::unordered_map<uint64_t, nng_socket> recv_sockets_;

  public:
    Pipeline();
    string GetPath();
    void ReadIfconfig(string if_path);
    string getIP(uint64_t id);

    string GetRecvUrl(uint64_t cnt);
    string GetSendUrl(uint64_t cnt);
    void SetSockets();

    void SendToOtherRsm(uint64_t receivingNodeId, const scrooge::CrossChainMessage &message);
    std::vector<pipeline::ReceivedCrossChainMessage> RecvFromOtherRsm();

    void BroadcastToOwnRsm(const scrooge::CrossChainMessage &message);
    vector<scrooge::CrossChainMessage> RecvFromOwnRsm();

    void DataSend(const scrooge::CrossChainMessage &buf, uint64_t node_id);
    std::optional<scrooge::CrossChainMessage> DataRecv(uint64_t node_id);
};
