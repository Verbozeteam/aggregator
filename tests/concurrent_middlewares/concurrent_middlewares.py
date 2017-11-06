import threading

NUM_MIDDLEWARES = 300

def middleware():
    return

threads = []
for i in range(NUM_MIDDLEWARES):
    t = threading.Thread(target=middleware)
    threads.append(t)
    t.start()

