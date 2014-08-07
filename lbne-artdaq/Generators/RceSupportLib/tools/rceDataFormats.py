import struct
import random
import math
import datetime
import time


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
            
        self.since_epoch = (self.now - NovaTimestamp.epoch).total_seconds()
        self.ticks_since_epoch = int(self.since_epoch * NovaTimestamp.ticks_per_sec)
        self.timestamp_low = self.ticks_since_epoch & 0xFFFFFFFF
        self.timestamp_high = (self.ticks_since_epoch >> 32) & 0xFFFFFF
        
class RceNanoslice(object):

    format = '<12I'
    num_adcs = 32
    adc_width = 12

    @classmethod
    def length_words(cls):
        return struct.calcsize(RceNanoslice.format) / struct.calcsize("<I")

    def __init__(self, adc_mode=0, mean=1000, sigma=100):

        if adc_mode == 0:    # Incrementing ADCs
            self.adc_vals = [x for x in xrange(RceNanoslice.num_adcs)]
            
        elif adc_mode == 1:  # Decrementing ADCs from full scale
            self.adc_vals  = [(4095 - x) for x in xrange(RceNanoslice.num_adcs)]
        
        elif adc_mode == 2:  # Decrementing ADCS across high nibble bit boundary
            self.adc_vals  = [(1030 - x) for x in xrange(RceNanoslice.num_adcs)]
            
        elif adc_mode == 3:  # Random ADC values across full bit-range interval  
            maxVal = (2**RceNanoslice.adc_width) - 1
            self.adc_vals = [random.randint(0, maxVal) for x in xrange(RceNanoslice.num_adcs)]
            
        else:               # Gaussian distribution with specified mean and sigma, clipped to ADC range
            self.adc_vals = [(int(random.gauss(mean,sigma)) & 0xFFFFFF) for x in xrange(RceNanoslice.num_adcs)]

    def pack(self):

         # Pack ADCs into 12 32-bit words according to scheme suggested by JJ -
        # low bytes of all ADCs in first 8 words, followed by high nibble of all
        # ADCs in last 4, giving a total of 12 words
        packedAdcs = [0] * RceNanoslice.adc_width

        for adc in range(RceNanoslice.num_adcs):
            lowByteOffset    = adc / 4
            lowByteShift     = (adc % 4) * 8
            highNibbleOffset = (adc / 8) + 8
            highNibbleShift  = (adc % 8) * 4
            packedAdcs[lowByteOffset] |= ((self.adc_vals[adc] & 0xFF) << lowByteShift)
            packedAdcs[highNibbleOffset] |= ((self.adc_vals[adc] & 0xF00) >> 8) << highNibbleShift

        return struct.pack(RceNanoslice.format, *packedAdcs)

class RceMicroslice(object):

    #header_format = '<IIBHBBBB'
    header_format = "<IIIIII"
    num_lanes = 4

    @classmethod
    def header_length_words(cls):

        return struct.calcsize(RceMicroslice.header_format) / struct.calcsize("<I")

    def __init__(self, sequence_id = 0):

        self.record_length = RceMicroslice.header_length_words()
        self.reserved_0 = 0
        self.record_type = 2 # data record
        self.format= 0
        self.timestamp = NovaTimestamp().timestamp_low
        
        self.sub_type = 2
        self.trig_type = 0
        self.data_format = 1
        self.reserved_1 = 0
        self.num_chans = 4
        self.num_times = 0 
        self.reserved_2 = 0
        
        self.physical_id = [0, 1, 2, 3]

        self.sequence_id = sequence_id
         
        self.num_nanoslices = 0;       
        self.nanoslices = []
        self.packed_nanoslices = ""
        
    def generate_nanoslices(self, num_nanoslices, adc_mode=0, mean=1000, sigma=100):

        if (num_nanoslices % 32) != 0:
            print "Illegal number of nanoslices requested"
        else:
            for nslice in xrange(num_nanoslices):
                for lane in xrange(RceMicroslice.num_lanes):
                    self.nanoslices.append(RceNanoslice(adc_mode, mean, sigma))
                    self.record_length += RceNanoslice.length_words()                
            
            self.num_nanoslices = num_nanoslices
            
            for nslice in self.nanoslices:
                self.packed_nanoslices += nslice.pack()

    def set_sequence_id(self, sequence_id=0):
        
        self.sequence_id = sequence_id
        
    def set_timestamp(self, posix_time = None):
        
        self.timestamp = NovaTimestamp(posix_time).timestamp_low
        
    def pack(self):

        #block_type_version = self.block_version << 4 | self.block_type
        
        now = time.time()
        if (self.num_nanoslices): 
            self.num_times = int(math.log(self.num_nanoslices)/math.log(2))

        header_word = [0] * RceMicroslice.header_length_words()
        header_word[0] = ((self.record_length & 0xFFFFF) <<  0) | ((self.reserved_0 & 0xF) << 20) | \
                         ((self.record_type   &     0xF) << 24) | ((self.format     & 0xF) << 28)
        header_word[1] = self.timestamp
        header_word[2] = ((self.sub_type    &  0xF) <<  0) | ((self.trig_type   & 0xF) <<  4) | \
                         ((self.data_format &  0xF) <<  8) | ((self.reserved_0  & 0xF) << 12) | \
                         ((self.num_chans   &  0xF) << 16) | ((self.num_times   & 0xF) << 20) | \
                         ((self.reserved_2  & 0xFF) << 24)
        header_word[3] = ((self.physical_id[0] & 0xFFFF) << 0) | ((self.physical_id[1] & 0xFFFF) << 16)
        header_word[4] = ((self.physical_id[2] & 0xFFFF) << 0) | ((self.physical_id[3] & 0xFFFF) << 16)
        header_word[5] = self.sequence_id
        
        packed_header = struct.pack(RceMicroslice.header_format, *header_word)
        
        return packed_header + self.packed_nanoslices

        
if __name__ == '__main__':

    import binascii
    import math

    print "RCE nanoslice has a length of", RceNanoslice.length_words(), "words"
    nslice = RceNanoslice(4)
    packed_nslice = nslice.pack()
    print "Packed nanoslice has length", len(packed_nslice), "bytes, contents:", binascii.hexlify(packed_nslice)
    
    print ""
    
    print "RCE microslice header has a length of", RceMicroslice.header_length_words(), "words"   
    mslice = RceMicroslice(123)
    packed_mslice = mslice.pack()
    print "Packed microslice header has length", len(packed_mslice) , "bytes, contents:", binascii.hexlify(packed_mslice)
   
    num_nano = 32
    mslice.generate_nanoslices(num_nano, 4, 1000, 100)
    
    print "Microslice filled with", num_nano, "nanoslices reports a record length of", mslice.record_length, "words"
    
    adc_sum = 0
    adc_sum_sq = 0
    num_adcs = 0
    for nslice in mslice.nanoslices:
        adc_sum += sum(nslice.adc_vals)
        adc_sum_sq  += sum([val**2 for val in nslice.adc_vals])
        num_adcs += len(nslice.adc_vals)
 
    mean = float(adc_sum) / num_adcs
    mean_sq = float(adc_sum_sq) / num_adcs
    stddev = math.sqrt(mean_sq - (mean**2))
 
    print "Microslice has", num_adcs, "total ADC values, with a mean value of", mean, "std dev of", stddev
 
    packed_mslice = mslice.pack()
 
    print "Packed microslice has length", len(packed_mslice), "bytes"  #, "contents:", binascii.hexlify(packed_mslice)
