@set msys=E:\work\ffmpeg_build\msys64
@set bin=%msys%\mingw64\bin
@path %path%;%bin%;%msys%\usr\bin
@set lib=%msys%\..\local64\lib
@set include=%msys%\..\local64\include
@set libcfhd=..\cineform\obj64\libcfhd.a
@set out=obj64

@call build
