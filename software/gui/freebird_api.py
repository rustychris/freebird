import serial
import os
import sys
import glob
import serial
import serial.tools.list_ports
import time
import logging
import datetime

# place holder decorators, for now just to document
def req_connected(f):
    return f
def req_cmdmode(f):
    return f


class FreebirdComm(object):
    """ A class for interacting with an attached freebird
    """
    TEENSY31_USBID='16c0:483'
    prompt='freebird>'
    
    def __init__(self):
        self.serial=None
        logging.basicConfig(level=logging.DEBUG)
        self.log=logging.getLogger() # can change this to something more appropriate later
        self.log.level=logging.DEBUG

            
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
            
    def connect(self,timeout=-1,min_score=15):
        """ Attempt to connect. 
        timeout: stop retrying after this many seconds.  0 for one shot, -1
          to try forever
        min_score: vague heuristic for choosing which serial ports are good candidates.
           for starters, 15 means the USB ID matches a teensy 3.1
        return True if successful.
        """
        t_start=time.time()
        while 1:
            ports = self.available_serial_ports()

            self.log.info('available ports: %s'%str(ports))

            for port,ptype,pinfo,pscore in ports:
                if self.open_serial_port(port):
                    return True
            
            if timeout>=0 and time.time() > t_start+timeout:
                break
            time.sleep(1.0)
        return False
    
    def disconnect(self):
        if self.serial is not None:
            self.serial.close()
            self.serial=None
            
    @property
    def connected(self):
        return self.serial is not None
            
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
    def enter_command_mode(self):
        self.log.info('entering command mode')
        self.serial.write("\r")
        self.serial.flush()

        while 1:
            line=self.serial.readline()
            if line.find(self.prompt)>=0:
                return True
            if len(line)==0:
                self.log.warning("Failed to find command mode prompt")
                return False
            else:
                print line

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
        
        for line in self.interact('datetime',timeout=2):
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

    def send(self,cmd,timeout=None):
        """ Send a command, return a list of response lines
        """
        output=[]
        for line in self.interact(cmd,timeout=timeout):
            output.append(line)
        return output
    
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
        self.serial.write(msg+"\r")

        old_timeout=self.serial.timeout
        if timeout is not None:
            self.serial.timeout=timeout
            
        try:
            while 1:
                line=self.serial.readline()
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
        finally:
            self.serial.timeout=old_timeout

###

if 0:
    fb=FreebirdComm()
    fb.connect(4)
    fb.enter_command_mode()


    fb.sync_datetime_to_local_machine()


    fb_dt,my_dt=fb.query_datetimes()
    print my_dt-fb_dt



