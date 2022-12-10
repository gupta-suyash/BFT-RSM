#include "pipeline.h"
#include "acknowledgement.h"
#include "pipe_queue.h"

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
    SPDLOG_INFO("IfConfigPath = {}", if_path);
    return if_path;
}

/* Collects all the ip addresses in the vector ip_addr.
 *
 * @param if_path is the vector of ip addresses.
 */
void Pipeline::ReadIfconfig(string if_path)
{
    std::ifstream fin{if_path};
    if (!fin)
    {
        SPDLOG_CRITICAL("Error opening file for reading");
        exit(1);
    }

    // Line by line fetching each IP address and storing them in vector.
    string ips;
    while (getline(fin, ips))
    {
        ip_addr.push_back(ips);
        SPDLOG_INFO("IP = {}", ips);
    }
}

/* Returns the required IP address from the vector ip_addr.
 *
 * @param id is the required IP.
 * @return the IP address.
 */
string Pipeline::getIP(uint64_t id)
{
    SPDLOG_INFO("ID: {} IP Addr Size: {}", id, ip_addr.size());
    return ip_addr[id];
}

/* Returns the connection URL for receiver threads.
 *
 * @param cnt is the Id the sender node.
 * @return the url.
 */
string Pipeline::GetRecvUrl(uint64_t cnt)
{
    uint64_t port_id = get_port_num() + (get_node_id() * get_nodes_rsm()) + cnt;
    string url = "tcp://" + getIP(get_node_id()) + ":" + to_string(port_id);
    return url;
}

/* Returns the connection URL for the sender threads.
 *
 * @param cnt is the Id of the receiver node.
 * @return the url.
 */
string Pipeline::GetSendUrl(uint64_t cnt)
{
    uint64_t port_id = get_port_num() + (cnt * get_nodes_rsm()) + get_node_id();
    string url = "tcp://" + getIP(cnt) + ":" + to_string(port_id);
    return url;
}

void fatal(const char *func, int rv)
{
    SPDLOG_CRITICAL("Fatal '{}' {}", func, nng_strerror(rv));
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
    for (uint64_t i = 0; i < g_node_cnt; i++)
    {
        if (i != get_node_id())
        {
            string url = GetSendUrl(i);
            const char *curl = url.c_str();
            SPDLOG_INFO("Generated URL for sending to nodeId = {} as '{}'", i, url);

            nng_socket sock;

            // Attempt to open the socket; fatal if fails.
            if ((rv = nng_push0_open(&sock)) != 0)
            {
                fatal("nng_push0_open", rv);
            }

            // Asynchronous wait for someone to listen to this socket.
            nng_dial(sock, curl, NULL, NNG_FLAG_NONBLOCK);
            send_sockets_.insert({i, sock});
        }
    }

    sleep(1);

    // Initializing receiver sockets.
    for (uint64_t i = 0; i < g_node_cnt; i++)
    {
        if (i != get_node_id())
        {
            string url = GetRecvUrl(i);
            const char *curl = url.c_str();
            SPDLOG_INFO("Generated URL for receiving from nodeId = {} as '{}'", i, url);

            nng_socket sock;
            // Attempt to open the socket; fatal if fails.
            if ((rv = nng_pull0_open(&sock)) != 0)
            {
                fatal("nng_pull0_open", rv);
            }

            // Wait for someone to dial up a socket.
            nng_listen(sock, curl, NULL, 0);
            recv_sockets_.insert({i, sock});
        }
    }
}

/* Sending data to nodes of other RSM.
 *
 * @param buf is the outgoing message of protobuf type.
 * @param nid is the destination node id.
 */
void Pipeline::DataSend(const scrooge::CrossChainMessage &buf, uint64_t node_id)
{
    int rv;
    string buffer;

    // Socket to communicate.
    auto sock = send_sockets_[node_id];

    buf.SerializeToString(&buffer);
    size_t sz = buffer.size();

    SPDLOG_DEBUG("Sending {} bytes to nodeId {}", sz, node_id);

    if ((rv = nng_send(sock, &buffer[0], sz, 0)) != 0)
    {
        SPDLOG_CRITICAL("NNG_SEND ERROR when sending to node {}, error = {}", node_id, nng_strerror(rv));
    }
}

/* Receving data from nodes of other RSM.
 *
 * @param nid is the sender node id.
 * @return the protobuf.
 */
