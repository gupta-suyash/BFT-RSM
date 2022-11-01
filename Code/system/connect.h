
/* This file is supposed to be imported by the protocol
 * running at each node and it will use these functions
 * to send messages to scrooge.
 *
 */

#include <iostream>

// TODO: Define correct parameters for these functions.
void SendBlock(const uint64_t block_id, const char *block);
void ReceiveBlock();
void Init();
