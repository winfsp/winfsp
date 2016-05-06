@echo off

call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat"

mc -b -c eventlog.mc
