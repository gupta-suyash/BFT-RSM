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
	"bytes"
	"encoding/gob"
	"encoding/json"
	"fmt"
	"log"
	"strings"
	"sync"
	"os"

	"go.etcd.io/etcd/v3/contrib/raftexample/ipc-pkg"
	"go.etcd.io/etcd/v3/contrib/raftexample/scrooge"
	"google.golang.org/protobuf/proto"

	"go.etcd.io/etcd/server/v3/etcdserver/api/snap"
	"go.etcd.io/raft/v3/raftpb"

	//Writer imports
	"bufio"
)

// tbh not sure if this will work, test it and see

// a key-value store backed by raft
type kvstore struct {
	proposeC       chan<- string // channel for proposing updates
	rawData        chan []byte   // channel for scrooge
	mu             sync.RWMutex
	kvStore        map[string]string // current committed key-value pairs
	snapshotter    *snap.Snapshotter
	sequenceNumber int
	writer         *bufio.Writer // local writer TODO
	openPipe	   *os.File // pipe for writing to Scrooge
}

type kv struct {
	Key string
	Val string
}

func newKVStore(snapshotter *snap.Snapshotter, rawData chan []byte, proposeC chan<- string, commitC <-chan *commit, errorC <-chan error, seq int) *kvstore {
	s := &kvstore{rawData: rawData, proposeC: proposeC, kvStore: make(map[string]string), snapshotter: snapshotter, sequenceNumber: seq}
	snapshot, err := s.loadSnapshot()
	if err != nil {
		log.Panic(err)
	}
	if snapshot != nil {
		log.Printf("loading snapshot at term %d and index %d", snapshot.Metadata.Term, snapshot.Metadata.Index)
		if err := s.recoverFromSnapshot(snapshot.Data); err != nil {
			log.Panic(err)
		}
	}
	// read commits from raft into kvStore map until error
	// rdtest := make(chan []byte, 1)
	//byteArray := []byte{97, 98, 99, 100, 101, 102}
	//s.rawData <- byteArray

	err = ipc.CreatePipe(path_to_pipe)
	if err != nil {
		print("Unable to open pipe: %v", err, "\n")
	}
	print("Pipe made", "\n")

	s.writer, s.openPipe, err = ipc.OpenPipeWriter(path_to_pipe)
	if err != nil {
		print("Unable to open pipe writer: %v", err, "\n")
	}
	print("passed the openpipewriter ", "\n")

	/*err = ipc.UsePipeWriter(s.writer, rdtest)
	if err != nil {
		print("Unable to use pipe writer", err)
	}*/

	go s.readCommits(commitC, errorC)
	return s
}

func (s *kvstore) FetchWriter(Fwriter *bufio.Writer) {
	print("fectch called", "\n")
	s.writer = Fwriter
	print("fectch executed", "\n")
}

func (s *kvstore) Lookup(key string) (string, bool) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	v, ok := s.kvStore[key]
	return v, ok
}

func (s *kvstore) Propose(k string, v string) {
	var buf strings.Builder
	if err := gob.NewEncoder(&buf).Encode(kv{k, v}); err != nil {
		log.Fatal(err)
	}
	s.proposeC <- buf.String()
}

func (s *kvstore) readCommits(commitC <-chan *commit, errorC <-chan error) {
	for commit := range commitC {
		if commit == nil {
			// signaled to load snapshot
			snapshot, err := s.loadSnapshot()
			if err != nil {
				log.Panic(err)
			}
			if snapshot != nil {
				log.Printf("loading snapshot at term %d and index %d", snapshot.Metadata.Term, snapshot.Metadata.Index)
				if err := s.recoverFromSnapshot(snapshot.Data); err != nil {
					log.Panic(err)
				}
			}
			continue
		}

		for _, data := range commit.data {
			var dataKv kv

			dec := gob.NewDecoder(bytes.NewBufferString(data))
			if err := dec.Decode(&dataKv); err != nil {
				log.Fatalf("raftexample: could not decode message (%v)", err)
			}

			print("call sendscrooge", "\n")
			s.sendScrooge(dataKv)
			print("end sendscrooge", "\n")

			s.mu.Lock()
			s.kvStore[dataKv.Key] = dataKv.Val
			s.mu.Unlock()
			s.sequenceNumber += 1
		}
		close(commit.applyDoneC)
	}
	if err, ok := <-errorC; ok {
		log.Fatal(err)
	}
	fmt.Println("kv store stops reading commit, closing pipe!")
	if err := s.openPipe.Close(); err != nil {
		log.Fatalf("kv store could not close opened pipe: ", err)
	}
}

func (s *kvstore) sendScrooge(dataK kv) {
	fmt.Println("startedSScr")
	request := &scrooge.ScroogeRequest{
		Request: &scrooge.ScroogeRequest_SendMessageRequest{
			SendMessageRequest: &scrooge.SendMessageRequest{
				Content: &scrooge.CrossChainMessageData{
					MessageContent: []byte(dataK.Val), //payload of some sort, check type
					SequenceNumber: uint64(s.sequenceNumber),
				},
				ValidityProof: []byte("substitute valididty proof"),
			},
		},
	}
	fmt.Printf("Payload successfully loaded! It is size: %v\n", len(dataK.Val))
	var err error
	requestBytes, err := proto.Marshal(request)
	if err == nil {
		fmt.Println("datareq:", requestBytes, "\n", "dataK:", dataK.Val)
		err = ipc.UsePipeWriter(s.writer, requestBytes, []byte(dataK.Val))
		fmt.Println("Writing done")
		if err != nil {
			print("Unable to use pipe writer", err)
		}
	}

}

func (s *kvstore) getSnapshot() ([]byte, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	return json.Marshal(s.kvStore)
}

func (s *kvstore) loadSnapshot() (*raftpb.Snapshot, error) {
	snapshot, err := s.snapshotter.Load()
	if err == snap.ErrNoSnapshot {
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	return snapshot, nil
}

func (s *kvstore) recoverFromSnapshot(snapshot []byte) error {
	var store map[string]string
	if err := json.Unmarshal(snapshot, &store); err != nil {
		return err
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	s.kvStore = store
	return nil
}
