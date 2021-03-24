@echo off
REM ignoring warnings: 4201-nameless structs; 4100 unreferenced formal parameters; 4189 variable initialized but not referenced
REM -Oi (use compiler intrinsics when able); -WX -W4 (warnings as errors, warning level 4); -GR- disable runtime type info; -EHa- disable exception handling
REM -MT (use static DLLs [for compatibility]); -Gm- (no incremental BS);  
cl -MT -nologo -EHa- -Fmwin32_newproject21.map -Gm- -GR- -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -Z7 -DPROJECT21_SLOW=1 -DPROJECT21_INTERNAL=1 ..\src\win32_newproject21.cpp /link -opt:ref user32.lib gdi32.lib
