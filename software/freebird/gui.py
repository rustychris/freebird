import wx
import threading
import os
from matplotlib.dates import num2date,date2num
import datetime
import numpy as np

ID_CONNECT=wx.NewId()

import device 
import datafile

import data_gui

class FreebirdGui(wx.App):
    def __init__(self):
        # don't redirect stdout/stderr
        super(FreebirdGui,self).__init__(False)

        self.frame=MainFrame(None,"Fly high, freebird")


class MainFrame(wx.Frame):
    def __init__(self,parent,title):
        super(MainFrame,self).__init__(parent, title=title, size=(600,450))

        self.huds={} # map port path to HUD panel instances

        self.init_menu()

        # and the main content area:
        self.notebook=wx.Notebook(self)
        self.Show(True)
        
    def init_menu(self):
        # Setting up the menu.
        connect_menu=wx.Menu()

        # wx.ID_ABOUT and wx.ID_EXIT are standard IDs provided by wxWidgets.
        item=connect_menu.Append(ID_CONNECT, "&Connect"," Connect to a device")
        self.Bind(wx.EVT_MENU, self.menu_connect, item)
        
        connect_menu.AppendSeparator()
        connect_menu.Append(wx.ID_EXIT,"E&xit"," quit")

        data_menu=wx.Menu()
        item=data_menu.Append(wx.ID_OPEN,"&Open local"," Open one or more output files from the local machine")
        self.Bind(wx.EVT_MENU,self.menu_data_open_local,item)

        # Creating the menubar.
        menu_bar = wx.MenuBar()
        menu_bar.Append(connect_menu,"&Connect")
        menu_bar.Append(data_menu,"&Data")
        self.SetMenuBar(menu_bar)  # Adding the MenuBar to the Frame content.

    def menu_connect(self,event):
        print "Connect!"
        dlg=ConnectDialog(self)
        if dlg.ShowModal() == wx.ID_OK:
            port = dlg.selected_port()
            print "OK - ",port[0]
            dlg.Destroy()
            hud = self.huds[port[0]] = ConnectedDeviceHUD(self.notebook,port[0])
            self.notebook.AddPage(hud,hud.port_short_name)
            hud.Show(True)
        else:
            print "Nope."
        
    def menu_data_open_local(self,event):
        print "Open local!"

        self.dirname = ''
        dlg = wx.FileDialog(self, "Choose a file", self.dirname, "", "*.BIN", wx.OPEN)
        if dlg.ShowModal() == wx.ID_OK:
            # for now, assume a single file, but allow for lists of files
            dc=DatafileViewer(self,"Data file",
                              filename=os.path.join(dlg.GetDirectory(),dlg.GetFilename()))
            dlg.Destroy()

