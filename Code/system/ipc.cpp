#include "ipc.h"

#include <cstring>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>

/* Opens a linux FIFO pipe to communicate to other processes
 * Will delete a previous file of the same name if it exists
 *
 * @param path is the path to where the pipe should be created
 */
bool createPipe(const std::string& path)
{
    constexpr auto kFullPermissions = 0777;
    const auto success = (mkfifo(path.c_str(), kFullPermissions) == 0);

    if (not success)
    {
        SPDLOG_ERROR(std::strerror(errno));
    }

    return success;
}

/* Takes over the current thread to read from a pipe located at path
 * This assumes that the contents of path will be of form {uint64_t size, uint8_t[size] message}
 *
 * @param path is the path to a pipe to be read from
 * @param messageReads is a queue where all messages read will be enqueued
 * @param exit is a bool to tell the thread to exit (if it is true the thread will return)
 */
void startPipeReader(const std::string& path,
boost::lockfree::queue<std::vector<uint8_t>> * const messageReads,
const std::atomic_bool& exit)
{

}

/* Takes over the current thread to write to a pipe located at path
 * This method will write all data in the form {uint64_t size, uint8_t[size] message}
 *
 * @param path is the path to where the pipe should be created
 * @param messageWrites is the 
 * @param exit is a bool to tell the thread to exit (if it is true the thread will return)
 */
void startPipeWriter(const std::string& path, boost::lockfree::queue<std::vector<uint8_t>> * const messageWrites, const std::atomic_bool& exit)
{

}
