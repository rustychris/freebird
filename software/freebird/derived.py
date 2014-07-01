"""
Various derived quantities, like converting from freebird/squid counts
to mS/cm,
or applying an offline sensor fusion algorithm to IMU data.

Note that many of these need sensor-specific data, and will default to
looking appropriate values in files ending in .cal
"""
import os
import glob
from scipy.signal import butter,lfilter,lfiltic
import numpy as np

def fmt_np_array(a):
    """ return a string representing the numpy array a, suitable for
    writing to a file which will later be read in via execfile.
    Assumes numpy is available as np
    """
    return repr(a).replace('array','np.array')

class Calibration(object):
    database=[]
    short_name='generic'
    serial=None
    def __init__(self):
        Calibration.database.append(self)
        print "Calibration database: %s/%s"%(self.short_name,self.serial)

    @classmethod
    def load_directory(klass,direc):
        for fn in glob.glob(os.path.join(direc,'*.cal')):
            klass.load_file(fn)
    @classmethod
    def load_file(klass,fn):
        print "Loading calibrations from %s"%fn
        execfile(fn)

    @classmethod
    def find(klass,short_name,serial):
        """ This would be where the matching could be made more forgiving
        """
        for cal in klass.database:
            if cal.short_name.lower()==short_name.lower() and cal.serial.lower()==serial.lower():
                return cal
        return None

class Magnetometer(Calibration):
    short_name="magnetometer"
    def __init__(self,offset,scale,serial=None,comment=None,**kwargs):
        self.offset=offset
        self.scale=scale
        self.serial=serial
        self.comment=comment
        self.__dict__.update(kwargs)
        super(Magnetometer,self).__init__()
    def save(self,fn):
        txt="\n".join(["Magnetometer(offset=%s,"%fmt_np_array(self.offset),
                       "             scale=%s,"%fmt_np_array(self.scale),
                       "             serial='%s',"%self.serial,
                       "             comment=%s)"%repr(self.comment)])
        with open(fn,'wt') as fp:
            fp.write(txt)
    def adjust(self,mxyz):
        return (mxyz-self.offset)*self.scale
        
class Squid(Calibration):
    short_name='squid'
    def __init__(self,a,b,Gd,serial=None,**kwargs):
        self.a=a
        self.b=b
        self.Gd=Gd
        self.serial=serial
        self.__dict__.update(kwargs)
        super(Squid,self).__init__()
        
class SBE7Probe(Calibration):
    short_name='sbe7probe'
    def __init__(self,K,serial=None,**kwargs):
        self.K=K
        self.serial=serial
        super(SBE7Probe,self).__init__()


class Derived(object):
    pass

class SquidPostprocess(Derived):
    """ Convert counts to voltage, to conductivity, to deemphasized conductivity
    applies compass calibration if it's available.
    """
    def __init__(self,squid_serial,sbe7probe_serial,freebird_serial):
        self.squid_serial=squid_serial
        self.sbe7probe_serial=sbe7probe_serial
        self.squid_cal=Calibration.find(short_name='squid',serial=squid_serial)
        self.sbe7_cal =Calibration.find(short_name='sbe7probe',serial=sbe7probe_serial)
        self.mag_cal = Calibration.find(short_name='magnetometer',serial=freebird_serial)
    
    def postprocess(self,fbin,data,field_name=None):
        """ Given a freebird_bin object and the data array,
        returns a list of tuples [(field_name,field_values,units),...]
        for new fields generated from the postprocessing

        For squid, assumes that there is a 'counts' field.

        field_name is either the name of a field in data, or some prefix
        for several fields, useful when multiple instruments of the same
        type are logged together, and are differentiated by some prefix.
        """
        field_name=field_name or 'counts'

        raw=data['counts']

        # Convert to voltage:
        Vx=raw*4.096 / 32768.0
        Y=(Vx-self.squid_cal.a)/self.squid_cal.b

        C_dC=10*Y/self.sbe7_cal.K

        f_s=fbin.sample_rate_hz()
        # 1st order,
        # Wn=1.0/((f_s/2)*2*np.pi*self.squid_cal.Gd)
        # f_s=512Hz nyq=256Hz  256*2pi rad
        # Gd=0.02 s, so this should be a 7.98Hz cutoff
        [b,a]=butter(1,1.0/((f_s/2)*2*np.pi*self.squid_cal.Gd))
        zi=lfiltic(b,a,[C_dC[0],C_dC[0]])
        C,zf=lfilter(b,a,C_dC,zi=zi)

        # conductance * cell constant * S/m to mS/cm
        fields=[('cond',C,'mS/cm'),('cond_emph',C_dC,'mS/cm')]

        if self.mag_cal is not None:
            cal_mag = self.mag_cal.adjust(data['imu_m'])
            fields.append( ('cal_imu_m',cal_mag,'counts') )

        # Remove offset from gyro, and scale based on the MPU6050 reference Arduino
        # code setting the gyro sensitivity to 250deg/s
        # Full scale is [-32768,32767]
        fields.append( ('cal_imu_g',(data['imu_g'] - data['imu_g'].mean(axis=0))*250./32768,'deg/s') )
                       
        return fields

