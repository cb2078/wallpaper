@echo off
cl /nologo /Zi /W4 /wd4996 /wd4505 /wd4127 /O2 main.c
start /b update_readme.bat > readme.md
