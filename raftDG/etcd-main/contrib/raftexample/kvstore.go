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
	"os"
	// "strings"
	"sync"
	"sync/atomic"
	"time"

	"go.etcd.io/etcd/v3/contrib/raftexample/ipc-pkg"
	"go.etcd.io/etcd/v3/contrib/raftexample/scrooge"
	"google.golang.org/protobuf/proto"

	"go.etcd.io/etcd/server/v3/etcdserver/api/snap"
	"go.etcd.io/raft/v3/raftpb"

	//Writer imports
	"bufio"
)

// Global counter and counter lock
var sequenceNumber uint64 = 0

// a key-value store backed by raft
type kvstore struct {
	proposeC       chan<- string // channel for proposing updates
	rawData        chan []byte   // channel for scrooge
	mu             sync.RWMutex
	kvStore        map[string]string // current committed key-value pairs
	snapshotter    *snap.Snapshotter
	writer         *bufio.Writer // local writer TODO
	openPipe       *os.File      // pipe for writing to Scrooge
	reader         *bufio.Reader // local reader TODO
	openOutputPipe *os.File      // pipe for reading to Scrooge

	totalTime	   time.Duration
}

type kv struct {
	Key 	  string
	Val 	  string
	StartTime time.Time
	SeqNo	  uint64
}

func newKVStore(snapshotter *snap.Snapshotter, rawData chan []byte, proposeC chan<- string, commitC <-chan *commit, errorC <-chan error) *kvstore {
	s := &kvstore{
		rawData: 	 	rawData, 
		proposeC:    	proposeC, 
		kvStore:     	make(map[string]string), 
		snapshotter: 	snapshotter, 
		// sequenceNumber: 0,
		// startTime: 		make([]time.Time, 10000),
		// elapsedTime:	make([]time.Duration, 10000),
		// keyMap:			make(map[string]uint64),
	}
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

	go s.ScroogeReader(path_to_opipe)

	// create write pipe
	err = ipc.CreatePipe(path_to_pipe)
	if err != nil {
		print("Unable to open input pipe: %v", err, "\n")
	}
	print("Input pipe made", "\n")

	// open pipe writer
	s.writer, s.openPipe, err = ipc.OpenPipeWriter(path_to_pipe)
	if err != nil {
		print("Unable to open pipe writer: %v", err, "\n")
	}
	// print("passed the openpipewriter ", "\n")

	go s.readCommits(commitC, errorC) // go routine for sending input to Scrooge

	// go s.timeTracker()

	return s
}

func (s *kvstore) timeTracker() {
	TT := 120 * time.Second // Total experiment time
	WT := 30 * time.Second // Warm up time



	timerTT := time.NewTimer(TT)
	timerWT := time.NewTimer(WT)
	// fmt.Println("Start warm up")
	
	var warmupCount uint64
	var totalCount uint64
	go func() {
		<-timerWT.C
		warmupCount = atomic.LoadUint64(&sequenceNumber)
	}()

	go func() {
		<-timerTT.C
		totalCount = atomic.LoadUint64(&sequenceNumber)
		fmt.Println("Total experiment time: ", TT, " Warm up time: ", WT)
		fmt.Println("Total requests at end of warm up: ", warmupCount)
		fmt.Println("Total requests at end of experiment: ", totalCount)
		fmt.Println("Total elapsed time: ", s.totalTime)
		fmt.Println("Average latency: ", time.Duration(int64(s.totalTime) / int64(totalCount)))

		os.Exit(0)
	}()
}

func (s *kvstore) ScroogeReader(path_to_opipe string) {
	var err error
	// create read pipe
	err = ipc.CreatePipe(path_to_opipe)
	if err != nil {
		fmt.Printf("Unable to open output pipe: %v", err, "\n")
	}
	print("Output pipe made", "\n")

	// open pipe reader
	s.reader, s.openOutputPipe, err = ipc.OpenPipeReader(path_to_opipe)
	if err != nil {
		print("Unable to open pipe reader: %v", err, "\n")
	}
	// print("passed the openpipereader ", "\n")

	s.receiveScrooge()
}

func (s *kvstore) Lookup(key string) (string, bool) {
	s.mu.RLock()
	defer s.mu.RUnlock()
	v, ok := s.kvStore[key]
	return v, ok
}

func (s *kvstore) Propose(k string, v string) {
	// fmt.Println("Propose key: ", k)

	var buf bytes.Buffer

	// layout := "15:04:05.999"
	// startTimeString := time.Now().Format(layout)
	// startTime, _ := time.Parse(layout, startTimeString)
	startTime := time.Now()

	newKv := kv{
		Key:		k,
		Val:		v,
		StartTime:  startTime,
	}

	if err := gob.NewEncoder(&buf).Encode(newKv); err != nil {
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
			
			dec := gob.NewDecoder(bytes.NewBuffer([]byte(data)))
			if err := dec.Decode(&dataKv); err != nil {
				log.Fatalf("raftexample: could not decode message (%v)", err)
			}

			// Why mutex since only one thread reading commits?
			// s.mu.Lock()
			// s.kvStore[dataKv.Key] = dataKv.Val
			// s.mu.Unlock()

			seqNo := atomic.AddUint64(&sequenceNumber, 1) - 1
			// s.dummy(seqNo)

			s.sendScrooge(dataKv, seqNo)


			// fmt.Printf("-------- Latency Data starts --------\n\n\n")

			// elapsedTime := time.Since(dataKv.StartTime)
			// fmt.Println("Start time: ", dataKv.StartTime)
			// fmt.Println("End time: ", time.Now())
			// fmt.Println("Elapsed time: ", elapsedTime)

			// s.totalTime += elapsedTime
			// fmt.Println("Total elapsed time: ", s.totalTime)

			// fmt.Printf("\n\n\n-------- Latency Data ends --------\n")
			
		}
		close(commit.applyDoneC)
	}
	if err, ok := <-errorC; ok {
		log.Fatal(err)
	}
	// fmt.Println("kv store stops reading commit, closing pipe!")
	if err := s.openPipe.Close(); err != nil {
		log.Fatalf("kv store could not close opened pipe: ", err)
	}
}

func (s *kvstore) sendScrooge(dataK kv, seqNo uint64) {
	payload := []byte(dataK.Val)

	request := &scrooge.ScroogeRequest{
		Request: &scrooge.ScroogeRequest_SendMessageRequest{
			SendMessageRequest: &scrooge.SendMessageRequest{
				Content: &scrooge.CrossChainMessageData{
					MessageContent: payload,
					SequenceNumber: seqNo,
				},
				ValidityProof: []byte("substitute valididty proof"),
			},
		},
	}
	// fmt.Printf("Actual data: %v\nActual payload size: %v\n", dataK.Val, len(payload))

	var err error
	requestBytes, err := proto.Marshal(request)

	if err == nil {
		// fmt.Println("Request bytes:", requestBytes)
		// fmt.Println("Request bytes size: ", len(requestBytes))
		err = ipc.UsePipeWriter(s.writer, requestBytes)
		if err != nil {
			print("Unable to use pipe writer", err)
		}
	}
}

func (s *kvstore) receiveScrooge() {
	for true {
		ipc.UsePipeReader(s.reader)
		// fmt.Println("Reading done")
		// if err != nil {
		// 	print("Unable to use pipe reader")
		// }
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

func (s *kvstore) dummy(seqNo uint64) {
	return
}

