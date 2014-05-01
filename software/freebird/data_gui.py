import numpy as np
import matplotlib
matplotlib.use('WXAgg')

from matplotlib.backends.backend_wxagg import FigureCanvasWxAgg as FigureCanvas
from matplotlib.backends.backend_wx import NavigationToolbar2Wx
from matplotlib.figure import Figure


import wx

class CanvasPanel(wx.Panel):
    def __init__(self, parent):
        wx.Panel.__init__(self, parent)
        self.figure = Figure()
        self.axes = self.figure.add_subplot(111)
        self.canvas = FigureCanvas(self, -1, self.figure)
        self.sizer = wx.BoxSizer(wx.VERTICAL)
        self.sizer.Add(self.canvas, 1, wx.LEFT | wx.TOP | wx.GROW)
        self.SetSizer(self.sizer)
        self.Fit()

    def show_data(self,fbfile,fields=['cond','imu_a']):
        if fbfile.data is None:
            return
        
        self.figure.clear()
        
        axs=[]
        for axi,varname in enumerate(fields):
            if axi==0:
                sharex=None
            else:
                sharex=axs[0]

            ax=self.figure.add_subplot(len(fields),1,axi+1,sharex=sharex)
            axs.append(ax)
            ax.plot_date( fbfile.data['dn_py'],
                          fbfile.data[varname],
                          'g-')
            
        self.figure.autofmt_xdate()

        # Not sure how to trigger it to actually draw things.
        self.canvas.draw()
        self.Fit()
