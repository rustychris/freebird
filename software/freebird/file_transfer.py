import device

fb=device.FreebirdComm()
fb.connect()
fb.enter_command_mode()

lines=fb.send('send_file=DATA0000.BIN')
fb.disconnect()
