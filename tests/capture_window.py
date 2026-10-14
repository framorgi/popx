"""Capture the SFML test process's window; never capture the desktop or another process."""
import ctypes as c
import sys
from PIL import Image
x=c.CDLL('libX11.so.6')
x.XOpenDisplay.restype=c.c_void_p
x.XDefaultRootWindow.argtypes=[c.c_void_p];x.XDefaultRootWindow.restype=c.c_ulong
x.XInternAtom.argtypes=[c.c_void_p,c.c_char_p,c.c_int];x.XInternAtom.restype=c.c_ulong
x.XQueryTree.argtypes=[c.c_void_p,c.c_ulong,c.POINTER(c.c_ulong),c.POINTER(c.c_ulong),c.POINTER(c.POINTER(c.c_ulong)),c.POINTER(c.c_uint)]
x.XGetWindowProperty.argtypes=[c.c_void_p,c.c_ulong,c.c_ulong,c.c_long,c.c_long,c.c_int,c.c_ulong,c.POINTER(c.c_ulong),c.POINTER(c.c_int),c.POINTER(c.c_ulong),c.POINTER(c.c_ulong),c.POINTER(c.c_void_p)]
x.XFree.argtypes=[c.c_void_p]
x.XFetchName.argtypes=[c.c_void_p,c.c_ulong,c.POINTER(c.c_void_p)]
x.XGetGeometry.argtypes=[c.c_void_p,c.c_ulong,c.POINTER(c.c_ulong),c.POINTER(c.c_int),c.POINTER(c.c_int),c.POINTER(c.c_uint),c.POINTER(c.c_uint),c.POINTER(c.c_uint),c.POINTER(c.c_uint)]
x.XGetImage.argtypes=[c.c_void_p,c.c_ulong,c.c_int,c.c_int,c.c_uint,c.c_uint,c.c_ulong,c.c_int];x.XGetImage.restype=c.c_void_p
x.XDestroyImage.argtypes=[c.c_void_p]
x.XCloseDisplay.argtypes=[c.c_void_p]
class XImage(c.Structure):
    _fields_=[('width',c.c_int),('height',c.c_int),('xoffset',c.c_int),('format',c.c_int),('data',c.c_void_p),('byte_order',c.c_int),('bitmap_unit',c.c_int),('bitmap_bit_order',c.c_int),('bitmap_pad',c.c_int),('depth',c.c_int),('bytes_per_line',c.c_int),('bits_per_pixel',c.c_int),('red_mask',c.c_ulong),('green_mask',c.c_ulong),('blue_mask',c.c_ulong)]
display=x.XOpenDisplay(None)
if not display:raise SystemExit('No display')
try:
    root=x.XDefaultRootWindow(display);atom=x.XInternAtom(display,b'_NET_WM_PID',0)
    queue=[root];target=None
    while queue:
        window=queue.pop();root_return=c.c_ulong();parent=c.c_ulong();children=c.POINTER(c.c_ulong)();count=c.c_uint()
        if x.XQueryTree(display,window,c.byref(root_return),c.byref(parent),c.byref(children),c.byref(count)):
            queue.extend(children[i] for i in range(count.value))
            if children:x.XFree(children)
        actual=c.c_ulong();fmt=c.c_int();n=c.c_ulong();left=c.c_ulong();data=c.c_void_p()
        x.XGetWindowProperty(display,window,atom,0,1,0,0,c.byref(actual),c.byref(fmt),c.byref(n),c.byref(left),c.byref(data))
        if data:
            if n.value and fmt.value==32 and c.cast(data,c.POINTER(c.c_ulong))[0]==int(sys.argv[1]):target=window
            x.XFree(data)
        name=c.c_void_p()
        if x.XFetchName(display,window,c.byref(name)) and name:
            if c.string_at(name)==('PopX Simulation ['+sys.argv[1]+']').encode():target=window
            x.XFree(name)
        if target:break
    if not target:raise SystemExit('Owned window not found')
    a=c.c_int();b=c.c_int();width=c.c_uint();height=c.c_uint();border=c.c_uint();depth=c.c_uint();r=c.c_ulong()
    x.XGetGeometry(display,target,c.byref(r),c.byref(a),c.byref(b),c.byref(width),c.byref(height),c.byref(border),c.byref(depth))
    ptr=x.XGetImage(display,target,0,0,width.value,height.value,c.c_ulong(-1).value,2)
    if not ptr:raise SystemExit('Capture unavailable')
    try:
        image=c.cast(ptr,c.POINTER(XImage)).contents
        if image.bits_per_pixel!=32 or image.byte_order!=0:raise SystemExit('Unsupported X image layout')
        Image.frombytes('RGB',(image.width,image.height),c.string_at(image.data,image.bytes_per_line*image.height),'raw','BGRX',image.bytes_per_line).save(sys.argv[2])
    finally:x.XDestroyImage(ptr)
finally:x.XCloseDisplay(display)
