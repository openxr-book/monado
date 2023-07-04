import eel
from bottle import app
from ctypes import *
import time
import sys

# Load the shared library
lib_monado = CDLL("./libmonado-ctl-gui.so")
# cdll.LoadLibrary("./libmonado-ctl-gui.so")
# lib_monado.printf

eel.init('eel_web', allowed_extensions=['.js','.html'])
# ctl.main_loop()

@eel.expose
def say_hello_py(x):
    print("Hello from %s" % x)
    
say_hello_py('Python World')
int connectection = lib_monado.get_mode
eel.displayConnectedDevices('Javascript World !') #Calling the dISPL

lib_monado.main_eel()
eel.start('gui_eel.html', mode='brave' , size=(300, 200)) 
