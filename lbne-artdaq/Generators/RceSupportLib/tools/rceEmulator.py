import SocketServer
import string
import time
import struct
import socket
import argparse
import signal

from rceDataSender import *
from rceKeyValueCommandParser import *
from rceXmlCommandParser import *

class RceEmulator(object):

    def __init__(self):

        parser = argparse.ArgumentParser(description="RCE data fragment transmission emulator")
        parser.add_argument('--host', action="store", dest="host", default='',
                            help="Host address for control server to listen on")
        parser.add_argument("--port", action="store", type=int, dest="port", default=9999,
                            help="Port for control server to listen on")
        group = parser.add_mutually_exclusive_group()
        group.add_argument('--xml', action="store_true", default=True,
                            help="Enable RCE-style XML command parser")
        group.add_argument("--kvp", action="store_true", default=False,
                            help="Enable legacy key-value pair command parser")
        self.args = parser.parse_args()

        self.doneEvent = threading.Event()

        SocketServer.TCPServer.allow_reuse_address = True
        self.server = SocketServer.TCPServer((self.args.host, self.args.port), RceServerHandler)
        if self.args.xml:
            self.server.parser = RceXmlCommandParser()
        else:
            self.server.parser = RceKeyValueCommandParser()

    def run(self):

        signal.signal(signal.SIGINT, self.signal_handler)

        print "\n\nRCE Server emulator starting up on", self.args.host, "port", self.args.port
        self.server.serve_forever()

        self.doneEvent.wait()

    def terminate(self):
        self.server.shutdown()
        self.doneEvent.set()

    def signal_handler(self, signum, frame):
        print "Caught interrupt, shutting down emulator..."
        t = threading.Thread(target=self.terminate)
        t.start()

class RceServerHandler(SocketServer.BaseRequestHandler):
    """
    RceServerHandler - request handler for RCE emulator
    """

    def handle(self):
        """
        Handle request, i.e. client connection, for RCE emulator
        """

        print "Opened conection from {} {}". format(self.client_address[0], self.client_address[1])

        # Loop until client closes connecion
        while True:

            # Receive data from client, break if empty (i.e. closing)
            self.data = self.request.recv(1024)
            if not self.data:
                break

            for line in self.data.splitlines():

                line = line.strip()
                if len(line) > 0:
                    
                    if line == '\a':
                        print "Got async disable sequence, ignoring"
                        continue
                    
                    # Pass data to command parser
                    response = self.server.parser.parse_request(line)

                    # Return response to client
                    self.request.sendall(response)

        print "Closed conection from {}". format(self.client_address[0])

if __name__ == "__main__":

    emulator = RceEmulator()
    emulator.run()
