import freebird_bin
reload(freebird_bin)


fbin=freebird_bin.FreebirdFile('../../../../testing/2014-04-25/DATA0004.BIN')
# DATA0004.BIN
data=fbin.data()

#clf() ; psd(data.astype(np.float64),Fs=500)
clf();plot(data)

