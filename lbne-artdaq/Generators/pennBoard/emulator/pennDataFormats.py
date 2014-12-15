import struct
import random
import math
import datetime
import time
import sys


def bin_to_char(b):
    #print "bin to char", b
    #for i in xrange(0, len(b), 8):
    #    print b[i:i+8], int(b[i:i+8], 2)
    return ''.join(chr(int(b[i:i+8], 2)) for i in xrange(0, len(b), 8))

def simple_twos_complement(s):
    s = s.replace('0','2')
    s = s.replace('1','0')
    s = s.replace('2','1')
    return s

def print_string_as_bin(s):
    for c in s:
        h = binascii.hexlify(c)
        print h, int(h,16), bin(int(h,16))[2:].zfill(8)

def print_payload_header(s):
    b = ''
    for c in s:
        h = binascii.hexlify(c)
        b +=  bin(int(h,16))[2:].zfill(8)
    print "mode", b[:4]
    print "28-bit timestamp", int(b[4:], 28), b[4:]
    return b[:4]

def print_payload(s):
    b = ''
    for c in s:
        h = binascii.hexlify(c)
        b +=  bin(int(h,16))[2:].zfill(8)
    print 'payload', b

class NovaTimestamp(object):
    
    # NoVa epoch is 00:00:00 GMT 1st Jan, 2000 
    epoch = datetime.datetime(2000, 1, 1, 0, 0, 0, 0)
    
    # NoVa clock is 64 MHZ, = 15.6ns tick
    ticks_per_sec = 64E6
    
    def __init__(self, posix_time=None):
        
        if posix_time == None:
            self.now = datetime.datetime.now()
        else:
            self.now = datetime.datetime.fromtimestamp(posix_time)
            
        self.since_epoch       = (self.now - NovaTimestamp.epoch).total_seconds()
        self.ticks_since_epoch = int(self.since_epoch * NovaTimestamp.ticks_per_sec)
        self.timestamp_low     = self.ticks_since_epoch & 0xFFFFFFFF
        self.timestamp_high    = (self.ticks_since_epoch >> 32) & 0xFFFFFF
        self.timestamp_short   = self.ticks_since_epoch & 0xFFFFFFF
        
