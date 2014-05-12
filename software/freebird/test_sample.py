import sys
import device
import array_append

fb=device.FreebirdComm()
if not fb.connect(min_score=2):
    print "Failed to connect!"
    sys.exit(1)
fb.enter_command_mode()

fb.send("log_to_serial=1")
fb.send("log_imu=1")
fb.send("log_adc=0")
fb.send("sample_monitor=0")
fb.send("sample_interval_us=100000")
fb.send("beep_interval_ms=0")




fig=figure(1)
fig.clf()
ax_xy=subplot(2,2,1)
ax_xz=subplot(2,2,2)
ax_yz=subplot(2,2,3)
axs=[ax_xy,ax_xz,ax_yz]

mxyz=zeros([0,3],np.float64)

plt_xy=ax_xy.plot( mxyz[:,0], mxyz[:,1], 'r.')[0]
plt_xz=ax_xz.plot( mxyz[:,0], mxyz[:,2], 'g.')[0]
plt_yz=ax_yz.plot( mxyz[:,1], mxyz[:,2], 'b.')[0]

max_val=450
for ax in axs:
    ax.axis([-max_val,max_val,-max_val,max_val])

sampler=fb.parsed_sample_sync()
try:
    for i in range(1000):
        rec=sampler.next()
        tup = rec['imu_m']
        mxyz=array_append.array_append(mxyz,tup)
        plt_xy.set_xdata(mxyz[:,0])
        plt_xy.set_ydata(mxyz[:,1])
        plt_xz.set_xdata(mxyz[:,0])
        plt_xz.set_ydata(mxyz[:,2])
        plt_yz.set_xdata(mxyz[:,1])
        plt_yz.set_ydata(mxyz[:,2])
        print len(mxyz)
        pause(0.001)
except KeyboardInterrupt:
    fb.enter_command_mode()
    
print fb.send("info")
    
#fb.disconnect()


from mayavi import mlab

mf=mlab.figure()
mlab.points3d(mxyz[:,0],
              mxyz[:,1],
              mxyz[:,2])
