@set bin=e:\work\mingw\bin
@path "%path%;%bin%"

@%bin%\gdb --args E:\work\vd_filterMod\out\Debug\VirtualDub.exe /F %CD%\obj32\avlib-1.vdplugin E:\shift_dev\data\test\cfhd\vfw_yuyv.avi 