std::optional<scrooge::CrossChainMessage> Pipeline::DataRecv(const uint64_t node_id)
{
    int rv;
    char *buffer;
    size_t sz;
    auto sock = recv_sockets_.at(node_id);

    // We want the nng_recv to be non-blocking and reduce copies.
    // So, we use the two available bit masks.
    rv = nng_recv(sock, &buffer, &sz, NNG_FLAG_ALLOC | NNG_FLAG_NONBLOCK);

    // nng_recv is non-blocking, if there is no data, return value is non-zero.
    if (rv != 0)
    {
        if (rv != 8)
        {
            // Silence error EAGAIN while using nonblocking nng functions
            SPDLOG_CRITICAL("nng_recv has error value = {}", nng_strerror(rv));
        }

        return std::nullopt;
    }

    scrooge::CrossChainMessage receivedMsg;
    const std::string str_buf{buffer, sz};
    receivedMsg.ParseFromString(str_buf);

    SPDLOG_DEBUG("Received {} bytes from nodeId={}, message = [SequenceId={}, AckId={}, transaction='{}']", sz, node_id,
                 receivedMsg.data().sequence_number(), receivedMsg.ack_count(), receivedMsg.data().message_content());

    nng_free(buffer, sz);
    return receivedMsg;
}

/* This function is used to send message to a specific node in other RSM.
 *
 * @param nid is the identifier of the node in the other RSM.
 */
void Pipeline::SendToOtherRsm(uint64_t receivingNodeId, const scrooge::CrossChainMessage& message)
{
    // The id of the receiver node in the other RSM.
    uint64_t recvr_id = receivingNodeId + (get_other_rsm_id() * get_nodes_rsm());

    SPDLOG_DEBUG("Sending message to other RSM: nodeId = {}, message = [SequenceId={}, AckId={}, transaction='{}']",
                 recvr_id, message.data().sequence_number(), message.ack_count(), message.data().message_content());

    DataSend(message, recvr_id);
}

/* This function is used to receive messages from the other RSM.
 *
 */
std::vector<scrooge::CrossChainMessage> Pipeline::RecvFromOtherRsm()
{
    // Starting id of the other RSM.
    uint64_t sendr_id_start = get_other_rsm_id() * get_nodes_rsm();
    std::vector<scrooge::CrossChainMessage> newMessages{};

    for (uint64_t curNode = 0; curNode < get_nodes_rsm(); curNode++)
    {
        // The id of the sender node.
        uint64_t sendr_id = curNode + sendr_id_start;

        const auto message = DataRecv(sendr_id);
        if (message.has_value())
        {
            SPDLOG_DEBUG(
                "Received message from other RSM: nodeId = {}, message = [SequenceId={}, AckId={}, transaction='{}']",
                sendr_id, message->data().sequence_number(), message->ack_count(), message->data().message_content());

            // This message needs to broadcasted to other nodes
            // in the RSM, so enqueue in the queue for sender.
            newMessages.push_back(std::move(*message));
        }
    }
    return newMessages;
}

/* This function is used to send messages to the nodes in own RSM.
 *
 */
void Pipeline::BroadcastToOwnRsm(const scrooge::CrossChainMessage &message)
{
    // Starting node id of RSM.
    uint64_t rsm_id_start = get_rsm_id() * get_nodes_rsm();

    for (uint64_t curNode = 0; curNode < get_nodes_rsm(); curNode++)
    {
        // The id of the receiver node.
        uint64_t recvr_id = curNode + rsm_id_start;

        if (recvr_id == get_node_id())
            continue;

        DataSend(message, recvr_id);

        SPDLOG_DEBUG("Sent message to own RSM: nodeId = {}, message = [SequenceId={}, AckId={}, transaction='{}']",
                     recvr_id, message.data().sequence_number(), message.ack_count(), message.data().message_content());
    }
}

/* This function is used to receive messages from the nodes in own RSM.
 *
 */
vector<scrooge::CrossChainMessage> Pipeline::RecvFromOwnRsm()
{
    std::vector<scrooge::CrossChainMessage> messages{};
    uint64_t rsm_id_start = get_rsm_id() * get_nodes_rsm();

    for (uint64_t curNode = 0; curNode < get_nodes_rsm(); curNode++)
    {
        // The id of the sender node.
        uint64_t sendr_id = curNode + rsm_id_start;

        if (sendr_id == get_node_id())
            continue;

        const auto message = DataRecv(sendr_id);
        if (message.has_value())
        {
            SPDLOG_DEBUG(
                "Received message from own RSM: nodeId = {}, message = [SequenceId={}, AckId={}, transaction='{}']",
                sendr_id, message->data().sequence_number(), message->ack_count(), message->data().message_content());
            messages.push_back(std::move(*message));
        }
    }

    return messages;
}
