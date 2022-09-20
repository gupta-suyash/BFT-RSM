#ifndef _DATA_COMM_
#define _DATA_COMM_

#include <iostream>
#include "types.h"

struct DataPack {
	DataPack() 
	{
		buf = NULL; 
		data_len = 0;
	}

	~DataPack() 
	{
		if(buf) {
			delete [] buf;
			buf = NULL;
		}
	}
	
	char* buf;
	size_t data_len;
};


/*class ProtoMessage
{
	UInt64 block_id;
	char *block;
public:
	static ProtoMessage * SetMessage(UInt64 bid, char *blk);
	UInt64 GetBlockId();
	void SetBlockId(UInt64 bid);
	char * GetBlock();
};*/

#endif