class ConnectedDeviceHUD(wx.Panel):
    """ A window holding widgets for info about a connected device.
    roughly 1:1 correspondence with device.FreebirdComm, though
    this doesn't get instantiated until a specific port has been selected.
    """
    def __init__(self,parent,port_path):
        super(ConnectedDeviceHUD,self).__init__(parent)
        self.port_path=port_path
        self.comm=device.FreebirdComm()
        self.state='unknown'

        main_vbox=wx.BoxSizer(wx.VERTICAL)
        
        label=wx.StaticText(self, label="Freebird on %s"%self.port_path)
        main_vbox.Add(label)

        main_vbox.Add((10,10))

        butn_hbox=wx.BoxSizer(wx.HORIZONTAL)
        self.connect_btn=wx.Button(self, label="Connect")
        self.Bind(wx.EVT_BUTTON,self.handle_connect,self.connect_btn)
        butn_hbox.Add(self.connect_btn)

        self.disconnect_btn=wx.Button(self,label="Disconnect")
        self.Bind(wx.EVT_BUTTON,self.handle_disconnect,self.disconnect_btn)
        butn_hbox.Add(self.disconnect_btn)

        self.stop_btn=wx.Button(self,label="Stop sampling")
        self.Bind(wx.EVT_BUTTON,self.handle_stop,self.stop_btn)
        self.start_btn=wx.Button(self,label="Start sampling")
        self.Bind(wx.EVT_BUTTON,self.handle_start,self.start_btn)
        butn_hbox.Add(self.start_btn)
        butn_hbox.Add(self.stop_btn)
        
        main_vbox.Add(butn_hbox)

        main_vbox.Add((10,10))

        # Clock:
        self.add_clock_controls(main_vbox)
        
        main_vbox.Add((10,10))

        # Serial console:
        
        self.serial_input=wx.TextCtrl(parent=self,size=(400,-1),style=wx.TE_PROCESS_ENTER)

        main_vbox.Add(self.serial_input,flag=wx.EXPAND)
        
        self.Bind(wx.EVT_TEXT_ENTER,self.handle_serial_input,self.serial_input)
        self.listener=self.comm.add_listener()
        self.Bind(wx.EVT_IDLE,self.poll_serial)

        self.sercon=wx.TextCtrl(parent=self,style=wx.TE_MULTILINE|wx.TE_READONLY,
                                pos=(10,10),size=(400,300))

        main_vbox.Add(self.sercon,flag=wx.EXPAND)
        
        self.SetSizerAndFit(main_vbox)

        self.set_state('disconnected')

    def add_clock_controls(self,main_vbox):
        clock_hbox=wx.BoxSizer(wx.HORIZONTAL)
        clock_btns=wx.BoxSizer(wx.VERTICAL)
        clock_txts=wx.BoxSizer(wx.VERTICAL)
        clock_hbox.Add(clock_btns)
        clock_hbox.Add((10,10))
        clock_hbox.Add(clock_txts)
        main_vbox.Add(clock_hbox)
        
        self.query_clock_btn=wx.Button(self,label="Query clock")
        self.Bind(wx.EVT_BUTTON,self.handle_query_clock,self.query_clock_btn)
        clock_btns.Add(self.query_clock_btn,flag=wx.ALIGN_CENTER)
        
        self.sync_clock_btn=wx.Button(self,label="Sync clock")
        self.Bind(wx.EVT_BUTTON,self.handle_sync_clock,self.sync_clock_btn)
        clock_btns.Add(self.sync_clock_btn,flag=wx.ALIGN_CENTER)

        grid=wx.GridBagSizer(vgap=5,hgap=5)
        
        grid.Add( wx.StaticText(self,label="Local:"),pos=(0,0))
        grid.Add( wx.StaticText(self,label="Freebird:"),pos=(1,0))

        clock_txts.Add(grid)
        
        self.local_dt_display=wx.StaticText(self,label="---")
        grid.Add(self.local_dt_display,pos=(0,1))
        
        self.device_dt_display=wx.StaticText(self,label="---")
        grid.Add(self.device_dt_display,pos=(1,1))
        

    # Would like to show serial interactions, while also allowing pre-packaged 
    # interactions.  So any characters coming back from the comm should show
    # up in the console here - but what is the right polling mechanism here?
    # (a) the GUI polls the comm
    # (b) the comm runs a thread which is always reading from the serial port.
    # (c) what if the comm runs a reading thread, always logging from serial to
    #     some number
    def handle_serial_input(self,event):
        txt=self.serial_input.GetValue()
        self.serial_input.SetValue("")
        
        if self.state in ['command']:
            self.comm.write(str(txt)+"\r\n")

    max_console_lines=40
    def poll_serial(self,event):
        buff=self.listener.read(timeout=0)
        if len(buff):
            # This is backwards because AppendText has some nice
            # auto-scrolling behavior, but it gets confused if it autoscrolls
            # and then the buffer changes.

            # truncate to max_console_lines
            while self.sercon.GetNumberOfLines() > self.max_console_lines:
                self.sercon.Remove(0,self.sercon.GetLineLength(0)+1)

            # seems that it comes back with \r\n end of line,
            # which skips two lines in the console.
            self.sercon.AppendText(buff.replace("\r",""))

            # unreliable:
            # and scroll to bottom
            # self.sercon.ShowPosition(self.sercon.GetLastPosition())
            # self.sercon.SetScrollPos(wx.VERTICAL,0) # ,self.sercon.GetScrollRange(wx.VERTICAL))
            
                
    def set_state(self,state):
        """ Update GUI controls to reflect a particular state
        """
        if state=='disconnected':
            self.connect_btn.Enable()
            self.disconnect_btn.Disable()
            self.query_clock_btn.Disable()
            self.sync_clock_btn.Disable()
            self.serial_input.Disable()
            self.stop_btn.Disable()
            self.start_btn.Disable()
        elif state in ['sampling','command']:
            self.connect_btn.Disable()
            self.disconnect_btn.Enable()
            if state=='sampling':
                self.query_clock_btn.Disable()
                self.sync_clock_btn.Disable()
                self.serial_input.Enable()
                self.stop_btn.Enable()
                self.start_btn.Disable()
            else:
                self.query_clock_btn.Enable()
                self.sync_clock_btn.Enable()
                self.serial_input.Enable()
                self.stop_btn.Disable()
                self.start_btn.Enable()
        self.state=state
            
    def handle_connect(self,evt):
        if self.state!='disconnected':
            print "WARNING: call to connect, but state is %s"%self.state
            return
        
        if self.comm.connect(4):
            print "Successfully connected"
            self.set_state( self.comm.poll_state() ) # 'sampling' or 'command'
            print "Got a state back"
        else:
            print "WARNING: could not connect, or could not enter command mode"
            
    def handle_disconnect(self,evt):
        self.comm.disconnect()
        self.set_state('disconnected')

    def handle_stop(self,evt):
        self.comm.write("!\r\n")

        for tries in range(10):
            self.set_state(self.comm.poll_state())
            if self.state=='command':
                return
            wx.Yield()
        print "Might have failed to stop"
        
    def handle_start(self,evt):
        self.comm.write("sample\r\n")

        for tries in range(10):
            self.set_state(self.comm.poll_state())
            if self.state=='sampling':
                return
            wx.Yield()
        print "Might have failed to start"

    def handle_query_clock(self,evt):
        if not self.comm.connected:
            print "NOT CONNECTED"
            return
        fb_dt,local_dt=self.comm.query_datetimes()

        fmt="%Y-%m-%d %H:%M:%S.%f"
        if fb_dt is not None:
            self.local_dt_display.SetLabel(local_dt.strftime(fmt))
        else:
            self.local_dt_display.SetLabel("n/a")
        if local_dt is not None:
            self.device_dt_display.SetLabel(fb_dt.strftime(fmt))
        else:
            self.device_dt_display.SetLabel("n/a")
            
        
    def handle_sync_clock(self,evt):
        if not self.comm.connected:
            print "NOT CONNECTED"
            return
        self.comm.sync_datetime_to_local_machine()
        
    @property
    def port_short_name(self):
        return os.path.basename(self.port_path)
        
