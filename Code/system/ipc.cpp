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
    // constexpr auto kFullPermissions = 0777;

    // const auto mkfifoResult = mkfifo(path.c_str(), kFullPermissions);
    // const bool isError = mkfifoResult == -1 && errno != EEXIST;

    // if (isError)
    // {
    //     SPDLOG_CRITICAL("Cannot Create Pipe at '{}': err={}", path, std::strerror(errno));
    // }

    // return not isError;
    return true;
}

/* Takes over the current thread to read from a pipe located at path
 * This assumes that the contents of path will be of form {uint64_t size, uint8_t[size] message}
 *
 * @param path is the path to a pipe to be read from
 * @param messageReads is a queue where all messages read will be enqueued
 * @param exit is a bool to tell the thread to exit (it will be set to true if the thread should or has returned)
 */
void startPipeReader(std::string path, std::shared_ptr<ipc::DataChannel> messageReads,
                     std::shared_ptr<std::atomic_bool> exit)
{
    std::ifstream pipe{path, std::ios_base::binary};

    while (not exit->load())
    {
        uint64_t readSize{};
        pipe.read(reinterpret_cast<char *>(&readSize), sizeof(readSize));

        if (pipe.fail())
        {
            SPDLOG_CRITICAL("Attempted to read new message from pipe but failed. EOF_Reached? = {}", pipe.eof());
            break;
        }

        std::vector<uint8_t> message(readSize);
        pipe.read(reinterpret_cast<char *>(message.data()), message.size());

        if (pipe.fail())
        {
            SPDLOG_CRITICAL("ATTEMPTED TO READ {} BYTES, AND READ FAILED", readSize, message.size());
            break;
        }

        while (!messageReads->push(std::move(message)))
        {
            if (exit->load())
            {
                SPDLOG_CRITICAL("Reader of pipe '{}' is exiting", path);
                *exit = true;
                return;
            }
            // std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }
    }
    *exit = true;
    SPDLOG_CRITICAL("Reader of pipe '{}' is exiting", path);
}

/* Takes over the current thread to write to a pipe located at path
 * This method will write all data in the form {uint64_t size, uint8_t[size] message}
 *
 * @param path is the path to where the pipe should be created
 * @param messageWrites is the
 * @param exit is a bool to tell the thread to exit (it will be set to true if the thread should or has returned)
 */
void startPipeWriter(std::string path, std::shared_ptr<ipc::DataChannel> messageWrites,
                     std::shared_ptr<std::atomic_bool> exit)
{
    std::ofstream pipe{path, std::ios_base::binary};

    while (not exit->load())
    {
        std::vector<uint8_t> message{};
        while (!messageWrites->pop(message))
        {
            if (exit->load())
            {
                SPDLOG_INFO("Writer of pipe '{}' is exiting", path);
                *exit = true;
                return;
            }
            // std::this_thread::sleep_for(std::chrono::milliseconds{10});
        }

        uint64_t writeSize = message.size();
        pipe.write(reinterpret_cast<char *>(&writeSize), sizeof(writeSize));
        if (pipe.fail())
        {
            SPDLOG_INFO("Attempted to write new message tot pipe but failed. EOF_Reached? = {}", pipe.eof());
            break;
        }

        pipe.write(reinterpret_cast<char *>(message.data()), message.size());
        if (pipe.fail())
        {
            SPDLOG_INFO("Attempted to write new message from pipe but failed. EOF_Reached? = {}", pipe.eof());
            break;
        }
        pipe.flush();
    }

    *exit = true;
    SPDLOG_INFO("Writer of pipe '{}' is exiting", path);
}

std::string readMessage(std::ifstream &file)
{
    uint64_t readSize{};
    file.read(reinterpret_cast<char *>(&readSize), sizeof(readSize));

    if (file.fail())
    {
        SPDLOG_CRITICAL("Attempted to read new message from pipe but failed. EOF_Reached? = {}", file.eof());
        return "";
    }

    std::string message(readSize, '\0');
    file.read(reinterpret_cast<char *>(message.data()), message.size());

    if (file.fail())
    {
        SPDLOG_CRITICAL("ATTEMPTED TO READ {} BYTES, AND READ FAILED", readSize, message.size());
        return "";
    }
    return message;
}

void writeMessage(std::ofstream &file, const std::string &data)
{
    uint64_t writeSize = data.size();
    file.write(reinterpret_cast<char *>(&writeSize), sizeof(writeSize));
    if (file.fail())
    {
        SPDLOG_CRITICAL("Attempted to write new message to pipe but failed. EOF_Reached? = {}", file.eof());
    }

    file.write(data.data(), data.size());
    if (file.fail())
    {
        SPDLOG_CRITICAL("Attempted to write new message from pipe but failed. EOF_Reached? = {}", file.eof());
    }
    file.flush();
}
