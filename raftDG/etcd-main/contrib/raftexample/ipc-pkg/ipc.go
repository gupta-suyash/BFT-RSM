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
func OpenPipeReader(pipePath string) (*bufio.Reader, *os.File, error) {
	if !doesFileExist(pipePath) {
		return bufio.NewReader(nil), nil, errors.New("file doesn't exist")
	}

	setupCloseHandler()
	pipe, fileErr := os.OpenFile(pipePath, os.O_RDONLY, 0777)
	if fileErr != nil {
		fmt.Println("Cannot open pipe for reading:", fileErr)
	}

	reader := bufio.NewReader(pipe)

	/*fmt.Println("returning writer, so pipe is closing!")*/
	return reader, pipe, nil
	/*if !doesFileExist(pipePath) {
		fmt.Println("File doesn't exist")
	}

	setupCloseHandler()

	pipe, fileErr := os.OpenFile(pipePath, os.O_RDONLY, 0777)
	if fileErr != nil {
		fmt.Println("Cannot open pipe for reading:", fileErr)
	}
	defer pipe.Close()
	defer close(pipeData)
	reader := bufio.NewReader(pipe)

	for {
		const numSizeBytes = 64 / 8

		readSizeBytes := loggedRead(reader, numSizeBytes)
		if readSizeBytes == nil {

			break
		}
		readSize := binary.LittleEndian.Uint64(readSizeBytes[:])

		readData := loggedRead(reader, readSize)
		if readData == nil {
			break
		}
		pipeData <- readData
	}*/
}

func UsePipeReader(reader *bufio.Reader) {
	fmt.Println("Begin reading from Scrooge")
	const numSizeBytes = 64 / 8

	fmt.Println("Start logged read sizeBytes and data")
	readSizeBytes := loggedRead(reader, numSizeBytes)
	if readSizeBytes == nil {
		fmt.Println("Error: no size bytes")
	}
	readSize := binary.LittleEndian.Uint64(readSizeBytes[:])

	readData := loggedRead(reader, readSize)
	if readData == nil {
		fmt.Println("Error: no data bytes")
	}
	fmt.Println("Read Data: ", readData)
}

// Blocking call that will continously write the data pipeInput into pipePath
// Byte strings will be written as [size uint64, bytes []byte] where len(bytes) == size and (bytes := <-pipeInput)
// All data is in little endian format
func OpenPipeWriter(pipePath string) (*bufio.Writer, *os.File, error) {
	if !doesFileExist(pipePath) {
		return bufio.NewWriter(nil), nil, errors.New("file doesn't exist")
	}

	setupCloseHandler()
	pipe, fileErr := os.OpenFile(pipePath, os.O_WRONLY, 0777)
	if fileErr != nil {
		fmt.Println("Cannot open pipe for writing:", fileErr)
	}

	writer := bufio.NewWriter(pipe)

	/*fmt.Println("returning writer, so pipe is closing!")*/
	return writer, pipe, nil

	/*go func(pipeChannel <-chan []byte) (bufio.Writer){
		setupCloseHandler() // TODO

		pipe, fileErr := os.OpenFile(pipePath, os.O_WRONLY, 0777)
		if fileErr != nil {
			fmt.Println("Cannot open pipe for writing:", fileErr)
		}
		defer pipe.Close()

		writer := bufio.NewWriter(pipe)
		return writer

		for data := range pipeInput {
			var writeSizeBytes [8]byte
			binary.LittleEndian.PutUint64(writeSizeBytes[:], uint64(len(data)))

			loggedWrite(writer, writeSizeBytes[:])
			loggedWrite(writer, data)
			writer.Flush()
		}

	}(pipeInput)*/

	//return *bufio.NewWriter(nil), nil
}

func UsePipeWriter(writer *bufio.Writer, requestBytes []byte) error {
	fmt.Println("request bytes: ", requestBytes)

	var writeSizeBytes [8]byte
	binary.LittleEndian.PutUint64(writeSizeBytes[:], uint64(len(requestBytes)))

	fmt.Println("Start logged write sizeBytes and requestBytes")
	loggedWrite(writer, writeSizeBytes[:])
	loggedWrite(writer, requestBytes)

	writer.Flush()
	fmt.Println("After write flush")

	return nil
}

func loggedRead(reader io.Reader, numBytes uint64) []byte {
	readData := make([]byte, numBytes)

	bytesRead, readErr := io.ReadFull(reader, readData)
	fmt.Println("start read data: ", readData, " bytes read: ", bytesRead)
	// fmt.Println("After logged read")

	if readErr != nil {
		fmt.Println("Pipe Writing Error: ", readErr, "[Desired Write size = ", numBytes, " Actually written size = ", bytesRead, "]")
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
