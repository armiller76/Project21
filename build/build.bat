@echo off

cl -Zi -DPROJECT21_SLOW=1 -DPROJECT21_INTERNAL=1 ..\src\win32_newproject21.cpp user32.lib gdi32.lib
