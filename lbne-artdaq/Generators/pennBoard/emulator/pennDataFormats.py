import struct
import random
import math
import datetime
import time
import sys
import binascii


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

def string_to_bin(s):
    b = ''
    for c in s:
        h = binascii.hexlify(c)
        b += bin(int(h,16))[2:].zfill(8)
    return b

def print_payload_header(s):
    b = string_to_bin(s)
    print "mode", b[:4]
    print "28-bit timestamp", int(b[4:], 2), b[4:]
    return b[:4]

def print_payload(s):
    b = ''
    for c in s:
        h = binascii.hexlify(c)
        b +=  bin(int(h,16))[2:].zfill(8)
        b += ' '
    print 'payload', b

class NovaTimestamp(object):
    
    # NOvA epoch is 00:00:00 GMT 1st Jan, 2000 
    epoch = datetime.datetime(2000, 1, 1, 0, 0, 0, 0)
    
    # NOvA clock is 64 MHZ, = 15.6ns tick
    ticks_per_sec = 64E6
    
    #NOvA clock is 56 bits

    def __init__(self, posix_time=None, override=None):
        
        if posix_time == None:
            self.now = datetime.datetime.now()
        else:
            self.now = datetime.datetime.fromtimestamp(posix_time)
            
        self.since_epoch       = (self.now - NovaTimestamp.epoch).total_seconds()
        self.ticks_since_epoch = int(self.since_epoch * NovaTimestamp.ticks_per_sec)
        if(override):
            self.ticks_since_epoch = override
        self.epoch_to_timestamp()

    def epoch_to_timestamp(self):
        self.timestamp_low     = self.ticks_since_epoch & 0xFFFFFFFF #32 bits
        self.timestamp_high    = (self.ticks_since_epoch >> 32) & 0xFFFFFF #24 bits
        self.timestamp_short   = self.ticks_since_epoch & 0xFFFFFFF #28 bits

    def increment(self):
        self.ticks_since_epoch += 1
        self.epoch_to_timestamp()

        
