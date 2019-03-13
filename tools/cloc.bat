@echo off

setlocal

cd %~dp0..

cloc --force-lang=c,i --fullpath "--not-match-d=ext/test|build/VStudio/.vs|build/VStudio/build" .
