# Microsoft Developer Studio Generated NMAKE File, Based on XMP Legacy Scrobbler.dsp
!IF "$(CFG)" == ""
CFG=XMP Legacy Scrobbler - Win32 Debug
!MESSAGE No configuration specified. Defaulting to XMP Legacy Scrobbler - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "XMP Legacy Scrobbler - Win32 Release" && "$(CFG)" != "XMP Legacy Scrobbler - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "XMP Legacy Scrobbler.mak" CFG="XMP Legacy Scrobbler - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "XMP Legacy Scrobbler - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "XMP Legacy Scrobbler - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "XMP Legacy Scrobbler - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\xmp-legacyscrobbler.dll" "$(OUTDIR)\XMP Legacy Scrobbler.pch"


CLEAN :
	-@erase "$(INTDIR)\cacheManager.obj"
	-@erase "$(INTDIR)\https.obj"
	-@erase "$(INTDIR)\lfm.obj"
	-@erase "$(INTDIR)\logManager.obj"
	-@erase "$(INTDIR)\Resource.res"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\XMP Legacy Scrobbler.obj"
	-@erase "$(INTDIR)\XMP Legacy Scrobbler.pch"
	-@erase "$(OUTDIR)\xmp-legacyscrobbler.dll"
	-@erase "$(OUTDIR)\xmp-legacyscrobbler.exp"
	-@erase "$(OUTDIR)\xmp-legacyscrobbler.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "XMPLEGACYSCROBBLER_EXPORTS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x1009 /fo"$(INTDIR)\Resource.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\XMP Legacy Scrobbler.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /incremental:no /pdb:"$(OUTDIR)\xmp-legacyscrobbler.pdb" /machine:I386 /def:".\xmp-sdk\xmpdsp.def" /out:"$(OUTDIR)\xmp-legacyscrobbler.dll" /implib:"$(OUTDIR)\xmp-legacyscrobbler.lib" 
DEF_FILE= \
	".\xmp-sdk\xmpdsp.def"
LINK32_OBJS= \
	"$(INTDIR)\cacheManager.obj" \
	"$(INTDIR)\https.obj" \
	"$(INTDIR)\lfm.obj" \
	"$(INTDIR)\logManager.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\XMP Legacy Scrobbler.obj" \
	"$(INTDIR)\Resource.res"

"$(OUTDIR)\xmp-legacyscrobbler.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "XMP Legacy Scrobbler - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\xmp-legacyscrobbler.dll" "$(OUTDIR)\XMP Legacy Scrobbler.pch"


CLEAN :
	-@erase "$(INTDIR)\cacheManager.obj"
	-@erase "$(INTDIR)\https.obj"
	-@erase "$(INTDIR)\lfm.obj"
	-@erase "$(INTDIR)\logManager.obj"
	-@erase "$(INTDIR)\Resource.res"
	-@erase "$(INTDIR)\StdAfx.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\XMP Legacy Scrobbler.obj"
	-@erase "$(INTDIR)\XMP Legacy Scrobbler.pch"
	-@erase "$(OUTDIR)\xmp-legacyscrobbler.dll"
	-@erase "$(OUTDIR)\xmp-legacyscrobbler.exp"
	-@erase "$(OUTDIR)\xmp-legacyscrobbler.ilk"
	-@erase "$(OUTDIR)\xmp-legacyscrobbler.lib"
	-@erase "$(OUTDIR)\xmp-legacyscrobbler.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "XMPLEGACYSCROBBLER_EXPORTS" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x1009 /fo"$(INTDIR)\Resource.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\XMP Legacy Scrobbler.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /incremental:yes /pdb:"$(OUTDIR)\xmp-legacyscrobbler.pdb" /debug /machine:I386 /def:".\xmp-sdk\xmpdsp.def" /out:"$(OUTDIR)\xmp-legacyscrobbler.dll" /implib:"$(OUTDIR)\xmp-legacyscrobbler.lib" /pdbtype:sept 
DEF_FILE= \
	".\xmp-sdk\xmpdsp.def"
LINK32_OBJS= \
	"$(INTDIR)\cacheManager.obj" \
	"$(INTDIR)\https.obj" \
	"$(INTDIR)\lfm.obj" \
	"$(INTDIR)\logManager.obj" \
	"$(INTDIR)\StdAfx.obj" \
	"$(INTDIR)\XMP Legacy Scrobbler.obj" \
	"$(INTDIR)\Resource.res"

"$(OUTDIR)\xmp-legacyscrobbler.dll" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("XMP Legacy Scrobbler.dep")
!INCLUDE "XMP Legacy Scrobbler.dep"
!ELSE 
!MESSAGE Warning: cannot find "XMP Legacy Scrobbler.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "XMP Legacy Scrobbler - Win32 Release" || "$(CFG)" == "XMP Legacy Scrobbler - Win32 Debug"
SOURCE=.\cacheManager.cpp

"$(INTDIR)\cacheManager.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\https.cpp

"$(INTDIR)\https.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\lfm.cpp

"$(INTDIR)\lfm.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\logManager.cpp

"$(INTDIR)\logManager.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\StdAfx.cpp

!IF  "$(CFG)" == "XMP Legacy Scrobbler - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "XMPLEGACYSCROBBLER_EXPORTS" /Fp"$(INTDIR)\XMP Legacy Scrobbler.pch" /Yc"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\StdAfx.obj"	"$(INTDIR)\XMP Legacy Scrobbler.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "XMP Legacy Scrobbler - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "XMPLEGACYSCROBBLER_EXPORTS" /Fp"$(INTDIR)\XMP Legacy Scrobbler.pch" /Yc"stdafx.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\StdAfx.obj"	"$(INTDIR)\XMP Legacy Scrobbler.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=".\XMP Legacy Scrobbler.cpp"

"$(INTDIR)\XMP Legacy Scrobbler.obj" : $(SOURCE) "$(INTDIR)"


SOURCE=.\Resource.rc

"$(INTDIR)\Resource.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)



!ENDIF 

