#include "pipeline.h"
#include "ack.h"
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
string Pipeline::getIP(uint16_t id)
{
    return ip_addr[id];
}

/* Returns the connection URL for receiver threads.
 *
 * @param cnt is the Id the sender node.
 * @return the url.
 */
string Pipeline::GetRecvUrl(uint16_t cnt)
{
    uint16_t port_id = get_port_num() + (get_node_id() * get_nodes_rsm()) + cnt;
    string url = "tcp://" + getIP(get_node_id()) + ":" + to_string(port_id);
    return url;
}

/* Returns the connection URL for the sender threads.
 *
 * @param cnt is the Id of the receiver node.
 * @return the url.
 */
string Pipeline::GetSendUrl(uint16_t cnt)
{
    uint16_t port_id = get_port_num() + (cnt * get_nodes_rsm()) + get_node_id();
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
    for (uint16_t i = 0; i < g_node_cnt; i++)
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
    for (uint16_t i = 0; i < g_node_cnt; i++)
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
void Pipeline::DataSend(crosschain_proto::CrossChainMessage buf, uint16_t node_id)
{
    int rv;
    string buffer;

    // Socket to communicate.
    auto sock = send_sockets_[node_id];

    buf.SerializeToString(&buffer);
    size_t sz = buffer.size();

    SPDLOG_DEBUG("Sending {} bytes to nodeId {}", sz, node_id);

    crosschain_proto::CrossChainMessage chk_buf;
    chk_buf.ParseFromString(buffer);

    SPDLOG_DEBUG("Sent message: [SequenceId={}, AckId={}, transaction='{}']", chk_buf.sequence_id(), chk_buf.ack_id(),
                 chk_buf.transactions());

    if ((rv = nng_send(sock, &buffer[0], sz, 0)) != 0)
    {
        fatal("nng_send", rv);
    }
}

/* Receving data from nodes of other RSM.
 *
 * @param nid is the sender node id.
 * @return the protobuf.
 */
crosschain_proto::CrossChainMessage Pipeline::DataRecv(uint16_t node_id)
{
    int rv;
    char *buffer;
    size_t sz;
    auto sock = recv_sockets_[node_id];

    // We want the nng_recv to be non-blocking and reduce copies.
    // So, we use the two available bit masks.
    rv = nng_recv(sock, &buffer, &sz, NNG_FLAG_ALLOC | NNG_FLAG_NONBLOCK);

    // nng_recv is non-blocking, if there is no data, return value is non-zero.
    if (rv != 0)
    {
        if (rv != 8)
        {
            // Silence error EAGAIN while using nonblocking nng functions
            SPDLOG_ERROR("nng_recv has error value = {}", nng_strerror(rv));
        }

        crosschain_proto::CrossChainMessage fakeMsg;
        fakeMsg.set_transactions("ERROR=" + std::to_string(rv));
        return fakeMsg;
    }

    crosschain_proto::CrossChainMessage receivedMsg;
    const std::string str_buf{buffer, sz};
    receivedMsg.ParseFromString(str_buf);

    SPDLOG_DEBUG("Received {} bytes from nodeId={}, message = [SequenceId={}, AckId={}, transaction='{}']", sz, node_id,
                 receivedMsg.sequence_id(), receivedMsg.ack_id(), receivedMsg.transactions());

    nng_free(buffer, sz);
    return receivedMsg;
}

/* This function is used to send message to a specific node in other RSM.
 *
 * @param nid is the identifier of the node in the other RSM.
 */
bool Pipeline::SendToOtherRsm(uint16_t nid)
{
    // Fetching the block to send, if any.
    crosschain_proto::CrossChainMessage msg = sp_qptr->EnqueueStore();
    if (msg.sequence_id() == 0)
        return false;

    // The id of the receiver node in the other RSM.
    uint16_t recvr_id = nid + (get_other_rsm_id() * get_nodes_rsm());
    // uint16_t recvr_id = 1;

    // Acking the messages received from the other RSM.
    const uint64_t ack_msg = ack_obj->GetAckIterator();
    msg.set_ack_id(ack_msg);

    SPDLOG_DEBUG("Sending message to other RSM: nodeId = {}, message = [SequenceId={}, AckId={}, transaction='{}']",
                 recvr_id, msg.sequence_id(), msg.ack_id(), msg.transactions());

    DataSend(msg, recvr_id);

    return true;
}

/* This function is used to receive messages from the other RSM.
 *
 */
void Pipeline::RecvFromOtherRsm()
{

    // Starting id of the other RSM.
    uint16_t sendr_id_start = get_other_rsm_id() * get_nodes_rsm();

    for (uint16_t j = 0; j < get_nodes_rsm(); j++)
    {
        // The id of the sender node.
        uint16_t sendr_id = j + sendr_id_start;

        crosschain_proto::CrossChainMessage msg = DataRecv(sendr_id);
        if (msg.sequence_id() != 0)
        {
            SPDLOG_DEBUG(
                "Received message from other RSM: nodeId = {}, message = [SequenceId={}, AckId={}, transaction='{}']",
                sendr_id, msg.sequence_id(), msg.ack_id(), msg.transactions());

            // Updating the ack list for msg received.
            ack_obj->AddToAckList(msg.sequence_id());

            // This message needs to broadcasted to other nodes
            // in the RSM, so enqueue in the queue for sender.
            sp_qptr->Enqueue(msg);
        }
    }
}

/* This function is used to send messages to the nodes in own RSM.
 *
 */
void Pipeline::SendToOwnRsm()
{
    // Check the queue if there is any message.
    crosschain_proto::CrossChainMessage msg = sp_qptr->Dequeue();
    if (msg.sequence_id() == 0)
        return;

    // Starting node id of RSM.
    uint16_t rsm_id_start = get_rsm_id() * get_nodes_rsm();

    for (uint16_t j = 0; j < get_nodes_rsm(); j++)
    {
        // The id of the receiver node.
        uint16_t recvr_id = j + rsm_id_start;

        if (recvr_id == get_node_id())
            continue;

        SPDLOG_DEBUG("Sent message to own RSM: nodeId = {}, message = [SequenceId={}, AckId={}, transaction='{}']",
                     recvr_id, msg.sequence_id(), msg.ack_id(), msg.transactions());

        DataSend(msg, recvr_id);
    }
}

/* Creates a copy of the input message.
 *
 * @param buf to be copied
 */
char *Pipeline::DeepCopyMsg(char *buf)
{
    size_t data_len = strlen(buf) + 1;
    char *c_buf = new char[data_len];
    memcpy(c_buf, buf, data_len);
    return c_buf;
}

/* This function is used to receive messages from the nodes in own RSM.
 *
 */
void Pipeline::RecvFromOwnRsm()
{
    // Starting id of each RSM.
    uint16_t rsm_id_start = get_rsm_id() * get_nodes_rsm();

    for (uint16_t j = 0; j < get_nodes_rsm(); j++)
    {
        // The id of the sender node.
        uint16_t sendr_id = j + rsm_id_start;

        if (sendr_id == get_node_id())
            continue;

        crosschain_proto::CrossChainMessage msg = DataRecv(sendr_id);
        if (msg.sequence_id() != 0)
        {
            SPDLOG_DEBUG(
                "Received message from own RSM: nodeId = {}, message = [SequenceId={}, AckId={}, transaction='{}']",
                sendr_id, msg.sequence_id(), msg.ack_id(), msg.transactions());

            // Updating the ack list for msg received.
            ack_obj->AddToAckList(msg.sequence_id());
        }
    }
}
