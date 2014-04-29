import wx
import os

ID_CONNECT=wx.NewId()

import freebird_api as api

class FreebirdGui(wx.App):
    def __init__(self):
        # don't redirect stdout/stderr
        super(FreebirdGui,self).__init__(False)

        self.frame=MainFrame(None,"Fly high, freebird")


class MainFrame(wx.Frame):
    def __init__(self,parent,title):
        super(MainFrame,self).__init__(parent, title=title, size=(400,300))

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
            dc=DataCollection(self,"Data collection",
                              paths=[os.path.join(dlg.GetDirectory(),dlg.GetFilename())])
            dlg.Destroy()

class ConnectedDeviceHUD(wx.Panel):
    """ A window holding widgets for info about a connected device.
    roughly 1:1 correspondence with freebird_api.FreebirdComm, though
    this doesn't get instantiated until a specific port has been selected.
    """
    def __init__(self,parent,port_path):
        super(ConnectedDeviceHUD,self).__init__(parent)
        self.port_path=port_path
        self.comm=api.FreebirdComm()
        self.state='unknown'

        self.grid=wx.GridBagSizer(hgap=5,vgap=5)
        
        label=wx.StaticText(self, label="Freebird on %s"%self.port_path)
        self.grid.Add(label,pos=(0,0),span=(1,5))
        
        self.connect_btn=wx.Button(self, label="Connect")
        self.grid.Add(self.connect_btn,pos=(1,0))
        self.Bind(wx.EVT_BUTTON,self.handle_connect,self.connect_btn)
        
        self.disconnect_btn=wx.Button(self,label="Disconnect")
        self.grid.Add(self.disconnect_btn,pos=(1,1),span=(1,2))
        self.Bind(wx.EVT_BUTTON,self.handle_disconnect,self.disconnect_btn)

        self.grid.Add((1,40),pos=(2,0))
                      
        # Clock:
        self.query_clock_btn=wx.Button(self,label="Query clock")
        self.grid.Add(self.query_clock_btn,pos=(3,0))
        self.Bind(wx.EVT_BUTTON,self.handle_query_clock,self.query_clock_btn)
        self.sync_clock_btn=wx.Button(self,label="Sync clock")
        self.grid.Add(self.sync_clock_btn,pos=(4,0))
        self.Bind(wx.EVT_BUTTON,self.handle_sync_clock,self.sync_clock_btn)

        self.grid.Add( wx.StaticText(self,label="Local:"),pos=(3,1))
        self.grid.Add( wx.StaticText(self,label="Freebird:"),pos=(4,1))
        
        self.local_dt_display=wx.StaticText(self,label="---")
        self.grid.Add(self.local_dt_display,pos=(3,2),span=(1,5))
        self.device_dt_display=wx.StaticText(self,label="---")
        self.grid.Add(self.device_dt_display,pos=(4,2),span=(1,5))


        # Serial console:
        self.serial_input=wx.TextCtrl(parent=self,size=(140,-1),style=wx.TE_PROCESS_ENTER)
        self.grid.Add(self.serial_input,pos=(6,0),span=(1,5))
        self.Bind(wx.EVT_TEXT_ENTER,self.handle_serial_input,self.serial_input)
        self.listener=self.comm.add_listener()
        self.Bind(wx.EVT_IDLE,self.poll_serial)
        
        self.sercon=wx.TextCtrl(parent=self,style=wx.TE_MULTILINE|wx.TE_READONLY,
                                pos=(10,10),size=(300,300))
        self.sercon.AppendText('Hello')
        self.grid.Add(self.sercon,pos=(7,0),span=(4,10))
        
        self.SetSizerAndFit(self.grid)

        self.set_state('disconnected')

    # Would like to show serial interactions, while also allowing pre-packaged 
    # interactions.  So any characters coming back from the comm should show
    # up in the console here - but what is the right polling mechanism here?
    # (a) the GUI polls the comm
    # (b) the comm runs a thread which is always reading from the serial port.
    # (c) what if the comm runs a reading thread, always logging from serial to
    #     some number
    def handle_serial_input(self,event):
        txt=self.serial_input.GetValue()
        print "Got text:",txt
        if state=='connected':
            self.comm.write(txt)
    def poll_serial(self,event):
        buff=self.listener.read(timeout=0)
        if len(buff):
            self.sercon.AppendText(buff)
                
    def set_state(self,state):
        """ Update GUI controls to reflect a particular state
        """
        if state=='disconnected':
            self.connect_btn.Enable()
            self.disconnect_btn.Disable()
            self.query_clock_btn.Disable()
            self.sync_clock_btn.Disable()
        elif state=='connected':
            self.connect_btn.Disable()
            self.disconnect_btn.Enable()
            self.query_clock_btn.Enable()
            self.sync_clock_btn.Enable()
        self.state=state
            
    def handle_connect(self,evt):
        if self.state!='disconnected':
            print "WARNING: call to connect, but state is %s"%self.state
            return
        
        if self.comm.connect(4) and self.comm.enter_command_mode():
            self.set_state('connected')
        else:
            print "WARNING: could not connect, or could not enter command mode"
            
    def handle_disconnect(self,evt):
        self.comm.disconnect()
        self.set_state('disconnected')
    def handle_query_clock(self,evt):
        if not self.comm.connected:
            print "NOT CONNECTED"
            return
        fb_dt,local_dt=self.comm.query_datetimes()

        fmt="%Y-%m-%d %H:%M:%S.%f"
        self.local_dt_display.SetLabel(local_dt.strftime(fmt))
        self.device_dt_display.SetLabel(fb_dt.strftime(fmt))
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
        self.fbcomm=api.FreebirdComm()
        self.ports=self.fbcomm.available_serial_ports()
        port_names=[port[0] for port in self.ports]
        
        super(ConnectDialog,self).__init__(parent,message="Choose a port",caption="Connect to Freebird",
                                           choices=port_names)
    def selected_port(self):
        return self.ports[self.GetSelection()]
    

class DataCollection(wx.Frame):
    """ Browse, convert, calibrate, etc. a set of output files
    stored locally.
    """
    def __init__(self,parent,title,paths):
        super(DataCollection,self).__init__(parent, title=title, size=(200,100))

        self.Show(True)

app=FreebirdGui()
app.MainLoop()

# it's giving some errors on wx.App not being created
# this might be due to running the script over and over, while wx
# seems to assume that there is only exactly one App, ever.