class ConnectDialog(wx.SingleChoiceDialog):
    """ A dialog frame which shows options for connected to device.
    if successful, creates a ConnectedDeviceHUD
    """
    def __init__(self,parent):
        self.fbcomm=device.FreebirdComm()
        self.ports=self.fbcomm.available_serial_ports()
        port_names=[port[0] for port in self.ports]
        
        super(ConnectDialog,self).__init__(parent,message="Choose a port",caption="Connect to Freebird",
                                           choices=port_names)
    def selected_port(self):
        return self.ports[self.GetSelection()]
    

class DatafileViewer(wx.Frame):
    """ Browse, convert, calibrate, etc. a set of output files
    stored locally.
    """
    def __init__(self,parent,title,filename):
        super(DatafileViewer,self).__init__(parent, title=title, size=(300,400))
        self.filename=filename
        
        self.fbfile=datafile.freebird_file_factory(self.filename)
        
        #HERE: when/where do we get this from the user?
        self.fbfile.serials['squid']='SN104'
        self.fbfile.serials['sbe7probe']='C175'
        
        self.grid=wx.GridBagSizer(hgap=5,vgap=5)
        label=wx.StaticText(self, label="File: %s"%(self.filename))
        self.grid.Add(label,pos=(0,0),span=(1,5))

        # Various attributes:
        self.grid.Add(wx.StaticText(self,label="Size [bytes]:"),pos=(1,0),span=(1,2))
        self.grid.Add(wx.StaticText(self,label="{:,}".format(self.fbfile.nbytes)),pos=(1,2),span=(1,3))

        self.grid.Add(wx.StaticText(self,label="Start:"),pos=(2,0),span=(1,2))
        self.start_text=wx.StaticText(self,label="n/a")
        self.grid.Add(self.start_text,pos=(2,2),span=(1,2))

        self.grid.Add(wx.StaticText(self,label="End:"),pos=(3,0),span=(1,2))
        self.end_text=wx.StaticText(self,label="n/a")
        self.grid.Add(self.end_text,pos=(3,2),span=(1,2))

        self.canvas=data_gui.CanvasPanel(self)
        self.grid.Add(self.canvas,pos=(5,0),span=(5,5))

        self.SetSizerAndFit(self.grid)

        self.Show(True)

        self.parse()

        self.canvas.show_data(self.fbfile)

    def update_attributes(self):
        if self.fbfile.data is None:
            self.start_text.SetLabel('n/a')
            self.end_text.SetLabel('n/a')
        else:
            self.start_text.SetLabel(num2date(self.data['dn_py'][0]).strftime("%Y-%m-%d %H:%M:%S"))
            self.end_text.SetLabel(num2date(self.data['dn_py'][-1]).strftime("%Y-%m-%d %H:%M:%S"))
    
    def parse(self):
        pd=wx.ProgressDialog("File load","Loading %s"%(os.path.basename(self.filename)),
                             parent=self)
        pd.Show(True)
                             
        def worker():
            self.data=self.fbfile.read_all()
        thr=threading.Thread(target=worker)
        thr.start()

        print "Starting parsing"
        while thr.isAlive():
            pd.Update(int(90*self.fbfile.progress))
            thr.join(0.0)
            wx.Yield()
        print "Done parsing"
        pd.Destroy()

        # Update some info about the file
        self.update_attributes()

app=FreebirdGui()
app.MainLoop()

# it's giving some errors on wx.App not being created
# this might be due to running the script over and over, while wx
# seems to assume that there is only exactly one App, ever.
