// Original Source code from Reginald Frank

package ipc

import (
	"bufio"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"os"
	"os/signal"
	"syscall"

	"go.etcd.io/etcd/server/v3/etcdserver/scrooge"
)

const O_NONBLOCK = syscall.O_NONBLOCK

// Creates a pipe at pipePath, deletes a previous file with the same name if it exists
func CreatePipe(pipePath string) error {
	if doesFileExist(pipePath) {
		err := os.Remove(pipePath)
		if err != nil {
			return err
		}
	}
	fmt.Printf("Pipe created at path: %s\n", pipePath)
	return syscall.Mkfifo(pipePath, 0777)
}

// Blocking call to output the data pipePath into pipeData
// Reads data from the pipe in format [size uint64, bytes []byte] where len(bytes) == size and (pipeData <- bytes)
// All data is in little endian format
func OpenPipeReader(pipePath string) (*os.File, error) {
	if !doesFileExist(pipePath) {
		return nil, errors.New("file doesn't exist")
	}

	setupCloseHandler()
	pipe, fileErr := os.OpenFile(pipePath, os.O_RDONLY, 0777)
	if fileErr != nil {
		fmt.Println("Cannot open pipe for reading:", fileErr)
	}

	return pipe, nil
}

func UsePipeReader(openReadPipe *os.File) {
	reader := bufio.NewReader(openReadPipe)

	for {
		const numSizeBytes = 64 / 8

		readSizeBytes := loggedRead(reader, numSizeBytes)
		if readSizeBytes == nil {
			continue
		}

		readSize := binary.LittleEndian.Uint64(readSizeBytes[:])

		readData := loggedRead(reader, readSize)

		if readData == nil {
			fmt.Println("No Data Read!")
		}

		// unmarshal readData into ScroogeRequest, and print seq num
		// var req scrooge.ScroogeRequest

		// proto.Unmarshal(readData, &req)

		// payload := string(req.GetSendMessageRequest().GetContent().GetMessageContent())
		// seqNum := req.GetSendMessageRequest().GetContent().GetSequenceNumber()

		// fmt.Println("Receive Sequence number: ", seqNum)
		// fmt.Println("Receive Payload: ", payload)

	}
}

// Blocking call that will continously write the data pipeInput into pipePath
// Byte strings will be written as [size uint64, bytes []byte] where len(bytes) == size and (bytes := <-pipeInput)
// All data is in little endian format
func OpenPipeWriter(pipePath string) (*os.File, error) {
	if !doesFileExist(pipePath) {
		return nil, errors.New("file doesn't exist")
	}

	setupCloseHandler()
	pipe, fileErr := os.OpenFile(pipePath, os.O_WRONLY, 0777)
	if fileErr != nil {
		fmt.Println("Cannot open pipe for writing:", fileErr)
	}

	/*fmt.Println("returning writer, so pipe is closing!")*/
	return pipe, nil
}

func UsePipeWriter(openWritePipe *os.File, requestBytes []byte) error {
	var writeSizeBytes [8]byte
	binary.LittleEndian.PutUint64(writeSizeBytes[:], uint64(len(requestBytes)))

	writer := bufio.NewWriter(openWritePipe)

	// fmt.Println("Start logged write sizeBytes and requestBytes")
	loggedWrite(writer, writeSizeBytes[:])
	loggedWrite(writer, requestBytes)

	writer.Flush()

	return nil
}

func loggedRead(reader io.Reader, numBytes uint64) []byte {
	readData := make([]byte, numBytes)

	bytesRead, readErr := io.ReadFull(reader, readData)

	if readErr != nil {
		fmt.Println("Pipe Reading Error: ", readErr, "[Desired Read size = ", numBytes, " Actually read size = ", bytesRead, "]")
		return nil
	} else {
		return readData
	}
}

func loggedWrite(writer io.Writer, data []byte) {
	bytesWritten, writeErr := writer.Write(data)
	if writeErr != nil {
		fmt.Println("Pipe Writing Error: ", writeErr, "[Desired Write size = ", len(data), " Actually written size = ", bytesWritten, "]")
		os.Exit(1)
	}
}

// SetupCloseHandler creates a 'listener' on a new goroutine which will notify the
// program if it receives an interrupt from the OS. We then handle this by calling
// our clean up procedure and exiting the program.
func setupCloseHandler() {
	c := make(chan os.Signal)
	signal.Notify(c, os.Interrupt, syscall.SIGTERM)
	go func() {
		<-c
		os.Exit(0)
	}()
}

func doesFileExist(fileName string) bool {
	_, error := os.Stat(fileName)

	return !os.IsNotExist(error)
}

// Helper functions that determine whether data read from Scrooge is a CommitAcknowledgment or an UnvalidatedCrossChainMessage.
func isCommitAcknowledgment(data scrooge.ScroogeTransfer) bool {
	if data.GetCommitAcknowledgment() != nil && data.GetUnvalidatedCrossChainMessage() == nil {
		return true
	}
	return false
}

func isUnvalidatedCrossChainMessage(data scrooge.ScroogeTransfer) bool {
	if data.GetCommitAcknowledgment() == nil && data.GetUnvalidatedCrossChainMessage() != nil {
		return true
	}
	return false
}
