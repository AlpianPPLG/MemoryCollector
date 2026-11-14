@echo off
set GCC=C:\msys64\ucrt64\bin\gcc.exe
if not exist "%GCC%" (
    set GCC=gcc.exe
)

if not exist "bin" mkdir bin

echo [BUILD] Compiling Systems Memory Collector and VM Engine with %GCC%...

set CFLAGS=-std=c23 -Wall -Wextra -O2 -Iinclude
set CORE_SRCS=src\mem_pal.c src\mem_alloc.c src\mem_roots.c src\mem_collector.c
set VM_SRCS=%CORE_SRCS% src\mem_vm.c

%GCC% %CFLAGS% %CORE_SRCS% examples\demo_basic.c -o bin\demo_basic.exe
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] Failed to compile demo_basic.exe & exit /b %ERRORLEVEL% )

%GCC% %CFLAGS% %CORE_SRCS% examples\demo_cycles.c -o bin\demo_cycles.exe
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] Failed to compile demo_cycles.exe & exit /b %ERRORLEVEL% )

%GCC% %CFLAGS% %CORE_SRCS% examples\demo_promotion.c -o bin\demo_promotion.exe
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] Failed to compile demo_promotion.exe & exit /b %ERRORLEVEL% )

%GCC% %CFLAGS% %CORE_SRCS% examples\demo_compaction.c -o bin\demo_compaction.exe
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] Failed to compile demo_compaction.exe & exit /b %ERRORLEVEL% )

%GCC% %CFLAGS% %VM_SRCS% examples\demo_vm.c -o bin\demo_vm.exe
if %ERRORLEVEL% NEQ 0 ( echo [ERROR] Failed to compile demo_vm.exe & exit /b %ERRORLEVEL% )

echo [SUCCESS] Build completed! All 5 executables ready in bin\
echo - bin\demo_basic.exe
echo - bin\demo_cycles.exe
echo - bin\demo_promotion.exe
echo - bin\demo_compaction.exe
echo - bin\demo_vm.exe
