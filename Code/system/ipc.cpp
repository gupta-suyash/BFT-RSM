#include "ipc.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>

/* Opens a linux FIFO pipe to communicate to other processes
 * Will delete a previous file of the same name if it exists
 *
 * @param path is the path to where the pipe should be created
 */
bool createPipe(const std::string &path)
{
    constexpr auto kFullPermissions = 0777;

    if (std::filesystem::exists(path))
    {
        const auto eraseError = std::remove(path.c_str()) != 0;
        if (eraseError)
        {
            SPDLOG_ERROR("Cannot Create Erase old pipe at '{}': err={}", path, std::strerror(eraseError));
        }
    }

    const auto success = (mkfifo(path.c_str(), kFullPermissions) == 0);

    if (not success)
    {
        SPDLOG_ERROR("Cannot Create Pipe at '{}': err={}", path, std::strerror(errno));
    }

    return success;
}

/* Takes over the current thread to read from a pipe located at path
 * This assumes that the contents of path will be of form {uint64_t size, uint8_t[size] message}
 *
 * @param path is the path to a pipe to be read from
 * @param messageReads is a queue where all messages read will be enqueued
 */
void startPipeReader(std::string path, std::shared_ptr<ipc::DataChannel> messageReads)
{
    using namespace boost::fibers;
    std::ifstream pipe{path, std::ios_base::binary};

    while (true)
    {
        uint64_t readSize{};
        pipe.read(reinterpret_cast<char *>(&readSize), sizeof(readSize));

        if (pipe.fail())
        {
            SPDLOG_INFO("Attempted to read new message from pipe but failed. EOF_Reached? = {}", pipe.eof());
            messageReads->close();
            return;
        }

        std::vector<uint8_t> message(readSize);
        pipe.read(reinterpret_cast<char *>(message.data()), message.size());

        if (pipe.fail())
        {
            SPDLOG_CRITICAL("ATTEMPTED TO READ {} BYTES, AND READ FAILED", readSize, message.size());
            messageReads->close();
            return;
        }

        const bool isChannelClosed = messageReads->push(std::move(message)) == channel_op_status::closed;
        if (isChannelClosed)
        {
            break;
        }
    }
    SPDLOG_INFO("Reader of pipe '{}' is exiting, channel was closed", path);
}

/* Takes over the current thread to write to a pipe located at path
 * This method will write all data in the form {uint64_t size, uint8_t[size] message}
 *
 * @param path is the path to where the pipe should be created
 * @param messageWrites is the channel which gets messages for this function to write to path
 */
void startPipeWriter(std::string path, std::shared_ptr<ipc::DataChannel> messageWrites)
{
    using namespace boost::fibers;
    std::ofstream pipe{path, std::ios_base::binary};

    std::vector<uint8_t> message{};
    while (messageWrites->pop(message) != channel_op_status::closed)
    {
        uint64_t writeSize = message.size();
        pipe.write(reinterpret_cast<char *>(&writeSize), sizeof(writeSize));
        if (pipe.fail())
        {
            SPDLOG_INFO("Attempted to write new message to pipe but failed. EOF_Reached? = {}", pipe.eof());
            messageWrites->close();
            return;
        }

        pipe.write(reinterpret_cast<char *>(message.data()), message.size());
        if (pipe.fail())
        {
            SPDLOG_INFO("Attempted to write new message to pipe but failed. EOF_Reached? = {}", pipe.eof());
            messageWrites->close();
            return;
        }
        pipe.flush();
    }
    SPDLOG_INFO("Writer of pipe '{}' is exiting, channel was closed", path);
}
