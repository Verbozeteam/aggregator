import threading
import os
import socket
import json
import struct
import argparse
from multiprocessing import Pool

def f(x):
    return x*x

parser = argparse.ArgumentParser(description='Middleware core')
parser.add_argument("-n", "--num_middlewares", required=False, default=100, type=int, help="Number of middlewares to emulate")
cmd_args = parser.parse_args()

NUM_MIDDLEWARES = cmd_args.num_middlewares
PORT_BUCKETS = [[]]

identity_contents = ""
for i in range(NUM_MIDDLEWARES):
    port = 14567 + i
    if len(PORT_BUCKETS[-1]) >= 100:
        PORT_BUCKETS.append([])
    PORT_BUCKETS[-1].append(port)

    if i != 0:
        identity_contents += "\n"
    identity_contents += "3\nTest Room {}:{}:2222".format(i+1, port)

def identity_hosting(identity_contents):
    with open("identity", "w") as F:
        F.write(identity_contents)
    os.system("./identity_runner.sh > /dev/null")
identity_thread = threading.Thread(target=identity_hosting, args=(identity_contents, ))
identity_thread.start()

def process_entry(ports):
    print ("Running {} middlewares...".format(len(ports)))
    def middleware(port):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind(("0.0.0.0", port))
        s.listen(1)
        (client, addr) = s.accept()
        while True:
            print (client.recv(1024))
        return

    threads = []
    for port in ports:
        t = threading.Thread(target=middleware, args=(port, ))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

process_pool = Pool(len(PORT_BUCKETS))
process_pool.map(process_entry, PORT_BUCKETS)

identity_thread.join()
