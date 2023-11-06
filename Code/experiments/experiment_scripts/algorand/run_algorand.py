# This is but a sketch...fill in all details
def main():
    update_gensis()
    update_wallets()
    executeCommand(pathname + "/remove_stale.sh")
    executeCommand(pathname + "/participation_rsm.sh")

# Update local wallet app json
def update_wallets(app_pathname, send_acct, receive_acct): # Check the client IP
    node_path = app_pathname + "/node/"
    wallet_path = app_pathname + "/go-algorand/wallet_app/node.json"
    appFile = open(wallet_path, 'w')
    api_token = open(node_path + "/algod.token", 'r')
    kmd_token = open(node_path + "/kmd-v0.5/kmd.token", 'r')
    appDict = {"api_token": api_token.readlines()[0].strip(), 
               "kmd_token": kmd_token.readlines()[0].strip(), 
               "send_acct": send_acct, 
               "receive_acct": receive_acct, 
               "server_url": "http://127.0.0.1", 
               "algo_port": 8080, 
               "kmd_port": 7833, 
               "wallet_port": 1234, 
               "client_port": 4003, 
               "algorand_port": 3456, 
               "client_ip": "128.110.218.203"
               }
    appFile.write(json.dumps(appDict))
    appFile.close()

# Update all genesis files
def update_gensis():
    # Copy genesis from main server
    node_path = app_pathname + "/node/"

if __name__ == "__main__":
    main()