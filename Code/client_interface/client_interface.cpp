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
#include "statisticstracker.cpp"

#include <../include/rapidjson/document.h>
#include <../include/rapidjson/writer.h>
#include <../include/rapidjson/stringbuffer.h>

#include "nng/nng.h"
#include <nng/transport/tcp/tcp.h>
#include <nng/protocol/pipeline0/pull.h>
#include <nng/protocol/pipeline0/push.h>

using namespace boost::asio;
using ip::tcp;

static constexpr uint64_t BASE_VALUE = 100;

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
        /* Setup client connection with the server */
        virtual bool registerClient() = 0;
        /* Send transaction to server */
        virtual void sendTransaction(std::string experiment_name) = 0;
        /* Waits for the Server response */
        virtual void receiveServerResponse(std::string experiment_name) = 0;

        /******************** Variables of the Class **********************/
        std::map<std::string, std::vector<Transaction>> transactions_list;
        std::string path_to_config;
        Json::Value config;
        std::vector<std::string> experiment_names; 
        std::map<std::string, StatisticsInterpreter> stats;
        
};

class AlgorandClient : public virtual Client {
    private:
        boost::asio::io_service send_io_service;
        boost::asio::io_service receive_io_service;
        tcp::socket send_socket;
        tcp::socket receive_socket;
        std::string send_ip;
        std::string receive_ip;
        uint64_t send_port;
        uint64_t receive_port;

        int64_t internal_seq_id;
        uint64_t final_max_seq_id;

        uint64_t ack_counter = 0;

    public:
        /* 
         * Sets up Client connection to the primary server machine and loads in
         * the appropriate traces into the clients' list of transactions 
         */
        AlgorandClient(std::string config, 
                       std::vector<std::string> experiments)
        : send_socket(send_io_service), receive_socket(receive_io_service) {
            this->path_to_config = config;
            this->experiment_names = experiments;
            // this->send_io_service = send_io_service;
            // this->receive_io_service = receive_io_service;
            
            std::vector<Transaction> transactions_list = {};
            internal_seq_id = 0;
            
            loadTrace();
            registerClient();
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
            this->send_ip = config["general"]["send_ip"].asString();
            this->receive_ip = config["general"]["receive_ip"].asString();
            this->send_port = config["general"]["send_port"].asUInt64();
            this->receive_port = config["general"]["receive_port"].asUInt64();
            for (std::string exp : experiment_names) {
                transactions_list.insert({exp, {}});
                std::string trace_path = config[exp]["input_file"].asString();
                //std::cout << "Trace path: " << trace_path.length() << " and experiment name: " << exp << std::endl;
                if (trace_path.length() > 0) 
                {
                    std::ifstream trace_stream;
                    trace_stream.open(trace_path);
                    //std::cout << "Trace stream is open? " << trace_stream.is_open() << std::endl;
                    if (trace_stream.is_open()) {
                        std::string trace;
                        //std::cout << "Trace stream is open! " << std::endl;
                        while (std::getline(trace_stream, trace)) {
                            Transaction txn = Transaction{internal_seq_id, trace};
                            //std::cout << "Trace value: " << trace << std::endl;
                            transactions_list[exp].push_back(txn);
                            if (internal_seq_id >= 0)
                                internal_seq_id += 1;
                        }
                    }
                    std::cout << "Done reading from the trace file! The internal seq id is: " << internal_seq_id << std::endl;
                    trace_stream.close();
                    // then load trace file in
                } else {
                    const auto rounds = config[exp]["rounds"].asUInt64();
                    const auto payload_offset = config[exp]["payload"].asUInt64();
                    for (size_t i = 0; i < rounds; i++) {
                        Transaction txn = Transaction{internal_seq_id, std::string(payload_offset*BASE_VALUE, 'L')};
                        transactions_list[exp].push_back(txn);
                        internal_seq_id += 1;
                    }
                }
                final_max_seq_id = internal_seq_id;
                internal_seq_id = -1;
                Transaction final_txn = Transaction{internal_seq_id, "end"};
                transactions_list[exp].push_back(final_txn);
            }
            configFile.close();
        }

        /* Get list of transactions */
        std::vector<Transaction> getTransactionList(std::string experiment_name) {
            return transactions_list[experiment_name];
        }

        /* Setup client connection with the server */
        bool registerClient() {
            /* Register sending URL */
            

            
            return true;
        }

        /* Send transaction to server */
        void sendTransaction(std::string experiment_name) {
            // standard send
            std::cout << "Send transaction " << send_ip << " port: " << send_port << std::endl;
            send_socket.connect(tcp::endpoint(boost::asio::ip::address::from_string(send_ip), send_port));
            std::cout << "Execute send transaction thread with " << transactions_list[experiment_name].size() << std::endl;
            for (Transaction txn : transactions_list[experiment_name]) {
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
                boost::asio::write(send_socket, boost::asio::buffer(json_str), error );
                if( !error ) {
                    std::cout << "Client sent hello message!" << std::endl;
                } else {
                    std::cout << "send failed: " << error.message() << std::endl;
                }
                std::cout << "Transaction sent: " << txn.seq_id << std::endl;
                stats[experiment_name].startTimer(txn.seq_id);
            }
            send_socket.close();
        }

        /* Waits for the Server response */
        void receiveServerResponse(std::string experiment_name) {
            /* Register receiving URL */
            boost::asio::ip::tcp::acceptor acceptor(receive_io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), receive_port));
            acceptor.accept(receive_socket);
            std::cout << "Accepted connection from " << receive_socket.remote_endpoint().address().to_string() << std::endl;
            while (true) {
                // TODO: Timeout of some sort to actually close the socket
                // getting response from server
                std::cout << "Here0 " << std::endl;
                boost::system::error_code error;
                boost::asio::streambuf receive_buffer;
                boost::asio::read(receive_socket, receive_buffer, boost::asio::transfer_all(), error);
                std::cout << "Here 1 " << std::endl;
                if(error && error != boost::asio::error::eof ) {
                    std::cout << "receive failed: " << error.message() << std::endl;
                }
                else {
                    const char* data = boost::asio::buffer_cast<const char*>(receive_buffer.data());
                    std::cout << "Here 2" << std::endl;
                    std::cout << data << std::endl;
                    rapidjson::Document document;
                    document.Parse(data);
                    std::cout << "Here 3 " << std::endl;
                    std::cout << " Ack count: " << document["ack_count"].GetInt() << std::endl;
                    stats[experiment_name].recordLatency((size_t)document["ack_count"].GetInt());
                    if (final_max_seq_id <= document["ack_count"].GetInt()) {
                        std::cout << "Here? " << std::endl;
                        break;
                    }
                }
            }
            receive_socket.close();
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
            stats[experiment_name].printOutAllResults();
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
