import numpy as np

#### Madgwick library, translated from the matlab
from numpy.linalg import norm

def mynorm(vec):
    return np.sqrt( np.sum(vec**2) )

def normalize(vecs):
    mags=np.sqrt( np.sum(vecs**2,axis=-1))
    return vecs / mags[...,None]

def norm_vec3(vecs):
    # mags=np.sqrt( vecs[:,0]**2 + vecs[:,1]**2 + vecs[:,2]**2 )
    mags=norm(vecs,axis=1)
    return vecs / mags[:,None]

def quat_prod(a,b):
    # QUATERNPROD Calculates the quaternion product
    # 
    #    ab = quaternProd(a, b)
    # 
    #    Calculates the quaternion product of quaternion a and b.
    # 
    #    For more information see:
    #    http://www.x-io.co.uk/node/8#quaternions
    # 
    # 	Date          Author          Notes
    # 	27/09/2011    SOH Madgwick    Initial release
    a=np.asarray(a)
    b=np.asarray(b)
    
    ab=np.zeros(4,np.float64)
    
    ab[0] = a[0]*b[0]-a[1]*b[1]-a[2]*b[2]-a[3]*b[3]
    ab[1] = a[0]*b[1]+a[1]*b[0]+a[2]*b[3]-a[3]*b[2]
    ab[2] = a[0]*b[2]-a[1]*b[3]+a[2]*b[0]+a[3]*b[1]
    ab[3] = a[0]*b[3]+a[1]*b[2]-a[2]*b[1]+a[3]*b[0]
    return ab

def quat_conj(q):
    #QUATERN2ROTMAT Converts a quaternion to its conjugate
    #
    #   qConj = quaternConj(q)
    #
    #   Converts a quaternion to its conjugate.
    #
    #   For more information see:
    #   http://www.x-io.co.uk/node/8#quaternions
    #
    #	Date          Author          Notes
    #	27/09/2011    SOH Madgwick    Initial release
    qconj=q.copy()
    qconj[...,1:] *= -1
    return qconj

def quatern2euler(q):
    # QUATERN2EULER Converts a quaternion orientation to ZYX Euler angles
    # 
    #    q = quatern2euler(q)
    # 
    #    Converts a quaternion orientation to ZYX Euler angles where phi is a
    #    rotation around X, theta around Y and psi around Z.
    # 
    #    For more information see:
    #    http://www.x-io.co.uk/node/8#quaternions
    # 
    # 	Date          Author          Notes
    # 	27/09/2011    SOH Madgwick    Initial release
    q=np.asarray(q)
    R=np.zeros( (3,3)+q.shape[:-1], np.float64)
    
    R[0,0,...] = 2* q[...,0]**2-1+2*q[...,1]**2
    R[1,0,...] = 2*(q[...,1]*q[...,2]-q[...,0]*q[...,3])
    R[2,0,...] = 2*(q[...,1]*q[...,3]+q[...,0]*q[...,2])
    R[2,1,...] = 2*(q[...,2]*q[...,3]-q[...,0]*q[...,1])
    R[2,2,...] = 2*q[...,0]**2-1+2*q[...,3]**2

    phi = np.arctan2(R[2,1,...], R[2,2,...] )
    theta = -np.arctan(R[2,0,...] / np.sqrt(1-R[2,0,...]**2) );
    psi = np.arctan2(R[1,0,...], R[0,0,...] );

    zyx=np.array( [phi, theta, psi] )
    ndim=zyx.ndim
    return zyx.transpose( (np.arange(ndim) +1 ) % ndim )
    

