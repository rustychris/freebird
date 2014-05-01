import os

import datafile
reload(datafile)

fbin=datafile.freebird_file_factory('../../../../testing/2014-04-30/DATA0004.BIN')
fbin.serials['squid']='SN104'
fbin.serials['sbe7probe']='C175'

data=fbin.read_all()
t_adj=3.5/86400.
data['dn_py']+=t_adj





stride=slice(4300*500,4430*500,1)

dnums=data['dn_py']


if 0:
    clf()
    ax1=subplot(2,1,1)
    plot_date(dnums[stride],data['counts'][stride],'g-') 
    ax2=subplot(2,1,2,sharex=ax1)
    imu_az=data['imu_a'][stride,2]
    plot_date(dnums[stride],data['imu_a'][stride,0],'r-')
    plot_date(dnums[stride],data['imu_a'][stride,1],'b-')
    plot_date(dnums[stride],data['imu_a'][stride,2],'k-')


# Everything looks pretty reasonable -
#  in the water, read something like 19880 counts.
#  in air, read about 22320.
# slight trend down as it was lowered, but no
# reversal when it comes up.
# the x-y accelerometers pickup the vibration of the
# feet - subtle in z, and not apparent in the squid signal
#  though the gradients are so low that's not surprising.

# droplets falling off the sensor have a very spiky
# response - could be a good test for the deemphasis code.


rsk_fn=os.path.join(os.path.dirname(fbin.filename),"065630_20140430_1619.rsk")
r2n=rsk.Rsk(rsk_fn)
rbr_data=r2n.named_array()

clf()
ax1=subplot(2,1,1)
plot_date(data['dn_py'],data['cond'],'g-')
plot_date(data['dn_py'],data['cond_emph'],'-',color='orange')
plot_date(rbr_data['dn_py'],rbr_data['cond05'],'b-')

ax2=subplot(2,1,2,sharex=ax1)
plot_date(rbr_data['dn_py'],rbr_data['pres07'],'k-')
ax1.axis((735353.66146002628, 735353.66226767935, -54.009432922175002, 80.001960019161231))



cast_dn_range=[735353.66161465307,735353.66167898581]
sq_pres=interp(data['dn_py'],rbr_data['dn_py'],rbr_data['pres07'])

figure(4)
C_adj=7.83
z_adj=0.14 # based on photo
clf()
rb_sel=(rbr_data['dn_py']>cast_dn_range[0])&(rbr_data['dn_py']<cast_dn_range[1])

sq_sel=(data['dn_py']>cast_dn_range[0])&(data['dn_py']<cast_dn_range[1])
        
plot(rbr_data['cond05'][rb_sel],-rbr_data['pres07'][rb_sel]+10.1325,'r-',label='RBR')
plot(data['cond'][sq_sel]+C_adj,-sq_pres[sq_sel]-z_adj+10.1325,'g-',label='Squid+%.2f'%C_adj)
legend()
xlabel('mS/cm')
ylabel('dbar')
savefig('../../../../notebook/figures/test-2014-04-30-cast.pdf')    


# So the 3.5 s offset is RTC clock drift.  separate issue.

# The squid never gets quite down to 0, and tops out at 26.2 mS/cm, where the
# RBR reports 33.9.
# The metal cage could cause a slight overestimate for the RBR, but not that much.

# And the squid signal is still very peaky, like the filter may not have
# effectively removed all transients.
from matplotlib.mlab import psd

dn_range=(735353.66222555796,
          735353.66237439169)
t_adj=3.5/86400.
dn=data['dn_py']+t_adj
sq_sel=(dn>dn_range[0])&(dn<dn_range[1])
sqPxx,sqf_hz=psd(cond[sq_sel],
                 Fs=fbin.sample_rate_hz(),scale_by_freq=True,
                 NFFT=4096)

rb_sel=(rbr_data['dn_py']>dn_range[0])&(rbr_data['dn_py']<dn_range[1])
rbPxx,rbf_hz=psd(rbr_data['cond05'][rb_sel],
                 Fs=6.0,scale_by_freq=True)
                 
C_adj=7.65                 
figure(2)
clf()
ax1=subplot(2,1,1)
loglog(sqf_hz,sqPxx,'b-')
loglog(rbf_hz,rbPxx,'r-')
ax2=subplot(2,1,2)
plot_date(rbr_data['dn_py'][rb_sel],rbr_data['cond05'][rb_sel],'r-')
plot_date(data['dn_py'][sq_sel]+t_adj,cond[sq_sel]+C_adj,'b-')




import sys
sys.path.append('../../../../../ct_river/notebook')
import plot_widgets
plot_widgets.LogLogSlopeGrid()

# Not very good....
# The whole power spectrum looks like k^{-4}, up to the
# nyquist of 250.
# maybe this is just the deemphasis acting on the background noise?

# To resolve:
# 1. why are the DC values different from the RBR?
# 2. can anything be said about real noise performance from this spectrum?


