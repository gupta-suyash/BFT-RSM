# Artifact Eval Instructions

This is the repository for the Artifact Evaluation of OSDI2025 proceeding: "Picsou: Enabling Replicated State Machines to Communicate Efficiently".

For all questions about the artifact please e-mail Reginald Frank [reginaldfrank77@berkeley.edu](mailto:reginaldfrank77@berkeley.edu). 



# Table of Contents
1. [General Information](#General Information)
2. [High Level Claims](#High Level Claims)
3. [Artifact Organization](#Artifact Organization)
4. [Validating the Claims](#Validating the Claims)

# General Information

This artifact contains, and allows to reproduce, experiments for (almost*) all figures included in the paper "Picsou: Enabling Replicated State Machines to Communicate Efficiently". 

*Cross-Country experiments have been excluded due to high GCP credit cost. Kafka results are also available but their collection is not automated.

It contains a prototype implementation of Picsou, a  Crash and Byzantine Fault Tolerant Cross-Cluster Consistent Broadcast protocol. The prototype is intended to connect two separate consensus instances, and currently there is support for RAFT (Etcd's Raft implementation) as well as FILE (an "infinitely fast" simulated consensus protocol). The prototype can simulate both crash and byzantine failures and it remains resilient to both. While the Picsou protocol can tolerate arbitrary failure patterns, the prototype does not attempt to simulate all possible behaviors. For example, while the protocol can handle crash failures, if a machine is shutdown during a test the prototype will abort after detecting the unresponsive machine (to avoid collecting results with an unexpected crash).

> **[NOTE]** The Basil prototype codebase is henceforth called "*Scrooge*". Throughout this document you will find references to Scrooge, and many configuration files are named accordingly. All these occurrences refer to the Picsou prototype.

Picsou's current codebase (BFT-RSM) was modified in order to improve ease of collecting results for artifact evaluation. While takeaways remain consistent, individual performance results may differ slightly across the benchmarks (better performance in some cases) as other minor modifications to the codebase were necessary to support the changes.

In addition to Picsou, this artifact contains prototype implementations for 5 baselines:

1. One-Shot (OST): In OST, a message is sent by a single sender to a single receiver. OST is only meant as a performance upper-bound. It does not satisfy C3B as message delivery cannot be guaranteed
2.  All-To-All (ATA): In ATA, every replica in the sending RSM sends all messages to all receiving replicas (O(N^2) message complexity). As in SCROOGE, every correct receiver is guaranteed to eventually receive the message
3. Leader-To-Leader (LL): The leader of the sending RSM sends a message to the leader of the receiving RSM, who then internally broadcasts the message. This protocol does not guarantee eventual delivery when leaders are faulty.
4. OTU: Modeling the C3B strategy in the paper GeoBFT (Suyash et al.), OTU breaks down an RSM into a set of sub-RSMs. Much like LL, GeoBFT’s cross-RSM communication protocol, OTU, has the leader of the sender RSM send its messages to at least (#faulty_receivers) +1 receiver RSM replicas. Each receiver then internally broadcast these messages. When the leader is faulty, replicas timeout and request a resend. OTU thus guarantees eventual delivery after at most (#faulty_senders) +1 resends in the worst-case (for O(faulty^2)=O(N^2) total messages).
5. KAFKA: Apache KAFKA is the de-facto industry-standard for exchanging data between services. Producers write data to a Kafka cluster, while consumers read data from it. Kafka, internally, uses Raft to reliably dissemi- nate messages to consumers. We use Kafka 2.13-3.7.0.

For more information read the current paper draft section 6 evaluation.

# High Level Claims

* **Main claim 1**: For all local baseline tests (`Throughput vs Message Size @ 19-node-network`, `Throughput vs Message Size @ 4-node-network`, `Throughput vs Network Size @ 100B-messages`, `Throughput vs Network Size @ 1MB-messages`) Picsou's Throughput lies between All-To-All (ATA) and One-Shot (OST). Expected performance is between 2.5-15x higher performance than A2A depending on network size
* **Main claim 2**: For all failure tests (`Byzantine Failures: Throughput vs Network Size`, `Crash Failures: Throughput vs Network Size`) Picsou has higher performance than its BFT (A2A) and CFT (A2A, LL, OTU) comparative baselines. Expected performance is between 2.5x and 7x higher than A2A where LL and OTU performance is comparatively similar to A2A
* **Main claim 3**: For all geo-distributed tests (`Data Reconciliation: Throughput vs Message Size`, `Disaster Recovery: Throughput vs Message Size`) As the size of the sent messages increase, the general throughput of all strategies will increase. Critically, strategies A2A, LL, OTU, will never exceed a system wide throughput of 50MBps (50MBps is the throughput of sending a message between 2 nodes cross region). In contrast `OST` and `Picsou` both will be able to reach 70MBps (the maximum throughput of Etcd Raft on these machines)
* **Supplementary**: All other microbenchmarks reported realistically represent Picsou.
* Example artifact output: [https://drive.google.com/file/d/1lXxlT_wlib-EFfCos_lPbRLkvaWvwtHG/view?usp=sharing](https://drive.google.com/file/d/1lXxlT_wlib-EFfCos_lPbRLkvaWvwtHG/view?usp=sharing)


# Artifact Organization

The artifact spans across the following 3 GitHub repositories. Please checkout the corresponding branch when validating claims for a given system.
1. [BFT-RSM](https://github.com/gupta-suyash/BFT-RSM/tree/main): Contains testing infrastructure and implementation of Picsou and our Baselines 1-4 (all but Kafka)
   1. Baseline implementations
      1. `Code/system/scrooge.cpp` -- scrooge implementation
      2. `Code/system/one_to_one.cpp` -- OST implementation (one_to_one unfair variant)
      3. `Code/system/all_to_all.cpp` -- all_to_all implementation
      4. `Code/system/geobft.cpp` -- OTU implementation
      5. `Code/system/leader_leader.cpp` -- LL implementation

   2. Testing Infrastructure
      1. `Code/experiments/results` -- location where all test results will be stored
      2. `Code/util_graph.py` -- python file which contains mapping between Graphs to visualize, and experiments to run for each datapoints
      3. `Code/auto_run_all_experiments.py` -- Main testing framework which:
         1. Loads all experiments needed to visualize graphs from `util_graph.py`
         2. Checks `Code/experiments/results` to see what experiments are remaining to run
         3. Spawns the required network for testing using `Code/auto_make_nodes.sh`
         4. Runs the required tests using `Code/auto_build_exp.sh` and  `Code/auto_run_exp.sh` to build and run tests
         5. Deletes the created network using `Code/auto_delete_nodes.sh`

      4. `Code/experiments/experiment_scripts/eval_all_graphs.py` -- script which uses `Code/util_graph.py` to load all desired graphs, pulls in results from `Code/experiments/results` and then outputs multiple `graph.png` files and a text representation of all results collected

2. [scrooge-kafka](https://github.com/chawinphat/scrooge-kafka): Contains a producer / consumer shim to use Apache Kafka as a Cross-Cluster Consistent Broadcast protocol.
3. [go-algorand](https://github.com/mmurray22/go-algorand/tree/71d8e9d38fbc40bb7836e97f1b37f233904524bd): Contains the edited version of Algorand which sends all blocks to Scrooge to communicate with other Consensus Clusters
4. [raft-application](https://github.com/mmurray22/raft-application/tree/047cfa5fa5d3d62cb200701224434979611bc3ab): Contains the edited version of Etcd Raft which sends all blocks to Scrooge to communicate with other Consensus Clusters. Additionally has edits for disaster recovery and key reconciliation. 
5. [resdb-application](https://github.com/mmurray22/resdb-application/tree/cbd92c10d44b23d7a208b62d8be1df20a748f049): Contains the edited version of ResDB  which sends all blocks to Scrooge to communicate with other Consensus Clusters. 


# Validating the Claims

All our experiments were run using Google Cloud. To reproduce our results and validate our claims with our testing infrastructure, you will need to 1) get access to our google cloud repo 2) Create a vm using our provided worker image, and 3) Run the `Code/auto_run_all_experiments.py` script to collect results and 4) Run `Code/experiments/experiment_scripts/eval_all_graphs.py` to visualize all trends. If you wish to use our code outside of GCP (e.g. cloud lab) that is ok, but our protocol parameters may need to change for the new machines/network and running cross-region experiments may also not be possible. Let us know if this is your desired method and we can provide additional instructions.



### Step by step guide:

1. Email [reginaldfrank77@berkeley.edu](mailto:reginaldfrank77@berkeley.edu) to get access to our Google Cloud project
2. Create a new VM with the osdi-artifact-eval.img
   1. Navigate to `Virtual Machines` > `VM instances` > `Create instance`
   2. Ensure region is `us-west1` zone can be any
   3. For Machine type select `c2-standard-8 (8 vCPUs, 32 GB Memory)`
   4. Under `OS and storage` change the image to `Custom image` > `osdi-artifact-evaluation-image`
   5. Create the VM and wait for it to be created
3. SSH onto the VM.
   1. Execute `sudo -su scrooge` (this will need to be done any time you ssh onto your created VM)
4. Run the experiments
   1. Execute `cd ~/BFT-RSM/Code` to migrate to the directory with all testing infrastructure
   2. Execute `tmux new -s <your session name>` to enter a new tmux session
   3. Execute `./auto_run_all_experiments.py` to start all experiments.
   4. You can follow along the python script output by following logs output into  `/tmp` or look at node outputs in  `~/BFT-RSM/Code/experiments/results`
   5. You can also find the ip addresses of machines in the experiments by navigating to the GCP website > `Instance groups` > `Instance groups` Feel free to ssh onto them and monitor node performance using tools like `htop` (CPU usage) and `nload` (network usage)
   6. If you want to re-run an experiment -- delete its corresponding folder in  `~/BFT-RSM/Code/experiments/results` and re-run `./auto_run_all_experiments.py`
5. Visualize the results
   1. Execute `cd ~/BFT-RSM/Code/experiments/results` to navigate to the directory containing all result files
   2. Execute `./eval_all_graphs.py` to load all result files and run analysis on them
   3. View outputted text to see numerical results -- View graphs visually at the multiple `*.png` files now in the results directory





Finally thanks for viewing/evaluating our artifact, let us know if you have any questions!
