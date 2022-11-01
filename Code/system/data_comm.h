#ifndef _DATA_COMM_
#define _DATA_COMM_

#include <iostream>

struct DataPack
{
    DataPack()
    {
        buf = NULL;
        data_len = 0;
    }

    ~DataPack()
    {
        if (buf)
        {
            delete[] buf;
            buf = NULL;
        }
    }

    char *buf;
    size_t data_len;
};

/*class ProtoMessage
{
    uint64_t block_id;
    char *block;
public:
    static ProtoMessage * SetMessage(uint64_t bid, char *blk);
    uint64_t GetBlockId();
    void SetBlockId(uint64_t bid);
    char * GetBlock();
};*/

#endif
