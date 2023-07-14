package etcdserver

import (
	"fmt"
	"os"

	"go.etcd.io/etcd/server/v3/etcdserver/ipc-pkg"
	"go.etcd.io/etcd/server/v3/etcdserver/scrooge"
	"google.golang.org/protobuf/proto"
)

// "bytes"
// "encoding/gob"
// "encoding/json"
// "fmt"
// "log"
// "sync"

const (
	// pipes used by Scrooge
	path_to_ipipe = "/tmp/scrooge-input"

	path_to_opipe = "/tmp/scrooge-input"
)

func (s *EtcdServer) CreatePipe() {
	err := ipc.CreatePipe(path_to_ipipe)
	if err != nil {
		fmt.Println("Unable to open output pipe: ", err)
	}
}

func (s *EtcdServer) ReadScrooge() {
	var err error

	// create read pipe
	// err = ipc.CreatePipe(path_to_opipe)
	// if err != nil {
	// 	fmt.Println("Unable to open output pipe: ", err)
	// }

	// open pipe reader
	openReadPipe, err := ipc.OpenPipeReader(path_to_opipe)
	if err != nil {
		fmt.Println("Unable to open pipe reader: ", err)
	}
	defer openReadPipe.Close()

	// continuously receives messages from Scrooge
	ipc.UsePipeReader(openReadPipe)
}

func (s *EtcdServer) WriteScrooge() {
	var err error

	// lg := s.Logger()

	// create write pipe
	// err = ipc.CreatePipe(path_to_ipipe)
	// if err != nil {
	// 	fmt.Println("Unable to open input pipe: ", err)
	// }

	// open pipe writer
	openWritePipe, err := ipc.OpenPipeWriter(path_to_ipipe)
	if err != nil {
		fmt.Println("Unable to open pipe writer: ", err)
	}
	defer openWritePipe.Close()

	// continously receives data of applied normal entries and subsequently writes the data to Scrooge
	for data := range s.WriteScroogeC {
		// lg.Info("######## Received data from apply(), Sending to Scrooge ########",
		// 	zap.String("data", string(data)),
		// 	zap.Uint64("sequence number", 0))

		// _ = data
		sendScrooge(data, 0, openWritePipe)
		// seqNumber++
	}
}

func sendScrooge(payload []byte, seqNumber uint64, openWritePipe *os.File) {
	request := &scrooge.ScroogeRequest{
		Request: &scrooge.ScroogeRequest_SendMessageRequest{
			SendMessageRequest: &scrooge.SendMessageRequest{
				Content: &scrooge.CrossChainMessageData{
					MessageContent: payload,
					SequenceNumber: seqNumber,
				},
				ValidityProof: []byte("substitute valididty proof"),
			},
		},
	}

	var err error
	requestBytes, err := proto.Marshal(request)

	if err == nil {
		err = ipc.UsePipeWriter(openWritePipe, requestBytes)
		if err != nil {
			print("Unable to use pipe writer", err)
		}
	}
}
