#include "message.h"

Message *Message::CreateMsg(char *buf, uint64_t sz, MessageType mtype)
{
    Message *msg = CreateMsg(mtype);
    msg->CopyFromBuf(buf, sz);

    // COPY_VAL(dest_id,data,ptr);
    return msg;
}

Message *Message::CreateMsg(char *buf, MessageType mtype)
{
    Message *msg = CreateMsg(mtype);
    msg->CopyFromBuf(buf);

    return msg;
}

Message *Message::CreateMsg(MessageType mtype)
{
    Message *msg;
    switch (mtype)
    {
    case kSend:
        msg = new SendMessage();
        break;
    case kForward:
        break;
    default:
        cout << "Invalid Type" << endl;
        exit(1);
    }
    msg->msize_ = UINT64_MAX;
    msg->mtype_ = mtype;
    msg->txn_id_ = UINT64_MAX;
    msg->sender_id_ = UINT16_MAX;
    msg->receiver_id_ = UINT16_MAX;
    msg->cumm_ack_ = UINT64_MAX;
    return msg;
}

void SendMessage::CopyFromBuf(char *buf, uint64_t sz)
{
    // Copying the first field: size of the message.
    msize_ = sz;

    // Copying the buf as the rest of the message; data blob.
    data_ = new char[msize_];
    memcpy(data_, buf, sz);

    cout << "Given Size: " << msize_ << endl;
    cout << "Given Data: " << data_ << endl;
}

void SendMessage::CopyFromBuf(char *buf)
{
    uint64_t ptr = 0;

    // Copying the first field: size of the message.
    COPY_VAL(msize_, buf, ptr);

    // Copying the rest of the messages as the data blob.
    data_ = new char[msize_];
    COPY_VAL(data_, buf, ptr);

    cout << "Buf Size: " << msize_ << endl;
    cout << "Buf Data: " << data_ << endl;
}

char *SendMessage::CopyToBuf()
{
    // Getting the size of the message.
    uint64_t sz = GetSize();

    char *buf = new char[sz];

    uint64_t ptr = 0;
    COPY_BUF(buf, msize_, ptr);
    COPY_BUF(buf, data_, ptr);

    return buf;
}

uint64_t SendMessage::GetSize()
{
    uint64_t sz = 0;
    sz += sizeof(uint64_t);
    sz += msize_;

    return sz;
}

void SendMessage::TestFunc()
{
    string str = "Hello";
    char *c_str = &str[0];
    uint64_t sz = strlen(c_str) + 1;

    Message *msg1 = Message::CreateMsg(c_str, sz, kSend);
    cout << "Checking: " << msg1->msize_ << " :: " << msg1->data_ << endl;

    char *buf = msg1->CopyToBuf();

    Message *msg2 = Message::CreateMsg(buf, kSend);
    cout << "Again: " << msg2->msize_ << " :: " << msg2->data_ << endl;
}
