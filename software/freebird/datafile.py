"""
Parse binary output files from freebird logger
"""
import os
import numpy as np
import re
from numpy.lib import recfunctions
import datetime
from matplotlib.dates import date2num
from contextlib import contextmanager
from collections import namedtuple
import array_append
import derived

def freebird_file_factory(filename):
    """ figures out the version of the given file, and returns
    an object ready to parse it
    """
    return FreebirdFile0001(filename)

class FreebirdFile(object):
    """ [will eventually] define the interface for
    freebird files
    """
    pass

class FreebirdFile0001(FreebirdFile):
    block_Nbytes = 512 # matches SD block size
    
    sample_interval_us=2000
    
    def __init__(self,filename):
        self.filename=filename
        self.fp=None
        self.data_block_start=None
        self.Block=namedtuple('Block',['header','data','text'])
        self.serials={}
        # for long running operations, the method can be run in a thread,
        # and will attempt to update progress with a fractional estimate of
        # it's progress.  
        self.progress=0.0
        self.data=None
        
    @property
    def nblocks(self):
        return os.stat(self.filename).st_size // self.block_Nbytes
    @property
    def nbytes(self):
        return os.stat(self.filename).st_size 

    @contextmanager
    def opened(self):
        """ Because the data files are often on removeable media, and
        keeping a file open might be a headache when trying to remove
        the media, extra care is taken to only have the file open while
        it is actively read.

        Any access to self.fp should be through the opened context manager,
        which can be nested if necessary
        """
        if self.fp is None:
            self.fp=open(self.filename,'rb')
            yield self.fp
            self.fp.close()
            self.fp=None
        else:
            yield self.fp
        
    def read_block(self,block_num):
        """ bytes for a block
        """
        with self.opened() as fp:
            return self.open_read_block(block_num)
    def open_read_block(self,block_num):
        self.fp.seek(block_num*self.block_Nbytes)
        buff=self.fp.read(self.block_Nbytes)
        if len(buff)!=self.block_Nbytes:
            return None
        return self.parse_block(buff)

    FLAG_TYPE_MASK=1
    FLAG_TYPE_DATA=0
    FLAG_TYPE_TEXT=1
    FLAG_OVERRUN=2
    header_dtype=np.dtype([('unixtime','<u4'),
                           ('ticks','<u2'),
                           ('frame_count','u1'),
                           ('flags','u1')])
    frame_dtype=None
    frame_bytes=None
    def parse_block(self,buff):
        header_bytes=self.header_dtype.itemsize
        header=np.fromstring(buff[:header_bytes],dtype=self.header_dtype)[0]
        
        flag_type=header['flags']&self.FLAG_TYPE_MASK
        if flag_type==self.FLAG_TYPE_TEXT:
            data=None
            #
            rest=buff[header_bytes:]
            eos=rest.find("\0")
            if eos<0:
                print "Warning - text block was not properly terminated"
                text=rest
            else:
                text=rest[:eos]
        elif flag_type==self.FLAG_TYPE_DATA:
            text=None
            #
            if self.frame_dtype is not None:
                frame_count=header['frame_count']
                trimmed=buff[header_bytes:header_bytes+frame_count*self.frame_bytes]
                data=np.fromstring(trimmed,dtype=self.frame_dtype)
            else:
                data="frame dtype unavailable"
        else:
            raise Exception("Not a valid type for block")
        return self.Block(header=header,text=text,data=data)
        
    def read_header(self):
        with self.opened():
            return self.open_read_header()

    def open_read_header(self):
        blk_i=0
        header_texts=[]
        for blk_i in range(self.nblocks):
            blk=self.read_block(blk_i)
            if blk is None:
                break
            if (blk.header['flags']&self.FLAG_TYPE_MASK)==self.FLAG_TYPE_TEXT:
                header_texts.append(blk.text)
            else:
                self.data_block_start=blk_i
                break
            blk_i+=1
        return "".join(header_texts)
    
    def read_all(self):
        """ populate header text, parse that to header data,
        process the raw data, and apply any postprocess steps necessary
        """
        with self.opened():
            self.header_text=self.open_read_header()
            self.header_data=self.parse_header(self.header_text)
            return self.open_read_data()

    @classmethod
    def parse_header(klass,header_text):
        lines=re.split(r'[\r\n]+',header_text)
        kvs=[]
        for line in lines:
            if line=="":
                continue
            keyval=re.split(r':\s*',line,maxsplit=1)
            if keyval and len(keyval)==2:
                key,val = keyval
                kvs.append( (key,val) )
            else:
                print "Skipping line:",line
        return dict(kvs)
        
    def read_data(self):
        with self.opened():
            return self.open_read_data()

    def open_read_data(self):
        self.frame_format=eval(self.header_data['frame_format'])
        self.frame_dtype=np.dtype(self.frame_format)
        self.frame_bytes=self.frame_dtype.itemsize

        if 'ticks_per_second' in self.header_data:
            ticks_per_second=self.header_data['ticks_per_second']
        elif 'rtc_timer_freq_hz' in self.header_data:
            ticks_per_second=self.header_data['rtc_timer_freq_hz']
        else:
            ticks_per_second='n/a'
        try:
            ticks_per_second=float(ticks_per_second)
        except ValueError:
            ticks_per_second=None
        
        self.frames=[]
        # np.datetime64 is less lossy, but dnums have better support in matplotlib
        # and readily convertible to matlab (offset of 366, not sure which way)
        self.timestamps=[]

        
        for blk_i in range(self.data_block_start,self.nblocks):
            # 0.9, because the stitching together at the end takes some time
            # but doesn't update progress
            self.progress=0.9*float(blk_i)/self.nblocks
            
            blk=self.open_read_block(blk_i)
            self.frames.append(blk.data)

            hdr=blk.header
            unixtime=hdr['unixtime']
            microsecs=int( float(hdr['ticks']) * 1e6 / ticks_per_second )
            # constructed as a UTC time, though printing often converts to local
            # time.
            # this will upcast to int64, with plenty of room.  note that the
            # argument to datetime64 must be an integer, no floating point.
            
            # this works in numpy 1.8, but not 1.7:
            # dt64=np.datetime64(1000000*hdr['unixtime'] + microsecs,'us')

            # in 1.7, it's pretty picky about casts, but this appears to work:
            dt64=np.datetime64('1970-01-01 00:00:00')
            dt64=dt64+np.timedelta64(int(hdr['unixtime']),'s')
            dt64=dt64+np.timedelta64(microsecs,'us')
            
            self.timestamps.append( dt64 )
            
        basic_data = np.concatenate(self.frames)

        data_and_time=self.add_timestamps(basic_data)
        self.data=self.add_derived_data(data_and_time)
        return self.data

    def add_timestamps(self,basic_data):
        expanded=[]
        dt_us=1000000/float(self.header_data['sample_rate_hz'])
        for timestamp,frame in zip(self.timestamps,self.frames):
            # make sure timestamp is in microseconds, so we can add more microseconds
            expanded.append(timestamp.astype('<M8[us]') + (np.arange(len(frame))*dt_us).astype(np.int64))
        full_datetimes=np.concatenate(expanded)
        full_pydnums=full_datetimes.astype('int64')/86400.0e6 # into days...
        # fix offset
        dnum0=date2num(self.timestamps[0].astype(datetime.datetime))
        full_pydnums+= dnum0-full_pydnums[0]

        new_data=array_append.recarray_add_fields(basic_data,
                                                  [('timestamp',full_datetimes),
                                                   ('dn_py',full_pydnums),
                                                   ('dn_mat',full_pydnums+366)])
        return new_data

    @property
    def freebird_serial(self):
        return header_data['teensy_uid']
    
    def postprocessors(self):
        derived.Calibration.load_directory(os.path.dirname(self.filename))

        posts=[]
        if 'squid' in self.serials:
            posts.append( derived.SquidPostprocess(squid_serial=self.serials['squid'],
                                                   sbe7probe_serial=self.serials['sbe7probe'],
                                                   freebird_serial=self.freebird_serial) )
            
        return posts
    
    def add_derived_data(self,data):
        # this may need to get reorganized...
        new_fields=[]
        for post in self.postprocessors():
            new_fields+=post.postprocess(self,data)

        # units=[nf[2] for nf in new_fields] # for now, discard units
        to_add=[nf[:2] for nf in new_fields]
        
        return array_append.recarray_add_fields(data,to_add)

    def sample_rate_hz(self):
        # from the data: 1e6/(diff(data['timestamp'].astype('i8')).mean())
        # from the settings:
        return float(self.header_data['sample_rate_hz'])
