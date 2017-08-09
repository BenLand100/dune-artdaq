import sys
import zmq
import datetime,time

socket_address = "tcp://*:5566"
frontend_name = "FrontEnd01"

context = zmq.Context()
socket = context.socket(zmq.PUB)
socket.bind(socket_address)

counter = 5
while True:
    status = "XON"
    if counter%20 < 5:
        status = "XOFF"
    socket.send_string("INHIBITMSG_%s-%s-%s" % (status,frontend_name,datetime.datetime.now()))
    print "Sent status %s" % status
    counter += 1
    time.sleep(1)

