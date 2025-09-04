#!/bin/sh

mkdir -p out &&
g++ src/main.cc -L./libs -lncurses -limtui -limgui-for-imtui -limtui-ncurses -o ./out/loom++

