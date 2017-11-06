import threading
import os

NUM_MIDDLEWARES = 10

def middleware():
    return

threads = []
for i in range(NUM_MIDDLEWARES):
    t = threading.Thread(target=middleware)
    threads.append(t)
    t.start()

