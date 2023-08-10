#! /bin/bash

install_path=$(pwd)
echo "installing etcd in: ${install_path}"

HOSTS=("10.10.1.1" "10.10.1.2" "10.10.1.3" "10.10.1.4" "10.10.1.5"
       "10.10.1.6" "10.10.1.7" "10.10.1.8" "10.10.1.9" "10.10.1.10"
       "10.10.1.11" "10.10.1.12" "10.10.1.13" "10.10.1.14" "10.10.1.15")

function setup() {
  for i in ${!HOSTS[@]}; do
    # ssh into each machine and do the setup work in parallel
    (ssh -A -o StrictHostKeyChecking=no ${HOSTS[${i}]} "$1; $2; $3; $4;") &

  done
  wait
}

setup_commands=(
    # Upgrade installed packages and install golang
    "sudo add-apt-repository -y ppa:longsleep/golang-backports"
    "sudo apt update"
    "sudo apt -y upgrade"
    "sudo apt -y install golang-go"
)

setup "${setup_commands[@]}"