@echo off

SET DEBUG=1

SET CC=clang
IF %DEBUG%==1 (
    SET CFLAGS_COMMON=-std=c99 -Wall -Wno-deprecated-declarations -O0 -g
) ELSE (
    SET CFLAGS_COMMON=-std=c99 -Wall -Wno-deprecated-declarations -DNDEBUG -O2
)

if NOT EXIST build MD build


SET SOURCES=./code/client/florilia.c ./code/client/win32/win32_florilia.c
SET LFLAGS=-lUser32 -lGdi32 -lWs2_32.lib
%CC% %CFLAGS_COMMON% %SOURCES% %LFLAGS% -o build/florilia.exe
