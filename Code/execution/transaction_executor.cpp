/*
 * Copyright (c) 2019-2022 ExpoLab, UC Davis
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include "execution/transaction_executor.h"
#include "ipc.h"

#include "scrooge_message.pb.h"
#include "scrooge_networking.pb.h"
#include "scrooge_request.pb.h"
#include "scrooge_transfer.pb.h"

#include <glog/logging.h>
#include <chrono>
#include <ctime>

namespace resdb {

TransactionExecutor::TransactionExecutor(
    const ResDBConfig& config, PostExecuteFunc post_exec_func,
    SystemInfo* system_info,
    std::unique_ptr<TransactionExecutorImpl> executor_impl,
		PostValidateFunc post_valid_func)
    : config_(config),
      post_exec_func_(post_exec_func),
      system_info_(system_info),
      executor_impl_(std::move(executor_impl)),
			post_valid_func_(post_valid_func), //@Suyash
      commit_queue_("order"),
      execute_queue_("execute"),
      scrooge_snd_queue_(20000), //@Suyash
      stop_(false) {
  global_stats_ = Stats::GetGlobalStats();
  ordering_thread_ = std::thread(&TransactionExecutor::OrderMessage, this);
  execute_thread_ = std::thread(&TransactionExecutor::ExecuteMessage, this);

  if (executor_impl_ && executor_impl_->IsOutOfOrder()) {
    execute_OOO_thread_ =
        std::thread(&TransactionExecutor::ExecuteMessageOutOfOrder, this);
  }

  //@Suyash
  tot_txn = 0;
  read_pipe_path_ = "/tmp/scrooge-output";
  write_pipe_path_ = "/tmp/scrooge-input";
  
  constexpr auto kFullPermissions = 0777;
  
  if(std::filesystem::exists(read_pipe_path_)) {
  	const auto eraseError = std::remove(read_pipe_path_.c_str()) != 0;
  	if(eraseError) {
  		LOG(INFO) << "Not Erased";
  	} else {
		LOG(INFO) << "Erased";
	}		
  } else {
		LOG(INFO) << "Does not exist";
  }
  
  const auto success = (mkfifo(read_pipe_path_.c_str(), kFullPermissions) == 0);
  if (not success) {
  	LOG(INFO) << "Cannot Create Read Pipe";
  } else {
	LOG(INFO) << "Created";
  }
  
  if(std::filesystem::exists(write_pipe_path_)) {
  	const auto eraseError = std::remove(write_pipe_path_.c_str()) != 0;
  	if(eraseError) {
  		LOG(INFO) << "Not Erased";
  	}				
  }
  
  const auto wsuccess = (mkfifo(write_pipe_path_.c_str(), kFullPermissions) == 0);
  if (not wsuccess) {
  	LOG(INFO) << "Cannot Create Write Pipe";
  }

  data_str = std::string(100, 'R');

  scrooge_snd_thread_ = std::thread(&TransactionExecutor::ScroogeSendMessage, this);
  scrooge_rcv_thread_ = std::thread(&TransactionExecutor::ScroogeRecvMessage, this);
}

TransactionExecutor::~TransactionExecutor() { Stop(); }

void TransactionExecutor::Stop() {
  stop_ = true;
  if (ordering_thread_.joinable()) {
    ordering_thread_.join();
  }
  if (execute_thread_.joinable()) {
    execute_thread_.join();
  }
  if (execute_OOO_thread_.joinable()) {
    execute_OOO_thread_.join();
  }
  //@Suyash
  if (scrooge_snd_thread_.joinable()) {
    scrooge_snd_thread_.join();
  } 
  if (scrooge_rcv_thread_.joinable()) {
    scrooge_rcv_thread_.join();
  }
}

void TransactionExecutor::SetPreExecuteFunc(PreExecuteFunc pre_exec_func) {
  pre_exec_func_ = pre_exec_func;
}

void TransactionExecutor::SetSeqUpdateNotifyFunc(SeqUpdateNotifyFunc func) {
  seq_update_notify_func_ = func;
}

bool TransactionExecutor::IsStop() { return stop_; }

uint64_t TransactionExecutor::GetMaxPendingExecutedSeq() {
  return next_execute_seq_ - 1;
}

int TransactionExecutor::Commit(std::unique_ptr<Request> message) {
  global_stats_->IncPendingExecute();
  if (executor_impl_ && executor_impl_->IsOutOfOrder()) {
    std::unique_ptr<Request> msg = std::make_unique<Request>(*message);
    execute_OOO_queue_.Push(std::move(message));
    commit_queue_.Push(std::move(msg));
  } else {
    commit_queue_.Push(std::move(message));
  }
  return 0;
}

void TransactionExecutor::AddNewData(std::unique_ptr<Request> message) {
  candidates_.insert(std::make_pair(message->seq(), std::move(message)));
}

std::unique_ptr<Request> TransactionExecutor::GetNextData() {
  if (candidates_.empty() || candidates_.begin()->first != next_execute_seq_) {
    return nullptr;
  }
  auto res = std::move(candidates_.begin()->second);
  if (pre_exec_func_) {
    pre_exec_func_(res.get());
  }
  candidates_.erase(candidates_.begin());
  return res;
}

void TransactionExecutor::OrderMessage() {
  while (!IsStop()) {
    auto message = commit_queue_.Pop();
    if (message != nullptr) {
      global_stats_->IncExecute();
      uint64_t seq = message->seq();
      if (next_execute_seq_ > seq) {
        LOG(INFO) << "request seq:" << seq << " has been executed"
                  << " next seq:" << next_execute_seq_;
        continue;
      }

      AddNewData(std::move(message));
    }

    while (!IsStop()) {
      std::unique_ptr<Request> message = GetNextData();
      if (message == nullptr) {
        break;
      }
      execute_queue_.Push(std::move(message));
      next_execute_seq_++;
      if (seq_update_notify_func_) {
        seq_update_notify_func_(next_execute_seq_);
      }
    }
  }
  return;
}

void TransactionExecutor::ExecuteMessage() {
  while (!IsStop()) {
    auto message = execute_queue_.Pop();
    if (message == nullptr) {
      continue;
    }
    bool need_execute = true;
    if (executor_impl_ && executor_impl_->IsOutOfOrder()) {
      need_execute = false;
    }
    Execute(std::move(message), need_execute);
  }
}

void TransactionExecutor::ExecuteMessageOutOfOrder() {
  while (!IsStop()) {
    auto message = execute_OOO_queue_.Pop();
    if (message == nullptr) {
      continue;
    }
    OnlyExecute(std::move(message));
  }
}

void TransactionExecutor::OnlyExecute(std::unique_ptr<Request> request) {
  // Execute the request on user size, then send the response back to the
  // client.
  BatchClientRequest batch_request;
  if (!batch_request.ParseFromString(request->data())) {
    LOG(ERROR) << "parse data fail";
  }
  batch_request.set_seq(request->seq());
  batch_request.set_hash(request->hash());
  batch_request.set_proxy_id(request->proxy_id());
  if (request->has_committed_certs()) {
    *batch_request.mutable_committed_certs() = request->committed_certs();
  }

  // LOG(INFO) << " get request batch size:"
  //          << batch_request.client_requests_size()<<" proxy
  //          id:"<<request->proxy_id();
  std::unique_ptr<BatchClientResponse> batch_response =
      std::make_unique<BatchClientResponse>();

  std::unique_ptr<BatchClientResponse> response;
  if (executor_impl_) {
    response = executor_impl_->ExecuteBatch(batch_request);
  }
}

void TransactionExecutor::Execute(std::unique_ptr<Request> request,
                                  bool need_execute) {
  // Execute the request on user size, then send the response back to the
  // client.
  BatchClientRequest batch_request;
  if (!batch_request.ParseFromString(request->data())) {
    LOG(ERROR) << "parse data fail";
  }

  batch_request.set_seq(request->seq());
  batch_request.set_hash(request->hash());
  batch_request.set_proxy_id(request->proxy_id());
  if (request->has_committed_certs()) {
    *batch_request.mutable_committed_certs() = request->committed_certs();
  }

  std::unique_ptr<BatchClientResponse> batch_response =
      std::make_unique<BatchClientResponse>();

  std::shared_ptr<BatchClientResponse> response;
  if (executor_impl_ && need_execute) {
    response = executor_impl_->ExecuteBatch(batch_request);
  }

  //@Suyash
  //tot_txn = tot_txn + batch_request.client_requests_size();
  //LOG(INFO) << "TOT: " << tot_txn;
  //
  // auto timeNow = std::chrono::system_clock::now();
  // auto passed = timeNow.time_since_epoch();
  // LOG(INFO) << "Executed: " << request->seq()-1 << "At: " << passed.count();

  
  global_stats_->IncTotalRequest(batch_request.client_requests_size());
  if (executor_impl_ == nullptr || executor_impl_->NeedResponse()) {
    if (response == nullptr) {
      response = std::make_unique<BatchClientResponse>();
    }

    response->set_createtime(batch_request.createtime());
    response->set_local_id(batch_request.local_id());

    if(request->seq() == 1) {
      LOG(INFO) << "Thread Id: " << gettid();
    }

    //@Suyash
    
    std::shared_ptr<BatchClientResponse> scrooge_response = response;
    scrooge_response->set_seq(request->seq());
    scrooge_response->set_current_view(request->current_view());
    

    /*
    while(!scrooge_snd_queue_.push(request->seq())) {
        std::this_thread::yield();
    }
    */

    
    //BatchClientResponse *scr_msg = scrooge_response.release();
    while(!scrooge_snd_queue_.push(scrooge_response)) {
        std::this_thread::yield();
    }
        

    post_exec_func_(std::move(request), std::move(response));
  }

  global_stats_->IncExecuteDone();
}


