package main

import (
	"flag"

	"github.com/golang/glog"
	"github.com/gupta-suyash/BFT-RSM/client/pkg/ipc"
)

func main() {
	flag.Set("logtostderr", "true")
	flag.Parse()
	pipePath := "/tmp/gotest"

	ipc.CreatePipe(pipePath)

	writeData := make(chan []byte, 1)

	readData, err := ipc.OpenPipeReader(pipePath)
	if err != nil {
		glog.Infoln(err)
	}

	err = ipc.OpenPipeWriter(pipePath, writeData)
	if err != nil {
		glog.Infoln(err)
	}

	data := []string{"Hello", "World", "What", "A", "Beautiful", "Day"}

	for _, word := range data {
		writeData <- []byte(word)

		glog.Infoln("Wrote [", word, "] To Pipe")

		readWord := <-readData

		glog.Infoln("Read [", string(readWord), "] From Pipe")
	}

	glog.Info("Finished")
}
