$PWD

set -e

# load environment parameters
. ./script/env.sh

# load ip list
. ./script/load_config.sh $1

config_real_path=$(readlink -f $1) 
config_real_dir=$(dirname ${config_real_path})

# obtain the src path
server_path=`echo "$server" | sed 's/:/\//g'`
server_path=${server_path:1}
server_name=`echo "$server" | awk -F':' '{print $NF}'`
server_bin=${server_name}

bin_path=bazel-bin/${server_path}

deploy_iplist=${iplist[@]}

echo "server name:"${server_bin}
echo "deploy to :"${deploy_iplist[@]}

# commands functions
function run_cmd(){
  count=1
  for ip in ${deploy_iplist[@]};
  do
     ssh -n -o BatchMode=yes -o StrictHostKeyChecking=no sugupta@${ip} "$1" &
    ((count++))
  done

  while [ $count -gt 0 ]; do
        wait $pids
        count=`expr $count - 1`
  done
}


run_cmd "killall -9 ${server_bin}"
