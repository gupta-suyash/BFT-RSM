go func() {
	// Create Pipe and channel here (arbitrary oath)
	path_to_pipe := "/tmp/scrooge-input"

	// need to have ipc available
	err = ipc.CreatePipe(path_to_pipe)
	if err != nil {
		print("UNABLE TO CREATE PIPE: %v", err)
	}
	node.log.Infof("Pipe created for input!")
	rawData := make(chan []byte)
	defer close(rawData)
	err = ipc.OpenPipeWriter(path_to_algorand, rawData)
	if err != nil {
		print("Unable to open pipe writer: %v", err)
	}

	// Set some basic metrics here
	start := time.Now()
	sequenceNumber := 0

	for time.Since(start) < durationRunTest {
  for /* wait to get message(s) I can send to scrooge */ {
	if /* I have info to send scrooge! */ {
	  break
	}
  }
		for /* each message I want to send to scrooge */ {
	// Do any necessary pre-send message processing
			// Create message request
			request := &scrooge.ScroogeRequest{
				Request: &scrooge.ScroogeRequest_SendMessageRequest{
					SendMessageRequest: &scrooge.SendMessageRequest{
						Content: &scrooge.CrossChainMessageData{
							MessageContent: payload, //payload of some sort
							SequenceNumber: uint64(sequenceNumber),
						},
						ValidityProof: []byte("substitute valididty proof"),
					},
				},
			}
			node.log.Infof("Payload successfully loaded! It is size: %v", len(payload))
			requestBytes, err := proto.Marshal(request) // proto buf
			if err == nil {
				rawData <- requestBytes
				print("Bytes sent over the ipc NEW!")
				note_set[txns[index].Txn.Amount] = struct{}{}
			}
			sequenceNumber += 1
		}
	}
}()