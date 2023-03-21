# Build/debug with Visual Studio:

Copy FFMpeg shared libs in directory `bin\ffdlls\ `  

avcodec-57.dll  
avdevice-57.dll  
avfilter-6.dll  
avformat-57.dll  
avutil-55.dll  
postproc-54.dll  
swresample-2.dll  
swscale-4.dll  

FFMpeg binaries last used: 20170827-ef0c6d9 (Zeranoe)  
I put backup copies here: https://sourceforge.net/projects/vdfiltermod/files/ffmpeg/  

# Build with static linking to FFMpeg:

I'm using GCC for this. Roughly the process is:

* build FFMpeg (using media-autobuild_suite)  
* build cineform (using cineform\build32.bat)  
* build plugin (using gcc\build32.bat)  

On last build, these patches were applied to FFMpeg (compared to Zeranoe shared build):  

riff.c
```
    { AV_CODEC_ID_H265,         MKTAG('H', 'E', 'V', 'C') },
```

ffv1_enc.c etc updated to head
