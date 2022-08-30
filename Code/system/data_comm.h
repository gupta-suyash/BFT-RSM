#ifndef _DATA_COMM_
#define _DATA_COMM_

#include <iostream>

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

#endif
