#!/bin/bash

DEBUG=1

CC=""
CFLAGS_COMMON=""
if [[ $DEBUG == "1" ]]; then
    CC="gcc"
    CFLAGS_COMMON="-std=c99 -Wall -O0 -g"
else
    CC="clang"
    CFLAGS_COMMON="-std=c99 -Wall -DNDEBUG -O2"
fi


mkdir -p build


echo "========================="
echo "compiling asset generator"
echo "========================="
SOURCES="code/asset_generator/asset_generator.c"
LFLAGS="-lm"
$CC $CFLAGS_COMMON $SOURCES $LFLAGS -o build/asset_generator
echo ""
echo ""



echo "========================="
echo "compiling florilia"
echo "========================="
# ./build/asset_generator # update assets memory
SOURCES="code/client/florilia.c code/client/linux/lin_florilia.c"
LFLAGS="-lX11 -lGL -lasound -lm"
$CC $CFLAGS_COMMON $SOURCES $LFLAGS -o build/florilia
echo ""
echo ""


echo "========================="
echo "compiling florilia_server"
echo "========================="
SOURCES="code/server/florilia_server.c"
LFLAGS=""
$CC $CFLAGS_COMMON $SOURCES $LFLAGS -o build/florilia_server
echo ""
echo ""