class MadgwickAHRS(object):
    """
    MADGWICKAHRS Implementation of Madgwick's IMU and AHRS algorithms

       For more information see:
       http://www.x-io.co.uk/node/8#open_source_ahrs_and_imu_algorithms

       Date          Author          Notes
       28/09/2011    SOH Madgwick    Initial release
    """

    SamplePeriod = 1./500
    # output quaternion describing the Earth relative to the sensor
    Beta = 1               	# algorithm gain

    ## Public methods
    def __init__(self,**kwargs):
        """
        SamplePeriod
        Quaternion
        Beta
        """
        self.Quaternion = np.array([1, 0, 0, 0])
        self.__dict__.update(kwargs)

    def Update(self,Gyroscope,Accelerometer,Magnetometer):
        q = self.Quaternion # short name local variable for readability

        # Normalise accelerometer measurement
        if norm(Accelerometer) == 0:
            return # handle NaN
        
        Accelerometer = Accelerometer/norm(Accelerometer) # normalise magnitude

        # Normalise magnetometer measurement
        if norm(Magnetometer) == 0:
            return # handle NaN
        
        Magnetometer = Magnetometer/norm(Magnetometer) # normalise magnitude
        qmag=np.concatenate(([0],Magnetometer))
        
        # Reference direction of Earth's magnetic field
        h = quat_prod(q, quat_prod(qmag, quat_conj(q)))
        b = np.array( [0,norm([h[1],h[2]]), 0, h[3]] )

        # Gradient decent algorithm corrective step
        F = np.array( [2*(q[1]*q[3] - q[0]*q[2]) - Accelerometer[0],
                       2*(q[0]*q[1] + q[2]*q[3]) - Accelerometer[1],
                       2*(0.5 - q[1]**2 - q[2]**2) - Accelerometer[2],
                       2*b[1]*(0.5 - q[2]**2 - q[3]**2) + 2*b[3]*(q[1]*q[3] - q[0]*q[2]) - Magnetometer[0],
                       2*b[1]*(q[1]*q[2] - q[0]*q[3]) + 2*b[3]*(q[0]*q[1] + q[2]*q[3]) - Magnetometer[1],
                       2*b[1]*(q[0]*q[2] + q[1]*q[3]) + 2*b[3]*(0.5 - q[1]**2 - q[2]**2) - Magnetometer[2]] )

        J = np.array([ [-2*q[2], 2*q[3], -2*q[0],2*q[1]],
                       [2*q[1],  2*q[0], 2*q[3], 2*q[2]],
                       [0,-4*q[1],-4*q[2], 0],
                       [-2*b[3]*q[2],2*b[3]*q[3], -4*b[1]*q[2]-2*b[3]*q[0], -4*b[1]*q[3]+2*b[3]*q[1]],
                       [-2*b[1]*q[3]+2*b[3]*q[1], 2*b[1]*q[2]+2*b[3]*q[0], 2*b[1]*q[1]+2*b[3]*q[3], -2*b[1]*q[0]+2*b[3]*q[2]],
                       [2*b[1]*q[2], 2*b[1]*q[3]-4*b[3]*q[1], 2*b[1]*q[0]-4*b[3]*q[2], 2*b[1]*q[1]] ])
        step = np.dot(J.T,F)

        step /= norm(step) # normalise step magnitude

        # Compute rate of change of quaternion
        qDot = 0.5 * quat_prod(q,
                               [0,Gyroscope[0],Gyroscope[1],Gyroscope[2]]) - \
                               self.Beta * step;

        # Integrate to yield quaternion
        q = q + qDot * self.SamplePeriod
        self.Quaternion = q / norm(q); # normalise quaternion

    def BulkUpdate(self,Gyroscope,Accelerometer,Magnetometer):
        """ speedups by processing a chunk of data at once -
        Each input is now shaped [N,3], instead of [3]
        """
        if 1:
            quats=BulkUpdate_proc(self.Quaternion,self.Beta,self.SamplePeriod,
                                  Gyroscope,Accelerometer,Magnetometer)
            self.Quaternion = quats[-1]
            return quats
        else:
            Accelerometer=normalize(Accelerometer)
            Magnetometer=normalize(Magnetometer)
            N=len(Accelerometer)
            quats=np.zeros( (N,4), 'f8')


            # will have to do something smarter about times that accel or magneto
            # are zero magnitude.

            qmags=np.zeros( (N,4), 'f8') # was qmag
            qmags[:,1:] = Magnetometer

            # Reference direction of Earth's magnetic field
            for n in range(N):
                q = self.Quaternion # short name local variable for readability
                h = quat_prod(q, quat_prod(qmags[n], quat_conj(q)))
                b = np.array( [0,norm([h[1],h[2]]), 0, h[3]] )

                # Gradient decent algorithm corrective step
                F = np.array( [2*(q[1]*q[3] - q[0]*q[2]) - Accelerometer[n,0],
                               2*(q[0]*q[1] + q[2]*q[3]) - Accelerometer[n,1],
                               2*(0.5 - q[1]**2 - q[2]**2) - Accelerometer[n,2],
                               2*b[1]*(0.5 - q[2]**2 - q[3]**2) + 2*b[3]*(q[1]*q[3] - q[0]*q[2]) - Magnetometer[n,0],
                               2*b[1]*(q[1]*q[2] - q[0]*q[3]) + 2*b[3]*(q[0]*q[1] + q[2]*q[3]) - Magnetometer[n,1],
                               2*b[1]*(q[0]*q[2] + q[1]*q[3]) + 2*b[3]*(0.5 - q[1]**2 - q[2]**2) - Magnetometer[n,2]] )

                J = np.array([ [-2*q[2], 2*q[3], -2*q[0],2*q[1]],
                               [2*q[1],  2*q[0], 2*q[3], 2*q[2]],
                               [0,-4*q[1],-4*q[2], 0],
                               [-2*b[3]*q[2],2*b[3]*q[3], -4*b[1]*q[2]-2*b[3]*q[0], -4*b[1]*q[3]+2*b[3]*q[1]],
                               [-2*b[1]*q[3]+2*b[3]*q[1], 2*b[1]*q[2]+2*b[3]*q[0], 2*b[1]*q[1]+2*b[3]*q[3], -2*b[1]*q[0]+2*b[3]*q[2]],
                               [2*b[1]*q[2], 2*b[1]*q[3]-4*b[3]*q[1], 2*b[1]*q[0]-4*b[3]*q[2], 2*b[1]*q[1]] ])
                step = np.dot(J.T,F)
                step /= norm(step) # normalise step magnitude

                # Compute rate of change of quaternion
                qDot = 0.5 * quat_prod(q,
                                       [0,Gyroscope[n,0],Gyroscope[n,1],Gyroscope[n,2]]) - \
                                       self.Beta * step;

                # Integrate to yield quaternion
                q = q + qDot * self.SamplePeriod
                self.Quaternion = q / norm(q); # normalise quaternion
                quats[n]=self.Quaternion
            return quats



