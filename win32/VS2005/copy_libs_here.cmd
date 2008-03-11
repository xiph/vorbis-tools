@ECHO OFF
SET VORBIS_DIR=..\..\..\libvorbis\win32\VS2005
SET VORBIS_DEBUG=%VORBIS_DIR%\debug
SET VORBIS_RELEASE=%VORBIS_DIR%\release

SET OGG_DIR=..\..\..\libogg\win32\VS2005\
SET OGG_DEBUG=%OGG_DIR%\debug
SET OGG_RELEASE=%OGG_DIR%\release

SET FLAC_DEBUG=..\..\..\flac\obj\debug\lib
SET FLAC_RELEASE=..\..\..\flac\obj\release\lib


COPY %VORBIS_DEBUG%\libvorbis.dll debug\
COPY %VORBIS_RELEASE%\libvorbis.dll release\

COPY %VORBIS_DEBUG%\libvorbisfile.dll debug\
COPY %VORBIS_RELEASE%\libvorbisfile.dll release\

COPY %OGG_DEBUG%\libogg.dll debug\
COPY %OGG_RELEASE%\libogg.dll release\

COPY %FLAC_DEBUG%\libFLAC_dynamic.dll debug\
COPY %FLAC_RELEASE%\libFLAC_dynamic.dll release\

PAUSE
