package util

import (
	"bufio"
	"os"

	"github.com/gupta-suyash/BFT-RSM/ipc_go/scrooge"
)

// For a file of urls where the i'th url is the address of the i-th node in a cluster
// This function will return a slice of NodeConfiguration representing the cluster
func ReadNodeConfigurations(file *os.File) []*scrooge.NodeConfiguration {
	scanner := bufio.NewScanner(file)
	var nodeConfigurations []*scrooge.NodeConfiguration

	for scanner.Scan() {
		url := scanner.Text()

		nodeConfiguration := scrooge.NodeConfiguration{
			UrlAddress: url,
			NodeId:     uint64(len(nodeConfigurations)),
		}

		nodeConfigurations = append(nodeConfigurations, &nodeConfiguration)
	}

	return nodeConfigurations
}
