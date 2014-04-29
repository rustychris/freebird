"""
Parse binary output files from freebird logger
"""
import os
import numpy as np

class FreebirdBlock(object):
    ver=0
    def __init__(self,block):
        """
        block: a 512-byte string
        """
        # size of valid data in units of 16-bit words
        self.count=np.fromstring(block[:2],dtype=np.uint16)[0]
        self.overrun=np.fromstring(block[2],dtype=np.uint8)[0]
        # location of the first frame boundary in this block
        # in units of 16-bit words
        self.frame_offset=np.fromstring(block[3],dtype=np.uint8)[0]
        
        self.raw = block[4:]
        self.data=self.raw[:2*self.count]
        
class FreebirdFile(object):
    block_Nbytes = 512 # matches SD block size
    
    log_imu=True
    log_adc=True
    sample_interval_us=2000
    
    def __init__(self,filename,startup=None):
        self.filename=filename
        if startup is not None:
            self.parse_startup(startup)

    def parse_startup(self,fn):
        with open(fn,'rt') as fp:
            for line in fp:
                if '=' in line:
                    key,val = line.split('=',maxsplit=1)
                    key=key.strip()
                    val=val.strip()
                    
                    if key=='log_imu':
                        # mimics logic of logger.cpp
                        self.log_imu=val[0]!='0'
                    elif key=='log_adc':
                        self.log_adc=val[0]!='0'
                    elif key=='sample_interval_us':
                        self.sample_interval_us=float(val)
                         
    def nblocks(self):
        return os.stat(self.filename).st_size // self.block_Nbytes

    def read_block(self,block_num):
        """ bytes for a block
        """
        with open(self.filename,'rb') as fp:
            fp.seek(block_num*self.block_Nbytes)
            return FreebirdBlock(fp.read(self.block_Nbytes))

    def parse_all(self):
        data=[]
        count=0 # running total of 16-bit words 
        frames=[] # frame boundaries, in 16-bit words
        for blk_i in range(self.nblocks()):
            blk=self.read_block(blk_i)
            if blk.frame_offset!=255:
                frames.append(count+blk.frame_offset)
            data.append(blk.data)
            count+=blk.count

        self.frames=frames
        self.count=count
        self.full_stream="".join(data)

    def dtype(self):
        flds=[]
        if self.log_adc:
            flds.append( ('adc','<i2') )
        if self.log_imu:
            flds.append( ('a','<i2',3) )
            flds.append( ('g','<i2',3) )
            flds.append( ('m','<i2',3) )
        return flds
    
    def data(self):
        self.parse_all()
        stream=self.full_stream[2*self.frames[0]:2*self.frames[-1]]

        return np.fromstring(stream,self.dtype())

        
        