//@Suyash
void TransactionExecutor::ScroogeSendMessage() {
  LOG(INFO) << "ScroogeSendMessage: " << config_.GetSelfInfo().id();

  // Creating Pipe for transfer
  //std::string pipe_path = "/tmp/scrooge-input"; //+ std::to_string(config_.GetSelfInfo().id());
  std::ofstream scr_write_{write_pipe_path_, std::ios_base::binary};

  LOG(INFO) << "PIPE: " << write_pipe_path_;

  auto start = std::chrono::steady_clock::now();

  while (!IsStop()) {
    std::shared_ptr<BatchClientResponse> response;
    //BatchClientResponse *response;
    //uint64_t response;
    auto success = scrooge_snd_queue_.pop(response);
    if (!success) {
      std::this_thread::yield();
      continue;
    }

    // Response in string format.
    //std::string resp_str = "ResDB data";
    //response->SerializeToString(&resp_str);
    //LOG(INFO) << "RESPONSE: " << resp_str;
   
    scrooge::ScroogeRequest scroogeRequest;
    scrooge::SendMessageRequest sendMessageRequest;
    scrooge::CrossChainMessageData messageData;
    
    messageData.set_message_content(data_str);
    messageData.set_sequence_number(response->seq()-1);
    //messageData.set_sequence_number(response-1);
    //LOG(INFO) << "WRITE SEQ: " << messageData.sequence_number();
    
    std::string validityProof = "bytes of a proof";
    sendMessageRequest.set_validity_proof(validityProof);
    *sendMessageRequest.mutable_content() = std::move(messageData);
    
    *scroogeRequest.mutable_send_message_request() = std::move(sendMessageRequest);
    //delete response;
    
    const std::string scroogeRequestBytes = scroogeRequest.SerializeAsString();
    uint64_t messageSize = scroogeRequestBytes.size();
    uint8_t* const messageBytes = (uint8_t*) scroogeRequestBytes.c_str();
    
    /*
     * Test Purposes
     
    scrooge::ScroogeTransfer scroogeRequest;
    scrooge::CommitAcknowledgment messageData;
    messageData.set_sequence_number(response->seq());
    LOG(INFO) << "DUMP SEQ: " << messageData.sequence_number();
    
    *scroogeRequest.mutable_commit_acknowledgment() = std::move(messageData);
    const std::string scroogeRequestBytes = scroogeRequest.SerializeAsString();
    uint64_t messageSize = scroogeRequestBytes.size();
    uint8_t* const messageBytes = (uint8_t*) scroogeRequestBytes.c_str();
    */
    
    //LOG(INFO) << "BEFORE WRITE " << messageSize << sizeof(messageSize) ;
    
    // Writing size of message to pipe.
    scr_write_.write(reinterpret_cast<char *>(&messageSize), sizeof(messageSize));
    if (scr_write_.fail()) {
        LOG(INFO) << "WRITE Size fail" << scr_write_.eof();
    }
    
    //LOG(INFO) << "MESSAGE SIZE WRITTEN";
    
    // Writing message to pipe.
    scr_write_.write(reinterpret_cast<char *>(messageBytes), messageSize);
    if (scr_write_.fail()) {
        LOG(INFO) << "WRITE Message fail" << scr_write_.eof();
    }
    
    scr_write_.flush();

    //auto timeNow = std::chrono::system_clock::now();
    //auto passed = timeNow.time_since_epoch();
    //LOG(INFO) << "Wrote: " << response->seq()-1 << "At: " << passed.count();
    
    auto end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end-start;
    if(elapsed_seconds.count() > 125) {
	scr_write_.close();
    }

  }
}


