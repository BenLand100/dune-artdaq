import time
import struct
import socket
import errno
import threading

from rceDataFormats import *

class RceDataSender(object):

    def __init__(self, use_tcp=False):

        self.dest_host = '127.0.0.1'
        self.dest_port = 8989
        self.send_rate = 10.0
        self.num_millislices = 10
        self.num_microslices = 10
        self.num_nanoslices = 32
        self.adc_mode = 0
        self.adc_mean = 1000.0
        self.adc_sigma = 100.0
        
        self.use_tcp = use_tcp

    def set_host(self, host):
        self.dest_host = host

    def set_port(self, port):
        self.dest_port = int(port)

    def set_rate(self, rate):
        self.send_rate = float(rate)

    def set_millislices(self, millislices):
        self.num_millislices = int(millislices)

    def set_microslices(self, microslices):
        self.num_microslices = int(microslices)

    def set_nanoslices(self, nanoslices):
        self.num_nanoslices = int(nanoslices)

    def set_adcmode(self, adcmode):
        self.adc_mode = int(adcmode)
        
    def set_adcmean(self, adcmean):
        self.adc_mean = float(adcmean)
        
    def set_adcsigma(self, adcsigma):
        self.adc_sigma = float(adcsigma)
        
    def run(self):

        cmd_ok = True
        reply = ""

        try:
            if self.use_tcp:
                print "Opening TCP socket connection to host %s port %d" % (
                    self.dest_host, self.dest_port)
                self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.sock.connect((self.dest_host, self.dest_port))
            else:
                self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        except socket.error as sock_err:
            if sock_err.errno == errno.ECONNREFUSED:
                cmd_ok = False
                reply = "Connection failed: %s" % sock_err
                print reply
            else:
                cmd_ok = False
                reply = "Unexpected socket error: %s" % sock_err
                print reply
        else:
            self.run_thread = threading.Thread(target=self.send_loop)
            self.run_thread.daemon = True
            self.run_thread.start()

        return cmd_ok, reply

    def send_loop(self):

        print "Sender loop starting: %d millislices of %d microslices at rate %.1f host %s port %d" % (
          self.num_millislices, self.num_microslices, self.send_rate, self.dest_host, self.dest_port)

        # num_samples = 40
        # start_sample = 1000
        # data = range(start_sample, start_sample + num_samples)
        # pack_format = '<' + str(num_samples) + 'H'
        # message = struct.pack(pack_format, *data)

        send_interval = 1.0 / self.send_rate

        uslice = RceMicroslice(0)
        uslice.generate_nanoslices(self.num_nanoslices, self.adc_mode, self.adc_mean, self.adc_sigma)
#        message = uslice.pack()
        
        start_time = time.time()

        num_uslices_total = self.num_millislices * self.num_microslices
        
        for count in range(num_uslices_total):
            next_time = time.time() + send_interval

            uslice.set_sequence_id(count)
            uslice.set_timestamp()
            message = uslice.pack()
            
            if self.use_tcp:
                self.sock.sendall(message)
            else:
                self.sock.sendto(message, (self.dest_host, self.dest_port))

            wait_interval = next_time - time.time()
            if wait_interval > 0.0:
                time.sleep(wait_interval)

        elapsed_time = time.time() - start_time

        sent_rate = float(num_uslices_total) / elapsed_time
        print "Sender loop terminating: sent %d microslices total in %f secs, rate %.1f Hz" % (
          num_uslices_total, elapsed_time, sent_rate
        )

        if self.use_tcp:
            self.sock.close()
            
    def stop(self):
        
        cmd_ok = True
        reply = ""
        
        self.run_thread.join()
        
        return cmd_ok, reply
