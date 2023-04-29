#include <jsoncpp/json/json.h>
#include <atomic>
#include <bit>
#include <vector>
#include <map>
#include <string>
#include <thread>
#include <fstream>
#include <optional>
#include <boost/asio.hpp>
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "statstracker.h"

#include <../include/rapidjson/document.h>
#include <../include/rapidjson/writer.h>
#include <../include/rapidjson/stringbuffer.h>

using namespace boost::asio;
using ip::tcp;

static constexpr uint64_t EXP_DURATION = 1;
static constexpr uint64_t WAIT_FOR_ACKS = 100;
struct Transaction
{
    int64_t seq_id;
    std::string value;
};

struct ServerResponse
{
    uint64_t ack_count;
};

class Client {
    public:
        /******************** Primary Functions ******************************/

        /* Get list of transactions */
        virtual std::vector<Transaction> getTransactionList(std::string experiment_name) = 0;

        /* Runs all the experiments on the client */
        virtual void runExperiments() = 0;

        /* Records the appropriate performance metrics */
        virtual void reportPerformanceMetrics(std::string experiment_name) = 0;
       
        /******************** Helper Functions ******************************/
        /* Fill the maps of transaction lists */
        virtual void loadTrace() = 0;
        /* Send transaction to server */
        virtual void sendTransaction(std::string experiment_name) = 0;
        /* Waits for the Server response */
        virtual void receiveServerResponse(std::string experiment_name) = 0;

        /******************** Variables of the Class **********************/
        std::map<std::string, std::vector<Transaction>> transactions_list;
        std::string path_to_config;
        Json::Value config;
        std::vector<std::string> experiment_names; 
        // std::map<std::string, StatisticsInterpreter> stats;
        
};

class AlgorandClient : public virtual Client {
    private:
        boost::asio::io_service send_io_service;
        boost::asio::io_service receive_io_service;
        std::vector<tcp::socket> send_sockets;
        tcp::socket receive_socket;
        std::vector<std::string> send_ips;
        std::string receive_ip;
        uint64_t send_port;
        uint64_t receive_port;
        std::string filename;

        uint64_t final_max_seq_id;

        uint64_t ack_counter = 0;

    public:
        /* 
         * Sets up Client connection to the primary se(rver machine and loads in
         * the appropriate traces into the clients' list of transactions 
         */
        AlgorandClient(std::string config, 
                       std::vector<std::string> experiments)
        : send_sockets(4, tcp::socket(send_io_service)), receive_socket(receive_io_service) {
            this->path_to_config = config;
            this->experiment_names = experiments;
            
            std::vector<Transaction> transactions_list = {};
            
            loadTrace();
        }

        /* Create a list of transactions */
        void loadTrace() {
            // Need to read in the config
            std::ifstream configFile(path_to_config, std::ifstream::binary);
            Json::Value config;
            try
            {
                configFile >> config;
            }
            catch (...)
            {
                std::cout << "Could not find config at path " << path_to_config << std::endl;
            }
            this->send_port = config["general"]["send_port"].asUInt64();
            for (size_t i = 0; i < config["general"]["send_ip"].size(); i++) {
                const Json::ArrayIndex idx = i;
                send_ips.push_back(config["general"]["send_ip"][idx].asString());
                send_sockets[i].connect(tcp::endpoint(boost::asio::ip::address::from_string(send_ips[i]), send_port));
            }
            this->receive_ip = config["general"]["receive_ip"].asString();
            this->receive_port = config["general"]["receive_port"].asUInt64();
            this->filename = config["general"]["filename"].asString();
            configFile.close();
        }

        /* Get list of transactions */
        std::vector<Transaction> getTransactionList(std::string experiment_name) {
            return transactions_list[experiment_name];
        }