class PennNanoslice(object):

    #ASSUME little-endian
    format_header            = '<4c'   #32 bit header = 8 bit version + 8 bit sequence + 16 bit block size(bytes)
    format_payload_header    = '<4c'   #32 bit header = 4 bit type + 28 bit (partial) timestamp
    #ASSUME 128 bit data
    format_payload_counter   = '<16c'  #128 bit data
    #ASSUME 8 bit data
    format_payload_trigger   = '<1c'   #8 bit data
    format_payload_timestamp = '<8c'   #64 bit data

    #ASSUME 128 bit data
    num_values_counter = 128
    #ASSUME 8 bit data
    num_values_trigger = 8

    version            = 0x3

    @classmethod
    def length_header(cls):
        return struct.calcsize(PennNanoslice.format_header) / struct.calcsize("<c")

    @classmethod
    def length_payload_counter(cls):
        return (struct.calcsize(PennNanoslice.format_payload_header) + struct.calcsize(PennNanoslice.format_payload_counter  )) / struct.calcsize("<c")

    @classmethod
    def length_payload_trigger(cls):
        return (struct.calcsize(PennNanoslice.format_payload_header) + struct.calcsize(PennNanoslice.format_payload_trigger  )) / struct.calcsize("<c")

    @classmethod
    def length_payload_timestamp(cls):
        return (struct.calcsize(PennNanoslice.format_payload_header) + struct.calcsize(PennNanoslice.format_payload_timestamp)) / struct.calcsize("<c")


    def __init__(self, mode = 0, nticks_per_nanoslice = 10, sequence = 0):
        self.mode = mode
        self.nticks_per_nanoslice = nticks_per_nanoslice
        self.sequence = sequence
        self.packed = False

    def create_payload_counter(self):
        #This is just a long list of bit values

        if self.mode == 0:    # All off
            payload = "0"  * PennNanoslice.num_values_counter
            
        elif self.mode == 1:  # All on
            payload = "1"  * PennNanoslice.num_values_counter
            
        elif self.mode == 2:  # Alternating on/off
            payload = "01" * (PennNanoslice.num_values_counter / 2)
            
        elif self.mode == 3:  # Random values across full bit-range interval  
            maxVal = 1
            payload = ''.join(str(random.randint(0, maxVal)) for i in xrange(PennNanoslice.num_values_counter))
            
        else:
            print 'Unknown mode:', self.mode
            sys.exit(1)

        return self.create_payload_header('c') + bin_to_char(payload)
       
    def create_payload_trigger(self):
        #This is just a long list of bit values

        if self.mode == 0:    # No triggers
            payload = "0" * PennNanoslice.num_values_trigger

        elif self.mode == 1:  # All on
            payload = "1"  * PennNanoslice.num_values_trigger
            
        elif self.mode == 2:  # Alternating on/off
            payload = "01" * (PennNanoslice.num_values_trigger / 2)
            
        elif self.mode == 3:  # A single random trigger
            payload = "0" * PennNanoslice.num_values_trigger
            l = list(payload)
            l[random.randint(0, len(l) - 1)] = "1"
            payload = ''.join(l)

        else:
            print 'Unknown mode:', self.mode
            sys.exit(1)

        return self.create_payload_header('t') + bin_to_char(payload)


    def create_payload_timestamp(self):
        #Get the NOvA timestamp & convert it to bits, then to char's

        #print (self.time.timestamp_high << 32) |  self.time.timestamp_low
        #print self.time.ticks_since_epoch
        low  = self.time.timestamp_low
        high = self.time.timestamp_high
        time64 = (high << 32) | low
        bin64  = bin(time64)[2:].zfill(64)
        return self.create_payload_header('s') + bin_to_char(bin64)

    def create_payload_header(self, dtype):
        #concatanate 4 bit data type & 28 bit timestamp

        if dtype == 'c':
            value = '0001'
        elif dtype == 't':
            value = '0010'
        elif dtype == 's': #for timeStamp
            value = '1000'
        else:
            print 'Unknown data type:', dtype
            sys.exit(1)

        time = bin(self.time.timestamp_short)[2:].zfill(28)

        return bin_to_char(value + time)

    def create_header(self, nbytes):
        version  = simple_twos_complement(bin(PennNanoslice.version)[2:].zfill(4)) + bin(PennNanoslice.version)[2:].zfill(4)

        sequence = bin(self.sequence)[2:].zfill(8)

        block_size = bin(nbytes)[2:].zfill(16)

        #print "header string"
        #print version, sequence, block_size
        #print_string_as_bin(bin_to_char(version + sequence + block_size))

        return bin_to_char(version + sequence + block_size)

    def pack(self):

        data  = ''
        nchar = 0
        self.time = 0

        #get the data words
        for i in xrange(self.nticks_per_nanoslice):
            self.time = NovaTimestamp(None)
            data  += self.create_payload_counter()
            nchar += int(PennNanoslice.format_payload_counter[1:-1]) + int(PennNanoslice.format_payload_header[1:-1])
            if random.randint(0,1):
                data += self.create_payload_trigger()
                nchar += int(PennNanoslice.format_payload_trigger[1:-1]) + int(PennNanoslice.format_payload_header[1:-1])

        #finish with the timestamp
        if not self.time:
            self.time = NovaTimestamp(None)
        data += self.create_payload_timestamp()
        nchar += int(PennNanoslice.format_payload_timestamp[1:-1]) + int(PennNanoslice.format_payload_header[1:-1])

        #and prepend the header (need to know block size before making header)
        nchar += int(PennNanoslice.format_header[1:-1])
        data   = self.create_header(nchar) + data
        self.format = '<'+str(nchar)+'c'

        #pack it up
        self.packed = True
        #print len(data), '=', len(self.create_header(0)), '+', len(self.create_payload_counter()), '+', len(self.create_payload_timestamp())
        #print len(self.create_payload_header('c'))
        self.packed_data = struct.pack(self.format, *data)
        return self.packed_data

    def size(self):
        if not self.packed:
            print 'Require pack() to be run before size() is known'
            sys.exit(1)
        return struct.calcsize(self.format) / struct.calcsize('<c')

    def print_nanoslice(self):
        if not self.packed:
            print 'Require pack() to be run before it can be unpacked & printed'
            sys.exit(1)
        data = struct.unpack(self.format, self.packed_data)

        #header
        print 'version',
        print_string_as_bin(data[0])
        print 'sequence id',
        print_string_as_bin(data[1])
        print 'block size',
        print_string_as_bin(data[2:4])
        
        i = 4
        while True:
            mode = print_payload_header(data[i:i+4])
            i += 4
            if mode == '0001':
                print_payload(data[i:i+PennNanoslice.num_values_counter/8])
                i += PennNanoslice.num_values_counter/8
            elif mode == '0010':
                print_payload(data[i:i+PennNanoslice.num_values_trigger/8])
                i += PennNanoslice.num_values_trigger/8
            elif mode == '1000':
                print_payload(data[i:i+8])
                break
        print
        #print_string_as_bin(data)


