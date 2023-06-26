// Copyright 2015 The etcd Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package main

import (
	"flag"
	"strings"

	"go.etcd.io/etcd/v3/contrib/raftexample/ipc-pkg"
	"go.etcd.io/raft/v3/raftpb"
)

const (
	path_to_pipe = "/tmp/scrooge-input"
)

func main() {
	cluster := flag.String("cluster", "http://127.0.0.1:9021", "comma separated cluster peers")
	id := flag.Int("id", 1, "node ID")
	kvport := flag.Int("port", 9121, "key-value server port")
	join := flag.Bool("join", false, "join an existing cluster")
	flag.Parse()

	proposeC := make(chan string)
	defer close(proposeC)
	confChangeC := make(chan raftpb.ConfChange)
	defer close(confChangeC)

	// raft provides a commit stream for the proposals from the http api
	var kvs *kvstore
	getSnapshot := func() ([]byte, error) { return kvs.getSnapshot() }
	commitC, errorC, snapshotterReady := newRaftNode(*id, strings.Split(*cluster, ","), *join, getSnapshot, proposeC, confChangeC)

	rawData := make(chan []byte)
	rdtest := make(chan []byte, 1)

	byteArray := []byte{97, 98, 99, 100, 101, 102}
	rdtest <- byteArray

	var err error
	print("passes error", "\n")

	err = ipc.CreatePipe(path_to_pipe)
	if err != nil {
		print("Unable to open pipe: %v", err, "\n")
	}
	print("Pipe made", "\n")

	writer, err := ipc.OpenPipeWriter(path_to_pipe, rdtest)
	if err != nil {
		print("Unable to open pipe writer: %v", err, "\n")
	}
	print("passed the openpipewriter ", "\n")

	for data := range rdtest {
		print(data, "\n")
	}

	kvs.FetchWriter(writer)

	kvs = newKVStore(<-snapshotterReady, rawData, proposeC, commitC, errorC, 0)

	/*err = ipc.UsePipeWriter(writer, kvs.rawData)
	if err != nil {
		print("Unable to use pipe writer", err)
	}*/

	/*err = ipc.UsePipeWriter(writer, rdtest)
	if err != nil {
		print("Unable to use pipe writer", err)
	}*/

	// open writer
	/*writer, err := ipc.OpenPipeWriter(path_to_pipe, kvs.rawData)
	if err != nil {
		print("Unable to open pipe writer: %v", err)
	}*/

	//kvs.FetchWriter(writer)

	// figure out how to http handle and flush the pipe at the same time

	// the key-value http handler will propose updates to raft
	serveHTTPKVAPI(kvs, *kvport, confChangeC, errorC)
}
