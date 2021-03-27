@echo off

REM Common compiler flags:
set CommonCompilerFlags= -MT -nologo -EHa- -Fmwin32_newproject21.map -Gm- -GR- -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -Z7 
set CommonLinkerIncludes= user32.lib gdi32.lib winmm.lib
set CommonLinkerFlags= -opt:ref

REM 64 bit build:
cl %CommonCompilerFlags% -DPROJECT21_SLOW=1 -DPROJECT21_INTERNAL=1 ..\src\win32_newproject21.cpp /link %CommonLinkerFlags% %CommonLinkerIncludes%

REM 32 bit build:
REM cl  %CommonCompilerFlags% -DPROJECT21_SLOW=1 -DPROJECT21_INTERNAL=1 ..\src\win32_newproject21.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags% user32.lib gdi32.lib

REM ignoring warnings: 4201-nameless structs; 4100 unreferenced formal parameters; 4189 variable initialized but not referenced
REM -Oi (use compiler intrinsics when able); -WX -W4 (warnings as errors, warning level 4); -GR- disable runtime type info; -EHa- disable exception handling
REM -MT (use static DLLs [for compatibility]); -Gm- (no incremental BS);
