import numpy as np
from matplotlib import pyplot as plt
import device
import threading
import array_append

class CompassCalibrator(object):
    def __init__(self,dev):
        """ dev: FreebirdComm which is already connected
        """
        self.dev=dev

    def run(self):
        self.dev.enter_command_mode()
        self.dev.send("log_to_serial=1")
        self.dev.send("log_imu=1")
        self.dev.send("log_adc=0")
        self.dev.send("sample_monitor=0")
        self.dev.send("sample_interval_us=100000")
        self.dev.send("beep_interval_ms=0")

        self.freebird_serial=self.dev.query_serial()
    
        self.mxyz=np.zeros([0,3],np.float64)

        self.sample_thread = threading.Thread(target=self.log_data)
        self.sample_thread.daemon=True
        self.end_sampling=False
        self.sample_thread.start()

        self.init_plots()
        self.display_data()
        self.dev.enter_command_mode() # redundant with sample_thread

    def init_plots(self):
        self.fig=plt.figure(1)

        self.fig.canvas.mpl_connect('close_event',self.handle_close)
        
        self.fig.clf()
        ax_xy=plt.subplot(2,2,1)
        ax_zy=plt.subplot(2,2,2)
        ax_xz=plt.subplot(2,2,3)
        self.axs=[ax_xy,ax_zy,ax_xz]

        ax_xy.set_xlabel('X')
        ax_xy.set_ylabel('Y')
        ax_zy.set_xlabel('Z (axial')
        ax_zy.set_ylabel('Y')
        ax_xz.set_xlabel('X')
        ax_xz.set_ylabel('Z (axial)')

        mxyz=self.mxyz
        self.plt_xy=ax_xy.plot( mxyz[:,0], mxyz[:,1], 'r.')[0]
        self.plt_zy=ax_zy.plot( mxyz[:,2], mxyz[:,1], 'b.')[0]
        self.plt_xz=ax_xz.plot( mxyz[:,0], mxyz[:,2], 'g.')[0]
        
        self.point_xy=ax_xy.plot( mxyz[-1:,0], mxyz[-1:,1],'ko')[0]
        self.point_zy=ax_zy.plot( mxyz[-1:,2], mxyz[-1:,1],'ko')[0]
        self.point_xz=ax_xz.plot( mxyz[-1:,0], mxyz[-1:,2],'ko')[0]
        
        max_val=450
        for ax in self.axs:
            ax.axis([-max_val,max_val,-max_val,max_val])

    def log_data(self):
        sampler=self.dev.parsed_sample_sync()

        try:
            while not self.end_sampling:
                rec=sampler.next()
                tup = rec['imu_m']
                self.mxyz=array_append.array_append(self.mxyz,tup)
        finally:
            self.dev.enter_command_mode()

    def handle_close(self,*evt):
        self.end_sampling=True
        
    def display_data(self):
        try:
            while not self.end_sampling:
                mxyz=self.mxyz
                self.plt_xy.set_xdata(mxyz[:,0])
                self.plt_xy.set_ydata(mxyz[:,1])
                self.plt_zy.set_xdata(mxyz[:,2])
                self.plt_zy.set_ydata(mxyz[:,1])
                self.plt_xz.set_xdata(mxyz[:,0])
                self.plt_xz.set_ydata(mxyz[:,2])

                self.point_xy.set_xdata(mxyz[-1:,0])
                self.point_xy.set_ydata(mxyz[-1:,1])
                self.point_zy.set_xdata(mxyz[-1:,2])
                self.point_zy.set_ydata(mxyz[-1:,1])
                self.point_xz.set_xdata(mxyz[-1:,0])
                self.point_xz.set_ydata(mxyz[-1:,2])
                
                print len(mxyz)
                plt.pause(0.001)
        except KeyboardInterrupt:
            pass
        
        self.end_sampling=True
        self.sample_thread.join()
        return

    def save(self,fn):
        np.savez(fn,samples=self.mxyz,
                 freebird_serial=self.freebird_serial)

# Just need a better way to sense when the user is done.
# when we have a separate figure, how to detect when it's
# closed?
