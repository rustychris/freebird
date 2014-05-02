import serial
import os
import sys
import glob
import serial
import serial.tools.list_ports
import time
import logging
import datetime
import weakref
import threading

# place holder decorators, for now just to document
def req_connected(f):
    return f
def req_cmdmode(f):
    return f

import collections
class TextRing(collections.deque):
    def __init__(self,maxlen):
        super(TextRing,self).__init__(maxlen=maxlen)
        self.cond=threading.Condition()
        
    def write(self,buff):
        with self.cond:
            self.extend(buff)
            self.cond.notify()

    def available(self):
        return len(self)>0

    def popleft_or_wait(self,deadline=None):
        """ deadline is when to give up on the pop
        """
        with self.cond:
            while not self.available():
                if deadline is not None:
                    timeout=deadline-time.time()
                    if timeout>=0:
                        self.cond.wait(timeout)
                    else:
                        # never got anything.
                        return None
                else:
                    self.cond.wait()
            # success
            return self.popleft()

    def clear(self):
        """ Remove any text sitting in the ring
        """
        self.read(timeout=0)

    def read(self,n=None,timeout=None):
        """ Collect up to n characters, in up to timeout seconds,
        and return as a string.

        if n==None implies n=inf,
        timeout==None implies timeout=inf.

        so you have to specify at least one.
        """
        if n is None and timeout is None:
            raise Exception("Can't wait forever to read infinite characters")

        buff=[]
        if timeout==0:
            # simple case - return whatever is in the buffer, up to length limit 
            with self.cond:
                while self.available() and ((n is None) or (len(buff)<n)):
                    buff.append(self.popleft())
        elif timeout is None and n is not None:
            with self.cond:
                while len(buff)<n:
                    while not self.available():
                        self.cond.wait()
                    buff.append(self.popleft())
        else:
            # stop either when length or time limit is reached
            t_stop=time.time() + timeout
            with self.cond:
                while len(buff)<n:
                    while not self.available():
                        timeout=t_stop-time.time()
                        if timeout>0:
                            self.cond.wait(timeout)
                    if self.available():
                        buff.append(self.popleft())
                    else:
                        break
        return ''.join(buff)

