@set msys=E:\work\ffmpeg_build\msys64
@set bin=%msys%\mingw32\bin
@path %path%;%bin%;%msys%\usr\bin
@set lib=%msys%\..\local32\lib
@set include=%msys%\..\local32\include
@set out=obj32

@call build