def BulkUpdate_proc(Quaternion,Beta,SamplePeriod,
                    Gyroscope,Accelerometer,Magnetometer):
    """ 
    """
    Accelerometer=normalize(Accelerometer.astype('f8'))
    Magnetometer=normalize(Magnetometer.astype('f8'))
    
    N=len(Accelerometer)
    quats=np.zeros( (N,4), 'f8')

    # will have to do something smarter about times that accel or magneto
    # are zero magnitude.

    qmags=np.zeros( (N,4), 'f8') # was qmag
    qmags[:,1:] = Magnetometer

    b=np.zeros(4,'f8')
    F=np.zeros(6,'f8')
    J=np.zeros((6,4),'f8')
    G=np.zeros(4,'f8')
    
    # Reference direction of Earth's magnetic field
    for n in range(N):
        if n%10000==0:
            print "AHRS: %d/%d  %.3f%%"%(n,N, 100*float(n)/N)
            
        q = Quaternion # short name local variable for readability
        h = quat_prod(q, quat_prod(qmags[n], quat_conj(q)))
        # b = np.array( [0,np.sqrt(h[1]**2+h[2]**2), 0, h[3]] )
        b[1]=np.sqrt(h[1]**2+h[2]**2)
        b[3]=h[3]

        # Gradient decent algorithm corrective step
        F[0] = 2*(q[1]*q[3] - q[0]*q[2]) - Accelerometer[n,0]
        F[1] = 2*(q[0]*q[1] + q[2]*q[3]) - Accelerometer[n,1]
        F[2] = 2*(0.5 - q[1]**2 - q[2]**2) - Accelerometer[n,2]
        F[3] = 2*b[1]*(0.5 - q[2]**2 - q[3]**2) + 2*b[3]*(q[1]*q[3] - q[0]*q[2]) - Magnetometer[n,0]
        F[4] = 2*b[1]*(q[1]*q[2] - q[0]*q[3]) + 2*b[3]*(q[0]*q[1] + q[2]*q[3]) - Magnetometer[n,1]
        F[5] = 2*b[1]*(q[0]*q[2] + q[1]*q[3]) + 2*b[3]*(0.5 - q[1]**2 - q[2]**2) - Magnetometer[n,2]

        J[0,0]=-2*q[2]
        J[0,1]=2*q[3]
        J[0,2]=-2*q[0]
        J[0,3]=2*q[1]
        J[1,0]=2*q[1]
        J[1,1]=2*q[0]
        J[1,2]=2*q[3]
        J[1,3]=2*q[2]
        J[2,0]=0
        J[2,1]=-4*q[1]
        J[2,2]=-4*q[2]
        J[2,3]=0
        J[3,0]=-2*b[3]*q[2]
        J[3,1]=2*b[3]*q[3]
        J[3,2]=-4*b[1]*q[2]-2*b[3]*q[0]
        J[3,3]=-4*b[1]*q[3]+2*b[3]*q[1]
        J[4,0]=-2*b[1]*q[3]+2*b[3]*q[1]
        J[4,1]=2*b[1]*q[2]+2*b[3]*q[0]
        J[4,2]=2*b[1]*q[1]+2*b[3]*q[3]
        J[4,3]=-2*b[1]*q[0]+2*b[3]*q[2]
        J[5,0]=2*b[1]*q[2]
        J[5,1]=2*b[1]*q[3]-4*b[3]*q[1]
        J[5,2]=2*b[1]*q[0]-4*b[3]*q[2]
        J[5,3]=2*b[1]*q[1]
        
        step = np.dot(J.T,F)
        step /= mynorm(step) # normalise step magnitude

        # Compute rate of change of quaternion
        G[1:]=Gyroscope[n]
        qDot = 0.5 * quat_prod(q,G) - Beta * step

        # Integrate to yield quaternion
        q = q + qDot * SamplePeriod
        quats[n]=Quaternion=q/mynorm(q) # normalise quaternion
    return quats
