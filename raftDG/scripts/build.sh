#! /bin/bash

install_path=$(pwd)
echo "installing etcd in: ${install_path}"

HOSTS=("10.10.1.1" "10.10.1.2" "10.10.1.3" "10.10.1.4" "10.10.1.5"
       "10.10.1.6" "10.10.1.7" "10.10.1.8" "10.10.1.9" "10.10.1.10"
       "10.10.1.11" "10.10.1.12" "10.10.1.13" "10.10.1.14" "10.10.1.15")

function build() {
  for i in ${!HOSTS[@]}; do
    # ssh into each machine and do the build work in parallel
    (ssh -A -o StrictHostKeyChecking=no ${HOSTS[${i}]} "$1; $2; $3; $4; $5; $6; $7; $8") &

  done
  wait
}

build_commands=(
    # Clone the remote BFT-RSM repo into install path
    "mkdir -p ${install_path}"
    "cd ${install_path}"
    "rm -rf ${install_path}/BFT-RSM"
    "GIT_SSH_COMMAND='ssh -o StrictHostKeyChecking=no' git clone git@github.com:gupta-suyash/BFT-RSM.git"
    "cd ${install_path}/BFT-RSM"
    "git checkout raftexample"

    # Build etcd binaries
    "cd ${install_path}/BFT-RSM/raftDG/etcd-main"
    "./scripts/build.sh"

    # Build Scrooge
    # "cd ${install_path}/BFT-RSM/Code"
    # "make"
)

build "${build_commands[@]}"