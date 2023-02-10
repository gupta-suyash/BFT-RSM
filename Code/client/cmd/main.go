package main

import (
	"flag"
	"os"

	"github.com/golang/glog"
	"github.com/gupta-suyash/BFT-RSM/client/pkg/ipc"
	"github.com/gupta-suyash/BFT-RSM/client/pkg/util"
	"github.com/gupta-suyash/BFT-RSM/client/scrooge"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/types/known/wrapperspb"
)

func main() {
	flag.Set("logtostderr", "true")
	nodeId := flag.Int("nodeId", -1, "Id of the current node")
	networkId := flag.Int("networkId", -1, "Network of the current node")
	numMessages := flag.Int("numMessages", 100, "The Number of messages to send")
	messageSize := flag.Int("messageSize", 100, "The size of messages to send")
	flag.Parse()

	if *nodeId == -1 {
		glog.Info("Must input -nodeId nodeId, exiting...")
		os.Exit(1)
	}

	if (*networkId != 0) && (*networkId != 1) {
		glog.Info("Must input -networkId [0,1], exiting...")
		os.Exit(1)
	}

	const kScroogeRequestPipe string = "/tmp/scrooge-input"
	const kScroogeTransferPipe string = "/tmp/client-input"

	ipc.CreatePipe(kScroogeTransferPipe)

	scroogeTransfers := make(chan *scrooge.ScroogeTransfer, 5)
	scroogeRequests := make(chan *scrooge.ScroogeRequest, 5)

	// Connect Go Channels with OS Pipes
	go openScroogeTransferReader(kScroogeTransferPipe, scroogeTransfers)
	go openScroogeRequestWriter(kScroogeRequestPipe, scroogeRequests)

	// Write/Read data to/from Go Channels
	go handleScroogeTransfers(scroogeTransfers, scroogeRequests)
	go writeMessageRequests(scroogeRequests, *messageSize, *numMessages)

	for {
		// spin forever. Should honestly stop after scroogeTransfers has done "enough" work
		// Would application to know what "enough" work is though. so do application specific stuff probably
	}
}

func writeMessageRequests(scroogeRequests chan<- *scrooge.ScroogeRequest, messageSize, numMessages int) {
	for sequenceNumber := 0; sequenceNumber < numMessages; sequenceNumber++ {
		glog.Info("Sending message request with sequence number ", sequenceNumber)
		scroogeRequests <- &scrooge.ScroogeRequest{
			Request: &scrooge.ScroogeRequest_SendMessageRequest{
				SendMessageRequest: &scrooge.SendMessageRequest{
					Content: &scrooge.CrossChainMessageData{
						MessageContent: make([]byte, messageSize),
						SequenceNumber: uint64(sequenceNumber),
					},
					ValidityProof: []byte("lol trust me on this one"),
				},
			},
		}
	}
}

// Will incorperate validating signatures but for now is just a stub
func verifyCrossChainMessage(unvalidatedCrossChainMessage *scrooge.UnvalidatedCrossChainMessage) bool {
	return true
}

// Blocking call that will send requests in response to scrooge transfers
func handleScroogeTransfers(scroogeTransferInput <-chan *scrooge.ScroogeTransfer, scroogeRequestOuput chan<- *scrooge.ScroogeRequest) {
	for scroogeTransfer := range scroogeTransferInput {
		switch transferType := scroogeTransfer.Transfer.(type) {
		case *scrooge.ScroogeTransfer_UnvalidatedCrossChainMessage:
			unvalidatedCrossChainMessage := scroogeTransfer.GetUnvalidatedCrossChainMessage()
			isValid := verifyCrossChainMessage(unvalidatedCrossChainMessage)

			scroogeRequestOuput <- &scrooge.ScroogeRequest{
				Request: &scrooge.ScroogeRequest_AuthenticateMessageRequest{
					AuthenticateMessageRequest: &scrooge.AuthenticateMessageRequest{
						MessageIdentifier: unvalidatedCrossChainMessage.GetMessageIdentifier(),
						AcceptMessage:     isValid,
					},
				},
			}

			if isValid {
				sequenceNumber := unvalidatedCrossChainMessage.MessageIdentifier.SequenceNumber
				senderId := unvalidatedCrossChainMessage.MessageIdentifier.SenderId
				glog.Info("Just validated message ", sequenceNumber, " from node ", senderId)
			}
		default:
			glog.Error("Unknown Scrooge Transfer Type: ", transferType)
		}
	}

	close(scroogeRequestOuput)
}

// Blocking call that will read and parse scrooge messages found at path pipePath
// All output will be put into scroogeTransfers
func openScroogeTransferReader(pipePath string, scroogeTransfers chan<- *scrooge.ScroogeTransfer) error {
	rawData := make(chan []byte, 5)

	err := ipc.OpenPipeReader(pipePath, rawData)
	if err != nil {
		return err
	}

	for data := range rawData {
		var scroogeTransfer scrooge.ScroogeTransfer
		err := proto.Unmarshal(data, &scroogeTransfer)

		if err == nil {
			scroogeTransfers <- &scroogeTransfer
		} else {
			glog.Error("Error deserializing ScroogeTransfer")
		}
	}

	close(scroogeTransfers)
	return nil
}

// Blocking call that will searilize and write all scroogeRequests in scroogeData to pipePath
func openScroogeRequestWriter(pipePath string, scroogeRequests <-chan *scrooge.ScroogeRequest) error {
	rawData := make(chan []byte, 5)

	err := ipc.OpenPipeWriter(pipePath, rawData)
	if err != nil {
		return err
	}

	for request := range scroogeRequests {
		requestBytes, err := proto.Marshal(request)

		if err == nil {
			rawData <- requestBytes
		} else {
			glog.Error("Error searilizing ScroogeRequest", err)
		}
	}
	return nil
}

// Reads the configuration in a file to a proto
func parseClusterConfiguration(path string, selfId *int, networkIdentifier string) scrooge.ClusterConfiguration {
	file, err := os.Open(path)
	if err != nil {
		glog.Error("Cannot open file ", path, " to read cluster configuration:", err)
		os.Exit(1)
		return scrooge.ClusterConfiguration{}
	}

	clusterIdentifier := scrooge.ClusterIdentifier{NetworkIdentifier: &wrapperspb.StringValue{Value: networkIdentifier}}
	nodeConfigurations := util.ReadNodeConfigurations(file)
	var selfConfiguration *scrooge.NodeConfiguration = nil

	if selfId != nil {
		selfConfiguration = nodeConfigurations[*selfId]
	}

	return scrooge.ClusterConfiguration{
		ClusterIdentifier:  &clusterIdentifier,
		NodeConfigurations: nodeConfigurations,
		SelfConfiguration:  selfConfiguration,
	}
}
