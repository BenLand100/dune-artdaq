#!/usr/bin/env python
import sys
import zmq
import numpy as np

port = "5555"
if len(sys.argv) > 1:
    port = sys.argv[1]
    int(port)

if len(sys.argv) > 2:
    port1 = sys.argv[2]
    int(port1)

# Socket to talk to server
context = zmq.Context()
socket = context.socket(zmq.SUB)

print "Collecting updates from weather server..."
socket.connect("tcp://localhost:%s" % port)

socket.setsockopt(zmq.SUBSCRIBE, '')
# Try to receive
try:
    x = socket.recv()

    print True, 'aaaa', type(x), np.frombuffer(x, dtype=np.uint64)
except Exception:
    print False
finally:
    socket.close() 