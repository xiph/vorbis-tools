# Microsoft Developer Studio Project File - Name="oggenc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=oggenc - Win32 Release DLL
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "oggenc.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "oggenc.mak" CFG="oggenc - Win32 Release DLL"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "oggenc - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "oggenc - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "oggenc - Win32 Debug DLL" (based on "Win32 (x86) Console Application")
!MESSAGE "oggenc - Win32 Release DLL" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "oggenc - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release\static"
# PROP Intermediate_Dir "Release\static"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\include" /I "..\..\vorbis\include" /I "..\..\ogg\include" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 ogg_static.lib vorbis_static.lib vorbisenc_static.lib /nologo /subsystem:console /machine:I386 /libpath:"..\..\ogg\win32\Static_Release" /libpath:"..\..\vorbis\win32\Vorbis_Static_Release" /libpath:"..\..\vorbis\win32\VorbisEnc_Static_Release"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "oggenc - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug\static"
# PROP Intermediate_Dir "Debug\static"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\include" /I "..\..\vorbis\include" /I "..\..\ogg\include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ogg_static_d.lib vorbis_static_d.lib vorbisenc_static_d.lib /nologo /subsystem:console /verbose /debug /machine:I386 /pdbtype:sept /libpath:"..\..\ogg\win32\Static_Debug" /libpath:"..\..\vorbis\win32\Vorbis_Static_Debug" /libpath:"..\..\vorbis\win32\VorbisEnc_Static_Debug"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "oggenc - Win32 Debug DLL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "oggenc___Win32_Debug_DLL"
# PROP BASE Intermediate_Dir "oggenc___Win32_Debug_DLL"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug\dynamic"
# PROP Intermediate_Dir "Debug\dynamic"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\include" /I "..\..\vorbis\include" /I "..\..\ogg\include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FD /GZ /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\include" /I "..\..\vorbis\include" /I "..\..\ogg\include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 ogg_static_d.lib vorbis_static_d.lib vorbisenc_static_d.lib /nologo /subsystem:console /debug /machine:I386 /out:"Debug\static\oggenc.exe" /pdbtype:sept /libpath:"..\..\ogg\win32\Static_Debug" /libpath:"..\..\vorbis\win32\Vorbis_Static_Debug" /libpath:"..\..\vorbis\win32\VorbisEnc_Static_Debug"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 ogg_d.lib vorbis_d.lib vorbisenc_d.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept /libpath:"..\..\ogg\win32\Dynamic_Debug" /libpath:"..\..\vorbis\win32\Vorbis_Dynamic_Debug" /libpath:"..\..\vorbis\win32\VorbisEnc_Dynamic_Debug"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "oggenc - Win32 Release DLL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "oggenc___Win32_Release_DLL"
# PROP BASE Intermediate_Dir "oggenc___Win32_Release_DLL"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Release\dynamic"
# PROP Intermediate_Dir "Release\dynamic"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\include" /I "..\..\vorbis\include" /I "..\..\ogg\include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FD /GZ /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MD /W3 /Gm /GX /ZI /Od /I "..\include" /I "..\..\vorbis\include" /I "..\..\ogg\include" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 ogg_d.lib vorbis_d.lib vorbisenc_d.lib /nologo /subsystem:console /debug /machine:I386 /out:"Debug\dynamic\oggenc.exe" /pdbtype:sept /libpath:"..\..\ogg\win32\Dynamic_Debug" /libpath:"..\..\vorbis\win32\Vorbis_Dynamic_Debug" /libpath:"..\..\vorbis\win32\VorbisEnc_Dynamic_Debug"
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 ogg.lib vorbis.lib vorbisenc.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept /libpath:"..\..\ogg\win32\Dynamic_Release" /libpath:"..\..\vorbis\win32\Vorbis_Dynamic_Release" /libpath:"..\..\vorbis\win32\VorbisEnc_Dynamic_Release"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "oggenc - Win32 Release"
# Name "oggenc - Win32 Debug"
# Name "oggenc - Win32 Debug DLL"
# Name "oggenc - Win32 Release DLL"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\audio.c
# End Source File
# Begin Source File

SOURCE=.\encode.c
# End Source File
# Begin Source File

SOURCE=..\share\getopt.c
# End Source File
# Begin Source File

SOURCE=..\share\getopt1.c
# End Source File
# Begin Source File

SOURCE=.\oggenc.c
# End Source File
# Begin Source File

SOURCE=.\platform.c
# End Source File
# Begin Source File

SOURCE=..\share\utf8.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\audio.h
# End Source File
# Begin Source File

SOURCE=.\encode.h
# End Source File
# Begin Source File

SOURCE=..\include\getopt.h
# End Source File
# Begin Source File

SOURCE=.\platform.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