void TransactionExecutor::ScroogeRecvMessage() {
  LOG(INFO) << "ScroogeRcvMessage: " << config_.GetSelfInfo().id();
  
  // Opening Pipe for transfer
  //std::ofstream scr_rtemp_{read_pipe_path_, std::ios_base::binary}; 

  std::ifstream scr_read_{read_pipe_path_, std::ios_base::binary};
  LOG(INFO) << "READ PIPE" << read_pipe_path_;

  uint64_t readSize, last_seq=1;
  
  while (!IsStop()) {
    // Attempt reading size.
    scr_read_.read(reinterpret_cast<char *>(&readSize), sizeof(readSize));
    if (scr_read_.fail()) {
      //LOG(INFO) << "READ Size fail" << scr_read_.eof();
      // When fails we reset the position.
      //scr_read_.seekg(fpos, scr_read_.beg);
      continue;
    }

    //LOG(INFO) << "READ SIZE: " << readSize;

    std::vector<uint8_t> message(readSize);
    scr_read_.read(reinterpret_cast<char *>(message.data()), message.size());
    if (scr_read_.fail()) {
      LOG(INFO) << "READ Message fail: "; //<< readSize << " Bytes: " << message.size();
      //scr_read_.seekg(fpos, scr_read_.beg);	
      continue;
    }

    scrooge::ScroogeTransfer newRequest;

    const auto isParseSuccessful = newRequest.ParseFromArray(message.data(), message.size());
    if (not isParseSuccessful) {
      LOG(INFO) << "BAD PARSE";
      continue;
    }
    //else{
    //  LOG(INFO) << "Parse successful: " << isParseSuccessful;
    //}

    switch (newRequest.transfer_case())
    {
      using request = scrooge::ScroogeTransfer::TransferCase;
      case request::kCommitAcknowledgment: {
	// Extracting commit_acknowledgment message.
	const auto commit_ack_msg = newRequest.commit_acknowledgment();
	uint64_t seq_no = commit_ack_msg.sequence_number()+1;
	//LOG(INFO) << "GOT: " << seq_no;

	// Time to validate and reply to client.
	while(last_seq < seq_no) {
	  //auto timeNow = std::chrono::system_clock::now();
  	  //auto passed = timeNow.time_since_epoch();
	  //LOG(INFO) << "Acked: " << last_seq << "At: " << passed.count();

	  post_valid_func_(last_seq);
	  last_seq++;
	}

	break;
      }
      case request::kUnvalidatedCrossChainMessage: {
      	LOG(INFO) << "Message received by other RSM";
      	break;
      }																					 
      default: {
      	LOG(INFO) << "UNKNOWN REQUEST TYPE: " << newRequest.transfer_case();
      }
    }
  }
	
}



}  // namespace resdb
