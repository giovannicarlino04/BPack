@echo off

if not exist bin\ mkdir bin\

pushd bin\

cl -nologo -Zi -FC ..\main.c ..\external\lz4.c -I..\include -I..\external

popd