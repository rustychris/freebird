import sys
import device
import compass_cal

fb=device.FreebirdComm()
if not fb.connect(min_score=2):
    print "Failed to connect!"
    sys.exit(1)

fb.enter_command_mode()

cal=compass_cal.CompassCalibrator(fb)
cal.run()
fn="compass-%s.dat"%(fb.query_serial())
cal.save(fn)
