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

// Dummy function to isolate errors
func sayHi() {
	fmt.Println("hello world")
}

// Creates a pipe at pipePath, deletes a previous file with the same name if it exists
func CreatePipe(pipePath string) error {
	if doesFileExist(pipePath) {
		err := os.Remove(pipePath)
		if err != nil {
			return err
		}
	}
	fmt.Println("Pipe created!")
	return syscall.Mkfifo(pipePath, 0777)
}

// Blocking call to output the data pipePath into pipeData
// Reads data from the pipe in format [size uint64, bytes []byte] where len(bytes) == size and (pipeData <- bytes)
// All data is in little endian format
func OpenPipeReader(pipePath string, pipeData chan<- []byte) {
	if !doesFileExist(pipePath) {
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
	}
}

// Blocking call that will continously write the data pipeInput into pipePath
// Byte strings will be written as [size uint64, bytes []byte] where len(bytes) == size and (bytes := <-pipeInput)
// All data is in little endian format
func OpenPipeWriter(pipePath string, pipeInput <-chan []byte) (*bufio.Writer, error) {
	if !doesFileExist(pipePath) {
		return bufio.NewWriter(nil), errors.New("FILE DOESN'T EXIST")
	}

	pipe, fileErr := os.OpenFile(pipePath, os.O_WRONLY, 0777)
	if fileErr != nil {
		fmt.Println("Cannot open pipe for writing:", fileErr)
	}
	defer pipe.Close()

	return bufio.NewWriter(pipe), nil

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

func UsePipeWriter(writer *bufio.Writer, pipeInput <-chan []byte) error {
	for data := range pipeInput {
		var writeSizeBytes [8]byte
		binary.LittleEndian.PutUint64(writeSizeBytes[:], uint64(len(data)))

		loggedWrite(writer, writeSizeBytes[:])
		loggedWrite(writer, data)
		writer.Flush()
	}
	return nil
}

func loggedRead(reader io.Reader, numBytes uint64) []byte {
	readData := make([]byte, numBytes)
	bytesRead, readErr := io.ReadFull(reader, readData)

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
		os.Exit(1)
		fmt.Println("Pipe Writing Error: ", writeErr, "[Desired Write size = ", len(data), " Actually written size = ", bytesWritten, "]")
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
