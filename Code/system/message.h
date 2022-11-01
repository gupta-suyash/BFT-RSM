#ifndef _MESSAGE_
#define _MESSAGE_

#include "global.h"

class Message
{
  public:
    uint64_t msize_;    // Size of the message.
    MessageType mtype_; // Tyep of message.
    uint64_t txn_id_;   // Identifier associated with the message.
    char *data_;
    uint16_t sender_id_;   // Sending node's identifier.
    uint16_t receiver_id_; // Destination node's identifier.
    uint64_t cumm_ack_;    // Cummulative acknowledgement of received messages.

    static Message *CreateMsg(char *buf, uint64_t sz, MessageType mtype);
    static Message *CreateMsg(char *buf, MessageType mtype);
    static Message *CreateMsg(MessageType mtype);

    virtual void CopyFromBuf(char *buf, uint64_t sz) = 0;
    virtual void CopyFromBuf(char *buf) = 0;
    virtual char *CopyToBuf() = 0;
    virtual uint64_t GetSize() = 0;
};

class SendMessage : public Message
{
  public:
    void CopyFromBuf(char *buf, uint64_t sz);
    void CopyFromBuf(char *buf);
    char *CopyToBuf();
    uint64_t GetSize();

    static void TestFunc();
};

#endif