class PennMicroslice(object):

    #ASSUME little-endian
    format_header            = '<4c'   #32 bit header = 8 bit version + 8 bit sequence + 16 bit block size(bytes)
    format_payload_header    = '<4c'   #32 bit header = 4 bit type + 28 bit (partial) timestamp
    #ASSUME 96 bit data
    format_payload_counter   = '<12c'  #96 bit data
    #ASSUME 32 bit data
    format_payload_trigger   = '<4c'   #32 bit data
    format_payload_timestamp = '<8c'   #64 bit data

    num_values_counter   = 8 * int(format_payload_counter[1:-1])
    num_values_trigger   = 8 * int(format_payload_trigger[1:-1])
    num_values_timestamp = 8 * int(format_payload_timestamp[1:-1])

    version            = 0x0

    @classmethod
    def length_header(cls):
        return struct.calcsize(PennMicroslice.format_header) / struct.calcsize("<c")

    @classmethod
    def length_payload_counter(cls):
        return (struct.calcsize(PennMicroslice.format_payload_header) + struct.calcsize(PennMicroslice.format_payload_counter  )) / struct.calcsize("<c")

    @classmethod
    def length_payload_trigger(cls):
        return (struct.calcsize(PennMicroslice.format_payload_header) + struct.calcsize(PennMicroslice.format_payload_trigger  )) / struct.calcsize("<c")

    @classmethod
    def length_payload_timestamp(cls):
        return (struct.calcsize(PennMicroslice.format_payload_header) + struct.calcsize(PennMicroslice.format_payload_timestamp)) / struct.calcsize("<c")


    def __init__(self, payload_mode = 0, trigger_mode = 0, nticks_per_microslice = 10, sequence = 0, fragment_microslice_at_ticks = -1):
        self.payload_mode = payload_mode
        self.trigger_mode = trigger_mode
        self.nticks_per_microslice = nticks_per_microslice
        self.fragment_microslice_at_ticks = fragment_microslice_at_ticks
        self.sequence = sequence
        self.packed = False
        self.time = 0

    def set_sequence_id(self, sequence):
        self.sequence = sequence % 256

    def create_payload_counter(self):
        #This is just a long list of bit values

        if self.payload_mode == 0:    # All off
            payload = "0"  * PennMicroslice.num_values_counter
            
        elif self.payload_mode == 1:  # All on
            payload = "1"  * PennMicroslice.num_values_counter
            
        elif self.payload_mode == 2:  # Alternating on/off
            payload = "01" * (PennMicroslice.num_values_counter / 2)
            
        elif self.payload_mode == 3:  # Random values across full bit-range interval  
            maxVal = 1
            payload = ''.join(str(random.randint(0, maxVal)) for i in xrange(PennMicroslice.num_values_counter))

        elif self.payload_mode == 4:  # Random values only in 8 bits
            maxVal = 1
            payload = ''.join(str(1 if (random.random() > 0.9) else 0) for i in xrange(8)) + "0"  * (PennMicroslice.num_values_counter - 8)
            if not payload.count("1", 0, 8):
                return False, 0
            
        else:
            print 'Unknown payload_mode:', self.payload_mode
            sys.exit(1)

        return self.create_payload_header('c') + bin_to_char(payload), payload.count("1", 0, 8)
       
    def create_payload_trigger(self, nhits = 0):
        #This is just a long list of bit values

        if self.payload_mode == 0:    # No triggers
            payload = "0" * PennMicroslice.num_values_trigger

        elif self.payload_mode == 1:  # All on
            payload = "1"  * PennMicroslice.num_values_trigger
            
        elif self.payload_mode == 2:  # Alternating on/off
            payload = "01" * (PennMicroslice.num_values_trigger / 2)
            
        elif self.payload_mode == 3:  # A single random trigger
            payload = "0" * PennMicroslice.num_values_trigger
            l = list(payload)
            l[random.randint(0, len(l) - 1)] = "1"
            payload = ''.join(l)

        elif self.payload_mode == 4:  # Use the number of hits to create a trigger word
            if nhits > 1:
                payload = bin(nhits)[2:].zfill(PennMicroslice.num_values_trigger)
            else:
                return False

        else:
            print 'Unknown payload_mode:', self.payload_mode
            sys.exit(1)

        return self.create_payload_header('t') + bin_to_char(payload)


    def create_payload_timestamp(self):
        #Get the NOvA timestamp & convert it to bits, then to char's

        #print (self.time.timestamp_high << 32) |  self.time.timestamp_low
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
        version  = simple_twos_complement(bin(PennMicroslice.version)[2:].zfill(4)) + bin(PennMicroslice.version)[2:].zfill(4)

        sequence = bin(self.sequence)[2:].zfill(8)

        block_size = bin(nbytes)[2:].zfill(16)
        if len(block_size) > 16:
            print "FATAL ERROR. Block created with block size greater than 16 bits. nbytes is ", nbytes
            sys.exit(1)

        #print "header string"
        #print version, sequence, block_size
        #print_string_as_bin(bin_to_char(version + sequence + block_size))

        return bin_to_char(version + sequence + block_size)

    def pack(self):

        data  = ''
        totaldata = ''
        nchar = 0
        totalnchar = 0

        #negative nticks_per_microslice means random number of data words
        nticks = self.nticks_per_microslice
        if nticks <= 0:
            nticks = random.randint(1, max(abs(nticks),1))

        #get the data words
        for i in xrange(nticks):
            if not self.time:
                #self.time = NovaTimestamp(None)
                self.time = NovaTimestamp(None, (1 << 29) - 10) #test the 28-bit rollover
            else:
                self.time.increment()
            #add the counter word
            counter, nhits = self.create_payload_counter()
            if counter:
                data  += counter
                nchar += int(PennMicroslice.format_payload_counter[1:-1]) + int(PennMicroslice.format_payload_header[1:-1])
                #add the trigger word
                if self.trigger_mode == 1 or (random.randint(0,1) and self.trigger_mode == 2):
                    trigger = self.create_payload_trigger(nhits)
                    if trigger:
                        data += trigger
                        nchar += int(PennMicroslice.format_payload_trigger[1:-1]) + int(PennMicroslice.format_payload_header[1:-1])
                    
            #add a header, and reset counters, if we've got too many ticks & have to make a fragmented block (or if a block is too big)
            if ((self.fragment_microslice_at_ticks > 0 and i > 0 and (i % self.fragment_microslice_at_ticks) == self.fragment_microslice_at_ticks - 1) and i != nticks - 1) or (nchar + int(PennMicroslice.format_header[1:-1]) + 20 >= 65535):
                nchar += int(PennMicroslice.format_header[1:-1])
                data   = self.create_header(nchar) + data
                totalnchar += nchar
                totaldata  += data
                nchar = 0
                data  = ''

        #finish with the timestamp
        if not self.time:
            self.time = NovaTimestamp(None)
        data += self.create_payload_timestamp()
        nchar += int(PennMicroslice.format_payload_timestamp[1:-1]) + int(PennMicroslice.format_payload_header[1:-1])

        #and prepend the header (need to know block size before making header)
        nchar += int(PennMicroslice.format_header[1:-1])
        data   = self.create_header(nchar) + data

        #account for any possible data from fragmented uslices
        totalnchar += nchar
        totaldata  += data

        #pack it up
        self.format = '<'+str(totalnchar)+'c'
        self.packed = True
        #print len(data), '=', len(self.create_header(0)), '+', len(self.create_payload_counter()), '+', len(self.create_payload_timestamp())
        #print len(self.create_payload_header('c'))
        self.packed_data = struct.pack(self.format, *totaldata)
        return self.packed_data

    def size(self):
        if not self.packed:
            print 'Require pack() to be run before size() is known'
            sys.exit(1)
        return struct.calcsize(self.format) / struct.calcsize('<c')

    def print_microslice(self, only_header = False):
        if not self.packed:
            print 'Require pack() to be run before it can be unpacked & printed'
            sys.exit(1)
        data = struct.unpack(self.format, self.packed_data)

        while data:

            #header
            print 'version',
            print_string_as_bin(data[0])
            print 'sequence id',
            print_string_as_bin(data[1])
            print 'block size',
            blocksizebin = string_to_bin(data[2:4])
            blocksize = int(blocksizebin, 2)
            print blocksize, blocksizebin
            
            #get the microslice contents (to be able to print it)
            contents = data[4:blocksize]
            #remove this microslice from the data (for fragmented blocks)
            data = data[blocksize:]

            if only_header:
                continue
        
            while contents:
                mode = print_payload_header(contents[:4])
                contents = contents[4:]
                if mode == '0001':
                    print_payload(contents[:PennMicroslice.num_values_counter/8])
                    contents = contents[PennMicroslice.num_values_counter/8:]
                elif mode == '0010':
                    print_payload(contents[:PennMicroslice.num_values_trigger/8])
                    contents = contents[PennMicroslice.num_values_trigger/8:]
                elif mode == '1000':
                    print_payload(contents[:PennMicroslice.num_values_timestamp/8])
                    contents = contents[PennMicroslice.num_values_timestamp/8:]

            #blank line to separate microslices
            print
        #print_string_as_bin(data)


if __name__ == '__main__':

    #create a single microslice and print all information
    print "PENN microslice header has a length of", PennMicroslice.length_header(), "chars"
    uslice = PennMicroslice(payload_mode = 4, trigger_mode = 4, nticks_per_microslice = 100, sequence = 0, fragment_microslice_at_ticks = 0)
    packed_uslice = uslice.pack()
    print "Packed microslice has length", len(packed_uslice), "bytes, contents:", binascii.hexlify(packed_uslice)
    uslice.print_microslice(only_header = False)
    #uslice.print_microslice(only_header = True)

    #sys.exit(1)

    print ""

    #create lots of microslices, printing only the header
    i = 0
    while i < 10:
    #while True:
        print "Microslice", i
        uslice = PennMicroslice(payload_mode = 2, trigger_mode = 2, nticks_per_microslice = -20, sequence = i % 256, fragment_microslice_at_ticks = 10)
        packed_uslice = uslice.pack()
        uslice.print_microslice(only_header = True)
        i += 1
