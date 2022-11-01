#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "crosschainmessage.pb.h"
#include <google/protobuf/util/json_util.h>

crosschain_proto::CrossChainMessage generateMessage(const int i)
{
    crosschain_proto::CrossChainMessage message;
    message.set_sequence_id(i);
    message.set_ack_id(7 * i);
    message.set_transactions("This is the " + std::to_string(i) + "th message");

    return message;
}

void outputTestFile(const std::string &fileOutputName)
{
    static constexpr auto kNumberOfTests = 500;
    std::ofstream output{fileOutputName};

    output << "INPUT" << std::endl;
    for (int i = 0; i < kNumberOfTests; i++)
    {
        const auto generatedMessage = generateMessage(i);
        output << generatedMessage.SerializeAsString().size() << '\n'
               << generatedMessage.SerializeAsString() << std::endl;
    }

    output << "EXPECTED_OUTPUT" << std::endl;
    for (int i = 0; i < kNumberOfTests; i++)
    {
        const auto generatedMessage = generateMessage(i);
        std::string jsonString;
        google::protobuf::util::MessageToJsonString(generatedMessage, &jsonString);
        output << jsonString << std::endl;
    }
}

void readInput(std::ifstream &input, std::vector<std::string> *serializedStrings, std::vector<std::string> *jsonStrings)
{
    const std::string INPUT_START = "INPUT";
    const std::string OUTPUT_START = "EXPECTED_OUTPUT";
    std::string line;

    std::getline(input, line);
    if (INPUT_START != line)
    {
        std::cout << "First line should be " << INPUT_START << "\nExiting...";
        exit(1);
    }

    std::getline(input, line); // Go to input start
    while (OUTPUT_START != line)
    {
        const auto numberOfBytes = std::stoull(line);
        char *const buff = new char[numberOfBytes + 1];

        input.read(buff, numberOfBytes);
        buff[numberOfBytes] = '\0'; // null terminate string ?? wtf c++
        std::getline(input, line);  // clear newline
        line = buff;

        serializedStrings->push_back(line);

        std::getline(input, line);
    }

    while (std::getline(input, line))
    {
        jsonStrings->push_back(line);
    }
}

void testInput(const std::string &filename)
{
    std::ifstream input{filename};
    std::vector<std::string> serializedStrings;
    std::vector<std::string> jsonStrings;

    readInput(input, &serializedStrings, &jsonStrings);

    assert(serializedStrings.size() == jsonStrings.size() && "Searilized and Json quantity differ");

    bool testPassed = true;

    for (size_t i = 0; i < serializedStrings.size(); i++)
    {
        const auto &serializedString = serializedStrings[i];
        const auto &jsonString = jsonStrings[i];

        crosschain_proto::CrossChainMessage searilizedMessage;
        crosschain_proto::CrossChainMessage jsonMessage;

        searilizedMessage.ParseFromString(serializedString);
        google::protobuf::util::JsonStringToMessage(jsonString, &jsonMessage);

        const auto correctSequenceId = searilizedMessage.sequence_id() == jsonMessage.sequence_id();
        const auto correctTransaction = searilizedMessage.transactions() == jsonMessage.transactions();
        const auto correctAckId = searilizedMessage.ack_id() == jsonMessage.ack_id();

        if (!(correctSequenceId && correctTransaction && correctAckId))
        {
            testPassed = false;
            std::string serializedJson;
            google::protobuf::util::MessageToJsonString(searilizedMessage, &serializedJson);
            std::cout << "ERROR: Message " << i << " not equal on [" << (correctSequenceId ? "" : "seq_id ")
                      << (correctTransaction ? "" : "transaction ") << (correctAckId ? "" : "ackId") << "]\n";
            std::cout << "\tExpected: " << jsonString << "\n\tReceived: " << serializedJson << std::endl;
        }
    }
    std::cout << "Test Finished.\n" << (testPassed ? "Success!\n" : "Failure\n");
}

int main(int argc, char *argv[])
{
    const std::string kFileName = "OUTPUT";
    if (std::string{"read"} == argv[1])
    {
        testInput(kFileName);
    }
    else if (std::string{"write"} == argv[1])
    {
        outputTestFile(kFileName);
    }
    else
    {
        std::cout << "PROVIDE INPUT READ OR WRITE" << std::endl;
    }
    return 0;
}
