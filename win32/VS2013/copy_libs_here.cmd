@ECHO OFF

MKDIR Release\ Debug\

SET VORBIS_DIR=..\..\..\vorbis\win32\VS2010
SET VORBIS_DEBUG=%VORBIS_DIR%\Win32\Debug
SET VORBIS_RELEASE=%VORBIS_DIR%\Win32\Release

SET OGG_DIR=..\..\..\ogg\win32\VS2015
SET OGG_DEBUG=%OGG_DIR%\Win32\Debug
SET OGG_RELEASE=%OGG_DIR%\Win32\Release

SET FLAC_DEBUG=..\..\..\flac\objs\debug\lib
SET FLAC_RELEASE=..\..\..\flac\objs\release\lib


COPY /Y %VORBIS_DEBUG%\libvorbis.dll Debug\
COPY /Y %VORBIS_RELEASE%\libvorbis.dll Release\

COPY /Y %VORBIS_DEBUG%\libvorbisfile.dll Debug\
COPY /Y %VORBIS_RELEASE%\libvorbisfile.dll Release\

COPY /Y %OGG_DEBUG%\libogg.dll Debug\
COPY /Y %OGG_RELEASE%\libogg.dll Release\

COPY /Y %FLAC_DEBUG%\libFLAC_dynamic.dll Debug\
COPY /Y %FLAC_RELEASE%\libFLAC_dynamic.dll Release\

PAUSE
