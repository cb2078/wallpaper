@echo off
:: cl /nologo /Zi /W4 /wd4996 /wd4505 /wd4127 /O2 main.c
clang -g -O2 -Wall -Wno-unused-function -Wno-logical-op-parentheses -Wno-deprecated-declarations -Wno-gnu-folding-constant -o main.exe main.c
