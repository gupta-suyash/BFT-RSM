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
    nodeId := flag.Int("id", -1, "id of the current node")
    networkId := flag.Int("network", -1, "network of the current node")
    flag.Parse()

    if *nodeId == -1 {
        glog.Info("Must input -id nodeId, exiting...")
        os.Exit(1)
    }

    if (*networkId != 0) && (*networkId != 1) {
        glog.Info("Must input -network [0,1], exiting...")

        os.Exit(1)
    }

    const kScroogeTransferPipe string = "/tmp/scrooge-input"
    const kScroogeRequestPipe string = "/tmp/client-input"
    const kNetworkZeroConfigurationPath string = "../configuration/networkZero.txt"
    const kNetworkOneConfigurationPath string = "../configuration/networkOne.txt"

    ipc.CreatePipe(kScroogeRequestPipe)

    scroogeTransfers := make(chan *scrooge.ScroogeTransfer, 5)
    scroogeRequests := make(chan *scrooge.ScroogeRequest, 5)

    go openScroogeTransferReader(kScroogeTransferPipe, scroogeTransfers)
    go openScroogeRequestWriter(kScroogeRequestPipe, scroogeRequests)
    go handleScroogeTransfers(scroogeTransfers, scroogeRequests)

    var ownClusterConfig, otherClusterConfig scrooge.ClusterConfiguration

    if *networkId == 0 {
        ownClusterConfig = parseClusterConfiguration(kNetworkZeroConfigurationPath, nodeId, "NetworkZero")
        otherClusterConfig = parseClusterConfiguration(kNetworkOneConfigurationPath, nil, "NetworkOne")

    } else {
        ownClusterConfig = parseClusterConfiguration(kNetworkZeroConfigurationPath, nil, "NetworkZero")
        otherClusterConfig = parseClusterConfiguration(kNetworkZeroConfigurationPath, nodeId, "NetworkOne")
    }

    scroogeRequests <- &scrooge.ScroogeRequest{
        Request: &scrooge.ScroogeRequest_SetClusterConfigRequest{
            SetClusterConfigRequest: &scrooge.SetClusterConfigRequest{
                ClusterConfiguration: &ownClusterConfig,
            },
        },
    }

    scroogeRequests <- &scrooge.ScroogeRequest{
        Request: &scrooge.ScroogeRequest_SetClusterConfigRequest{
            SetClusterConfigRequest: &scrooge.SetClusterConfigRequest{
                ClusterConfiguration: &otherClusterConfig,
            },
        },
    }

    messageRequests := make(chan *scrooge.SendMessageRequest)
    go generateCrossChainMessages(messageRequests, &ownClusterConfig, &otherClusterConfig)

    for messageRequest := range messageRequests {
        glog.Info("Sending message request with sequence number", messageRequest.Content.GetSequenceNumber())
        scroogeRequests <- &scrooge.ScroogeRequest{
            Request: &scrooge.ScroogeRequest_SendMessageRequest{
                SendMessageRequest: messageRequest,
            },
        }
    }
}

func generateCrossChainMessages(messageRequests chan<- *scrooge.SendMessageRequest, ownClusterConfig, otherClusterConfig *scrooge.ClusterConfiguration) {
    for i := uint64(0); true; i++ {
        messageRequests <- &scrooge.SendMessageRequest{
            ClusterIdentifier: otherClusterConfig.GetClusterIdentifier(),
            Content: &scrooge.CrossChainMessageData{
                MessageContent: []byte(string(i)),
                SequenceNumber: i,
            },
            ValidityProof: []byte("lol trust me on this one"),
        }
    }
}

// Will send the message to other nodes who will validate it before accepting it
func broadcastCrossChainMessage(unvalidatedCrossChainMessage *scrooge.UnvalidatedCrossChainMessage) {}

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
                broadcastCrossChainMessage(unvalidatedCrossChainMessage)
            }
        default:
            glog.Error("Unknown Scrooge Transfer Type: ", transferType)
        }
    }
}

// Blocking call that will read and parse scrooge messages found at path pipePath
// All output will be put into scroogeData
func openScroogeTransferReader(pipePath string, scroogeTransfers chan<- *scrooge.ScroogeTransfer) error {
    rawData := make(chan []byte, 5)
    go ipc.OpenPipeReader(pipePath, rawData)

    for data := range rawData {
        var scroogeTransfer scrooge.ScroogeTransfer
        proto.Unmarshal(data, &scroogeTransfer)
        scroogeTransfers <- &scroogeTransfer
    }
    return nil
}

// Blocking call that will searilize and write all scroogeRequests in scroogeData to pipePath
func openScroogeRequestWriter(pipePath string, scroogeRequests <-chan *scrooge.ScroogeRequest) error {
    rawData := make(chan []byte, 5)
    go ipc.OpenPipeWriter(pipePath, rawData)

    for request := range scroogeRequests {
        requestBytes, err := proto.Marshal(request)

        if err == nil {
            rawData <- requestBytes
        } else {
            glog.Error("Error searilizing scroogeRequest", err)
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