        /* Send transaction to server */
        void sendTransaction(std::string experiment_name) {
            // standard send
            std::cout << "Execute send transaction thread with " << transactions_list[experiment_name].size() << std::endl;
            uint64_t num_sent = 0;
            // int64_t internal_seq_id = 0;
            std::atomic<int64_t>* internal_seq_id = new std::atomic<int64_t>(0);
            auto start = std::chrono::steady_clock::now();
            for (size_t i = 0; i < send_sockets.size(); i++) { 
                pid_t pid = fork();
                if (pid == 0) {
                    // This is the child process
                    while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < EXP_DURATION) {
                        Transaction txn = Transaction{*internal_seq_id, this->filename};
                        (*internal_seq_id)++;
                        // internal_seq_id += 1;
                        
                        /* Create JSON to send */
                        rapidjson::Document response(rapidjson::kObjectType);
                        response.SetObject();
                        response.AddMember("sequence_id", txn.seq_id, response.GetAllocator());
                        rapidjson::Value str_val;
                        str_val.SetString(txn.value.c_str(), response.GetAllocator());
                        response.AddMember("value", str_val, response.GetAllocator());
                        rapidjson::StringBuffer response_buffer;
                        rapidjson::Writer<rapidjson::StringBuffer> writer(response_buffer);
                        response.Accept(writer);
                        std::string json_str = response_buffer.GetString();
                        json_str += '\n';

                        /* Send the JSON */
                        boost::system::error_code error;
                        boost::asio::write(send_sockets[i], boost::asio::buffer(json_str), error );
                        if( !error ) {
                            // std::cout << "Transaction sent: " << txn.seq_id << std::endl;
                            num_sent += 1;
                            // if (num_sent > 3000) {
                            //     break;
                            // }
                        } else {
                            std::cout << "send failed: " << error.message() << std::endl;
                            break;
                        }
                        startClientTimer(txn.seq_id);
                    }
                    std::cout << "Message sent: " << num_sent << std::endl;
                    send_sockets[i].close();
                    _exit(0);
                } else if (pid < 0) {
                    // Error occurred
                    std::cerr << "Failed to fork process for command for socket " << i << std::endl;
                }
            }

            int status;
            while (wait(&status) > 0) {
                std::cout << "Child process exited with status " << WEXITSTATUS(status) << std::endl;
            } 
        }

        /* Waits for the Server response */
        void receiveServerResponse(std::string experiment_name) {
            /* Register receiving URL */
            boost::asio::ip::tcp::acceptor acceptor(receive_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), receive_port));
            acceptor.accept(receive_socket);
            std::cout << "Accepted connection from " << receive_socket.remote_endpoint().address().to_string() << std::endl;
            uint64_t num_received = 0;
            auto start = std::chrono::steady_clock::now();
            while (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count() < WAIT_FOR_ACKS) {
                // TODO: Timeout of some sort to actually close the socket
                // getting response from server
                boost::system::error_code error;
                boost::asio::streambuf receive_buffer;
                boost::asio::read(receive_socket, receive_buffer, boost::asio::transfer_all(), error);
                if(error && error != boost::asio::error::eof ) {
                    std::cout << "receive failed: " << error.message() << std::endl;
                    break;
                }
                else {
                    const char* data = boost::asio::buffer_cast<const char*>(receive_buffer.data());
                    std::cout << "Data " << data << std::endl;
                    rapidjson::Document document;
                    document.Parse(data);
                    std::cout << "Receiving messages!" << std::endl;
                    std::cout << " Ack count: " << document["ack_count"].GetInt() << std::endl;
                    recordClientLatency(document["ack_count"].GetUint64());
                }
            }
            receive_socket.close();
            std::cout << "Ack count: " << ackCount() << std::endl;  
            std::cout << "End of the receive socket thread" << std::endl;
        }

        void runExperiments() {
            std::cout << "Run experiments! " << std::endl;
            for (std::string exp : experiment_names) {
                std::cout << "Now running experiment: " << exp << std::endl;
                auto sendThread = std::thread(&AlgorandClient::sendTransaction, this, exp);
                auto receiveThread = std::thread(&AlgorandClient::receiveServerResponse, this, exp);
                sendThread.join();
                receiveThread.join();
                std::cout << "Are we getting here?" << std::endl;
                reportPerformanceMetrics(exp);
            }
        }

        /*bool checkCorrectness(){
            // check for correctness of received data
        }*/

	    void reportPerformanceMetrics(std::string experiment_name) {
            // Graph the metrics
            // stats[experiment_name].printOutAllResults();
        }
};

int main(int argc, char* argv[]) {
   /*if (argc < 3) {
       std::cout << "Too few args! Format: ./client <send_url> <receive_url>" << std::endl;
       return -1;
   }*/
   std::string path_to_config = "/proj/ove-PG0/murray/BFT-RSM/Code/client_interface/client.json"; //TODO
   std::vector<std::string> experiment_names = {"simple"}; // TODO
   AlgorandClient* algoCli = new AlgorandClient(path_to_config, experiment_names);
   algoCli->runExperiments();
   return 0;
}
