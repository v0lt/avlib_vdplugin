@ECHO OFF
SETLOCAL ENABLEDELAYEDEXPANSION
CD /D %~dp0

REM -------------------------------------

FOR /F "tokens=2*" %%A IN (
  'REG QUERY "HKLM\SOFTWARE\7-Zip" /v "Path" 2^>NUL ^| FIND "REG_SZ" ^|^|
   REG QUERY "HKLM\SOFTWARE\Wow6432Node\7-Zip" /v "Path" 2^>NUL ^| FIND "REG_SZ"') DO SET "SEVENZIP=%%B\7z.exe"

IF NOT EXIST "%SEVENZIP%" (
  ECHO 7Zip not found.
  GOTO :END
)

REM -------------------------------------

SET "FFMPEG_DLLS=avcodec-62.dll avformat-62.dll avutil-60.dll swresample-6.dll swscale-9.dll"
SET "FFMPEG_LIBS=avcodec.lib avformat.lib avutil.lib swresample.lib swscale.lib"
SET "FFMPEG_DEFS=avcodec-62.def avformat-62.def avutil-60.def swresample-6.def swscale-9.def"

SET /A COUNT=0
FOR %%D IN (%FFMPEG_DLLS%) DO IF EXIST "_bin\ffmpeg_win32\%%D" SET /A COUNT+=1
FOR %%D IN (%FFMPEG_LIBS%) DO IF EXIST "ffmpeg\lib_win32\%%D" SET /A COUNT+=1
FOR %%D IN (%FFMPEG_DEFS%) DO IF EXIST "ffmpeg\lib_win32\%%D" SET /A COUNT+=1

IF %COUNT% EQU 15 (
  ECHO FFmpeg files are already downloaded and unpacked.
  GOTO :END
)

SET FFMPEG_ZIP=ffmpeg-8.0-windows-desktop-vs2022-default.7z

IF NOT EXIST _bin\ffmpeg_win32\%FFMPEG_ZIP% (
  ECHO Downloading "%FFMPEG_ZIP%"...
  MKDIR _bin\ffmpeg_win32
  curl -o "_bin\ffmpeg_win32\%FFMPEG_ZIP%" --insecure -L "https://sourceforge.net/projects/avbuild/files/windows-desktop/%FFMPEG_ZIP%/download"
)

IF NOT EXIST _bin\ffmpeg_win32\%FFMPEG_ZIP% (
  ECHO Failed to download %FFMPEG_ZIP%.
  GOTO :END
)

SET PATH_DLLS=
FOR %%D IN (%FFMPEG_DLLS%) DO SET PATH_DLLS=!PATH_DLLS! *\x86\%%D
SET PATH_LIBS=
FOR %%D IN (%FFMPEG_LIBS%) DO SET PATH_LIBS=!PATH_LIBS! *\x86\%%D
FOR %%D IN (%FFMPEG_DEFS%) DO SET PATH_LIBS=!PATH_LIBS! *\x86\%%D

"%SEVENZIP%" e "_bin\ffmpeg_win32\%FFMPEG_ZIP%" -o"_bin\ffmpeg_win32\" %PATH_DLLS% -r -aos
"%SEVENZIP%" e "_bin\ffmpeg_win32\%FFMPEG_ZIP%" -o"ffmpeg\lib_win32\" %PATH_LIBS% -r -aos

:END
ENDLOCAL
TIMEOUT /T 5
EXIT /B
