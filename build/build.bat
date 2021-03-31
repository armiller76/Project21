@echo off

REM Common compiler flags:
set CommonCompilerFlags= -MTd -nologo -EHa- -Gm- -GR- -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -Z7 

set PlatformLinkerArgs= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib
set ApplicationLinkerFlags= -incremental:no -EXPORT:ApplicationUpdate -EXPORT:ApplicationGetSound -PDB:project21_%random%.pdb

REM 64 bit build:
del *.pdb >nul 2>nul
cl %CommonCompilerFlags% -DPROJECT21_SLOW=1 -DPROJECT21_INTERNAL=1 -Fmproject21.map ..\src\project21.cpp -LD -link %ApplicationLinkerFlags%
cl %CommonCompilerFlags% -DPROJECT21_SLOW=1 -DPROJECT21_INTERNAL=1 -Fmwin32_newproject21.map ..\src\win32_newproject21.cpp -link %PlatformLinkerArgs%

REM 32 bit build:
REM cl  %CommonCompilerFlags% -DPROJECT21_SLOW=1 -DPROJECT21_INTERNAL=1 ..\src\project21.cpp -LD -link -subsystem:windows,5.1 %CommonLinkerIncludes%
REM cl  %CommonCompilerFlags% -DPROJECT21_SLOW=1 -DPROJECT21_INTERNAL=1 ..\src\win32_newproject21.cpp -link -subsystem:windows,5.1 %CommonLinkerFlags% %CommonLinkerIncludes%

REM ignoring warnings: 4201-nameless structs; 4100 unreferenced formal parameters; 4189 variable initialized but not referenced
REM -Oi (use compiler intrinsics when able); -WX -W4 (warnings as errors,  warning level 4); -GR- disable runtime type info; -EHa- disable exception handling
REM -MT (use static DLLs [for compatibility]); -Gm- (no incremental BS);
