import compass_cal
reload(compass_cal)

fn="../../../../../ct_river/data/2014-05/tioga_winch/freebird/20140514/compass-0x5002930054E40942-20140514T0643.dat.npz"
cf=compass_cal.CalibrationFit(fn)

cf.fit()

cf.write_cal('compass.cal',overwrite=True)
    
samples=cf.raw_data['samples']


figure(1).clf()
plot(samples[:,0],samples[:,1],'r.')
plot(samples[:,1],samples[:,2],'g.')
plot(samples[:,0],samples[:,2],'b.')
axis('equal')


## Brute force fitting for axis-aligned ellipse.
# Find the center:
#  the center is defined by the (x,y,z) point which minimizes variance in the distance from the center.
# fitting all five parameters at once was considerably slower
from scipy.optimize import fmin
def cost_center(xyz):
    return var(np.sum(( samples - xyz )**2,axis=1))

center=fmin(cost_center,[1,1,1])
csamples=samples-center

figure(1).clf()
plot(csamples[:,0],csamples[:,1],'r.')
plot(csamples[:,1],csamples[:,2],'g.')
plot(csamples[:,0],csamples[:,2],'b.')
axis('equal')

# And the scale factors:
def cost_scale(xy):
    # overall scale doesn't matter, so really just two degrees of freedom
    # and don't allow inverting any axis
    xyz=array([abs(xy[0]),abs(xy[1]),1.0]) # 
    return var(np.sum(( csamples*xyz )**2,axis=1))

scales=abs(fmin(cost_scale,[1,1]))
scales=array([scales[0],scales[1],1.0])

adjusted=csamples*scales

if 0:
    from mayavi import mlab
    mfig=mlab.figure()
    mlab.clf()

    mlab.points3d(adjusted[:,0],adjusted[:,1],adjusted[:,2])
    mlab.points3d([0],[0],[0],color=(1,0,0),scale_factor=20)