class PennMicroslice(object):

    #ASSUME 8 bit(1c) version + 16 bit(2c) sequence + 16 bit(2c) run mode + 64 bit(8c) full timestamp + padding to get to 16c
    header_format = '<16c'

    @classmethod
    def header_length_words(cls):
        return struct.calcsize(PennMicroslice.header_format) / struct.calcsize("<c")

    def __init__(self, sequence_id = 0):

        self.record_length = PennMicroslice.header_length_words()

        self.version     = 0x01
        self.run_mode    = 0x0010
        self.sequence_id = sequence_id
        self.time        = NovaTimestamp(None)
        self.timestamp   = (self.time.timestamp_high << 32) |  self.time.timestamp_low
        
        self.num_nanoslices = 0;       
        self.nanoslices = []
        self.packed_nanoslices = ''
        
    def generate_nanoslices(self, num_nanoslices, mode = 0, nticks_per_nanoslice = 10):
        if (num_nanoslices % 4) != 0:
            print "Illegal number of nanoslices requested"
            sys.exit(1)
        else:
            for ins in xrange(num_nanoslices):
                if nticks_per_nanoslice >= 0:
                    nslice = PennNanoslice(mode, nticks_per_nanoslice, ins)
                else:
                    nslice = PennNanoslice(mode, random.randint(0, -nticks_per_nanoslice), ins)
                self.nanoslices.append(nslice)
            
            self.num_nanoslices = num_nanoslices
            
            for nslice in self.nanoslices:
                self.packed_nanoslices += nslice.pack()
                self.record_length     += nslice.size()

    def set_sequence_id(self, sequence_id=0):
        self.sequence_id = sequence_id

    def set_timestamp(self, posix_time = None):
        self.timestamp = NovaTimestamp(posix_time).timestamp_low

    def pack(self):

        #ASSUME 8 bit(1c) version + 16 bit(2c) sequence + 16 bit(2c) run mode + 64 bit(8c) full timestamp + padding to get to 16c

        version     = bin(~self.version)[3:].zfill(4) + bin(self.version)[2:].zfill(4)
        sequence_id = bin(self.sequence_id)[2:].zfill(16)
        run_mode    = bin(self.run_mode)[2:].zfill(16)
        timestamp   = bin(self.timestamp)[2:].zfill(64)

        header_word = version + sequence_id + run_mode + timestamp

        padding     = '0' * ((int(PennMicroslice.header_format[1:-1]) * 8) - len(header_word))
        header_word += padding

        header_word = bin_to_char(header_word)

        packed_header = struct.pack(PennMicroslice.header_format, *header_word)
        
        return packed_header + self.packed_nanoslices

        
if __name__ == '__main__':

    import binascii
    import math

    print "PENN nanoslice header has a length of", PennNanoslice.length_header(), "chars"
    nslice = PennNanoslice(mode = 2, nticks_per_nanoslice = 10, sequence = 0)
    packed_nslice = nslice.pack()
    print "Packed nanoslice has length", len(packed_nslice), "bytes, contents:", binascii.hexlify(packed_nslice)
    
    nslice.print_nanoslice()

    print ""
    
    """
    print "PENN microslice header has a length of", PennMicroslice.header_length_words(), "chars"   
    mslice = PennMicroslice(123)
    packed_mslice = mslice.pack()
    print "Packed microslice header has length", len(packed_mslice) , "bytes, contents:", binascii.hexlify(packed_mslice)
   
    num_nano = 32
    mslice.generate_nanoslices(num_nano, mode = 0, nticks_per_nanoslice = 5)
    
    print "Microslice filled with", num_nano, "nanoslices reports a record length of", mslice.record_length, "words"

    packed_mslice = mslice.pack()
 
    print "Packed microslice has length", len(packed_mslice), "bytes"#, "contents:", binascii.hexlify(packed_mslice)
    """

