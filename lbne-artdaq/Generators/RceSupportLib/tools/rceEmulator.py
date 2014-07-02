import SocketServer
import threading
import string
import time
import struct
import socket
import argparse

class RceEmulator(object):

    def __init__(self):

        parser = argparse.ArgumentParser(description="RCE data fragment transmission emulator")
        parser.add_argument('--host', action="store", dest="host", default='',
                            help="Host address for control server to listen on")
        parser.add_argument("--port", action="store", type=int, dest="port", default=9999,
                            help="Port for control server to listen on")
        self.args = parser.parse_args()


        SocketServer.TCPServer.allow_reuse_address = True
        self.server = SocketServer.TCPServer((self.args.host, self.args.port), RceServerHandler)
        self.server.parser = RceCommandParser()

    def run(self):

        print "\n\nRCE Server emulator starting up on", self.args.host, "port", self.args.port
        self.server.serve_forever()

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
                    # Pass data to command parser
                    response = self.server.parser.parse_command(line)

                    # Return response to client
                    self.request.sendall(response)

        print "Closed conection from {}". format(self.client_address[0])


class RceCommandParser(object):
    """
    RceCommandParser - command parser for RCE emulator
    """

    def __init__(self):

        # Define dictionary of legal commands and assoicated methods
        self.commands = { 'START' : self.start_cmd,
                          'STOP'  : self.stop_cmd,
                          'PING'  : self.ping_cmd,
                          'SET'   : self.set_cmd,
                          'GET'   : self.get_cmd,
                        }
        
        # Define empty dictionary of parameters to be set/get
        self.params = {}
        
    def parse_command(self, cmd_data):
        """
        Command parser - takes a line of space-separated data and parses it
        into a command and associated parameter arguments
        """

        # Split data into space separated list
        cmd_words = string.split(cmd_data)

        #print ">>>", cmd_words

        req_command = cmd_words[0].upper()

        # Test if first word, i.e. the command word, is a legal command
        if req_command in self.commands:

            # If so, split remaining words into key-value pairs separated by '=''
            try:
                args = dict(item.split("=") for item in cmd_words[1:])

                # Pass arguments to command method
                (cmd_ok, reply) = self.commands[req_command](args)
                
            except ValueError:
                cmd_ok = False
                reply = "Illegal argument format specified"
                
        else:
            cmd_ok = False
            reply = "Unrecognised command"

        # Format response depending on success of command
        if cmd_ok:
            response = "ACK {} {}\n".format(req_command, reply)
        else:
            response = "NACK {} error=\"{}\"\n".format(req_command, reply)
        print "  >>> ", response,
        return response

    def start_cmd(self, args):
        """
        Handle a START command to transmit simulated RCE data fragments over UDP
        """

        cmd_ok = True
        reply=""

        print "Got START command", args

        # Create a data sender
        sender = RceDataSender()

        # Loop over legal parameters for the data sender; if they exist in
        # the current parameter list, call the appropriate set_ method on the sender.
        # Trap any exceptions raised by the set_ methods trying to convert
        # an argument value to the approriate type
        try:
            for param in ['host', 'port', 'rate', 'frags']:
                if param in self.params:
                    getattr(sender, 'set_' + param)(self.params[param])

        except ValueError as e:
            print "Error parsing argument to START command:", e
            cmd_ok = False

        except AttributeError as e:
            print "Attempted to set illegal parameter for data sender:", e
            cmd_ok = False

        else:
            sender.run()

        return cmd_ok, reply

    def stop_cmd(self, args):

        cmd_ok = True
        reply = ""
        print "Got STOP command", args

        return cmd_ok, reply

    def ping_cmd(self, args):

        cmd_ok = True
        reply = ""
        print "Got PING command", args

        return cmd_ok

    def set_cmd(self, args):

        cmd_ok = True
        reply = ""
        print "Get SET command", args

        for required_arg in ['param', 'value', 'type']:
            if not required_arg in args:
                reply = "Required {} argument missing".format(required_arg)
                cmd_ok = False

        if cmd_ok:
            try:
                self.params[args['param']] = eval(args['type'])(args['value'])

            except NameError as e:
                reply = "Failed to set parameter {} of type {} to value {} : {}".format(args['param'], args['type'], args['value'], e)
                cmd_ok = False

        return cmd_ok, reply

    def get_cmd(self, args):

        cmd_ok = True
        reply = ""
        print "Got GET command", args

        if 'param' in args:
            if args['param'] in self.params:
                reply = "param={} value={}".format(args['param'], self.params[args['param']])
            else:
                reply = "No such parameter {}".format(args['param'])
                cmd_ok = False
        else:
            reply = "No param argument specified"
            cmd_ok = False

        return cmd_ok, reply

class RceDataSender(object):

    def __init__(self):

        self.dest_host = '127.0.0.1'
        self.dest_port = 8989
        self.frag_rate = 10.0
        self.num_frags = 10

    def set_host(self, host):
        self.dest_host = host

    def set_port(self, port):
        self.dest_port = int(port)

    def set_rate(self, rate):
        self.frag_rate = float(rate)

    def set_frags(self, frags):
        self.num_frags = int(frags)

    def run(self):

        self.run_thread = threading.Thread(target=self.send_loop)
        self.run_thread.daemon = True
        self.run_thread.start()

    def send_loop(self):

        print "Sender loop starting: %d frags at rate %.1f host %s port %d" % (
          self.num_frags, self.frag_rate, self.dest_host, self.dest_port)

        num_samples = 40
        start_sample = 1000
        data = range(start_sample, start_sample + num_samples)
        pack_format = '<' + str(num_samples) + 'H'
        message = struct.pack(pack_format, *data)

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        start_time = time.time()
        for count in range(self.num_frags):
            sock.sendto(message, (self.dest_host, self.dest_port))
            time.sleep(1.0 / self.frag_rate)
        elapsed_time = time.time() - start_time

        sent_rate = float(self.num_frags) / elapsed_time
        print "Sender loop terminating: sent %d frags in %f secs, rate %.1f Hz" % (
          self.num_frags, elapsed_time, sent_rate
        )

if __name__ == "__main__":

    emulator = RceEmulator()
    emulator.run()