class FreebirdComm(object):
    """ A class for interacting with an attached freebird
    """
    TEENSY31_USBID='16c0:483'
    prompt='freebird>'
    timeout=10
    
    def __init__(self):
        self.serial=None
        logging.basicConfig(level=logging.DEBUG)
        self.log=logging.getLogger() # can change this to something more appropriate later
        self.log.level=logging.DEBUG
        self.consumers=[]
        self.serial_buffer=self.add_listener()

    def __del__(self):
        self.disconnect()

    def available_serial_ports(self):
        """ 
        returns a list: [ [port_path, port type, info, score], ... ]

        where score is a rough determination of how likely it is to be a freebird.
        
        """
        ports=[]
        for port_path,port_type,port_info in serial.tools.list_ports.comports():
            score=0
            if port_type=='USB Serial':
                score+=5
            if port_info.find(self.TEENSY31_USBID)>=0:
                score+=10
            if port_path.find('HC-0')>=0:
                # on Mac, the BT2S adapter shows up as HC-06-DevB
                score+=7
            ports.append( [port_path,port_type,port_info,score] )
        ports.sort(key=lambda elt: -elt[3])
        return ports
            
    def connect(self,min_score=15):
        """ Attempt to connect. 
        timeout: stop retrying after this many seconds.  0 for one shot, -1
          to try forever
        min_score: vague heuristic for choosing which serial ports are good candidates.
           for starters, 15 means the USB ID matches a teensy 3.1
        return True if successful.
        """
        timeout=self.timeout
        
        t_start=time.time()
        while 1:
            ports = self.available_serial_ports()

            self.log.info('available ports: %s'%str(ports))

            for port,ptype,pinfo,pscore in ports:
                if self.open_serial_port(port):
                    self.io_thread=threading.Thread(target=self.listen)
                    self.io_thread.daemon=True
                    self.io_thread.start()
                    return True
            
            if timeout>=0 and time.time() > t_start+timeout:
                break
            time.sleep(1.0)
        return False
    
    def disconnect(self):
        if self.serial is not None:
            # in this order so that the reader thread will set
            # self.serial is None, before it sees that the port is
            # closed.
            port=self.serial
            self.serial=None
            port.close()

    @property
    def connected(self):
        return self.serial is not None

    def poll_state(self):
        """ not very robust way to determine the state of the logger and
        connection.
        this blocks, and may not deal well with a logger that's streaming
        data.
        """
        if not self.connected:
            return 'disconnected'
        else:
            # send a blank line, should either return prompt, or some message
            # about sampling mode
            self.serial_buffer.clear()
            self.write("\r")

            for lcount in range(10):
                line=self.readline(timeout=1.0)
                if len(line)==0:
                    self.log.warning("Failed to get enough input for msg=%s"%msg)
                    return 'unknown'
                line=line.strip()
                if line.find(self.prompt)>=0:
                    return 'command'
                if len(line)>0 and line[0] in ['#','$']:
                    return 'sampling'
                else:
                    print "Trying poll state, got line:",line
            print "Unsure, but assuming sampling mode"
            return 'sampling'
            
    def open_serial_port(self,port):
        self.disconnect()
        try:
            self.serial = serial.Serial(port=port,baudrate=115200,timeout=0.1)
        except Exception as exc:
            self.log.warning("Failed to open serial port %s"%port)
            self.log.warning("Exception: %s"%str(exc))
            return False
        return True
        
    @req_connected
    def enter_command_mode(self,timeout=None):
        self.log.info('entering command mode')
        self.write("!\r")

        while 1:
            line=self.readline(timeout=self.timeout)
            if len(line)==0:
                self.log.warning("readline timed out trying for command mode")
                return False
            if line.strip()==self.prompt:
                self.log.info("enter_command_mode: found prompt - good")
                return True
                    

    @req_cmdmode
    def query_datetimes(self):
        """ This returns a pair of synchronized datetimes, the first
        from the freebird, the second from the local machine.

        They are synchronized in the sense that they represent the same
        instant in time, as seen from the two clocks.  Code on the freebird
        attempts to synchronize the output with a transition in the seconds
        digit of the time, and code here attempts to get an accurate timestamp
        of when the data is received locally.  Simple tests show repeatability
        better than 2ms.
        """ 
        freebird_dt=None
        local_dt=None
        
        for line in self.interact('datetime',timeout=self.timeout):
            now_dt=datetime.datetime.now()
            parts=line.split(':',1)
            if len(parts)==2 and parts[0].strip()=='datetime':
                time_str=parts[1].strip()
                freebird_dt=datetime.datetime.strptime(time_str,'%Y-%m-%d %H:%M:%S')
                local_dt=now_dt

        return freebird_dt,local_dt
    @req_cmdmode
    def sync_datetime_to_local_machine(self):
        """ Set the freebird clock to the local clock
        Attempts to wait until local time is at an integer second,
        then sync to the freebird.

        Basic tests show that this results in the freebird clock
        lagging the local clock by about 6ms.
        """
        # wait til the latter half of a second:
        while datetime.datetime.now().microsecond < 500000:
            pass
        # now wait til the rollover:
        while 1:
            dt=datetime.datetime.now()
            if dt.microsecond > 500000:
                continue
            cmd=datetime.datetime.now().strftime('datetime=%Y-%m-%d %H:%M:%S')
            self.send(cmd)
            break

    # "low-level" I/O methods - multiplex access to the serial stream
    def add_listener(self):
        tring=TextRing(1024)
        self.consumers.append(tring)
        return tring
        
    def listen(self):
        """ should be started in separate thread
        """
        self.log.info("Beginning listener loop")
        while self.serial:
            self.serial.timeout=1.0
            try:
                buff=self.serial.read()
            except serial.SerialException as exc:
                if not self.serial:
                    # probably the main thread closed the port
                    pass
                else:
                    print "Serial exception ",exc
                buff=""
                
            if len(buff)>0:
                for tring in self.consumers:
                    tring.write(buff)
        self.log.info("Leaving listener loop")

    @req_connected
    def read(self,n=None,timeout='default'):
        """ return bytes ready from the serial port -
        callable in the main thread.
        if n is None or 0, return all available data.

        if timeout is None, wait forever.  otherwise, a decimal
        number of seconds to wait.
        if 'default', use self.timeout
        """
        if timeout is 'default':
            timeout=self.timeout
        return self.serial_buffer.read(n=n,timeout=timeout)
        
    @req_connected
    def write(self,buff):
        self.serial.write(buff)
        self.serial.flush()
        
    def readline(self,timeout='default'):
        """ Read data from the io_consumer until an end of line
        (either \r or \n) is seen.  If any single read takes longer
        than timeout, return whatever data has been read so far.

        returned lines include the EOL character, unless it timed out
        """
        buff=[]
        while 1:
            char=self.read(n=1,timeout=timeout)
            if char is None:
                break
            buff.append(char)
            if char in "\r\n":
                break
        return "".join(buff)

    # "high-level" methods for interacting with device
    @req_cmdmode
    def interact(self,msg,timeout=None):
        """ call/response interaction as a generator

        return values are single lines of output returned from freebird,
          with whitespace stripped from ends
          
        first line is msg as it was echoed back.
        blank lines are skipped
        assumes that response will end with the prompt - so this cannot
        be used to switch modes.

        if the read times out, with no prompt seen, returns None, and
        stops iteration.

        if a prompt is seen, it is not returned and iteration stops
        """
        self.write(msg+"\r")

        while 1:
            line=self.readline(timeout=timeout)
            if len(line)==0:
                self.log.warning("Failed to get enough input for msg=%s"%msg)
                yield None
                break
            line=line.strip()
            if line.find(self.prompt)>=0:
                break
            else:
                if len(line)>0:
                    yield line
            
    def send(self,cmd,timeout=None):
        """ Send a command, return a list of response lines
        """
        output=[]
        for line in self.interact(cmd,timeout=timeout):
            output.append(line)
        return output

###

if 0:
    fb=FreebirdComm()
    fb.connect(4)
    fb.enter_command_mode()

    # fb.sync_datetime_to_local_machine()
    fb_dt,my_dt=fb.query_datetimes()
    print "My time - freebird_time:",my_dt-fb_dt
    fb.disconnect()


