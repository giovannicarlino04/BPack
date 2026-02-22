@echo off

if not exist bin\ mkdir bin\

pushd bin\

cl -nologo -O2 ..\main.c ..\external\lz4.c -I..\include -I..\external

popd