#Debug = -g

cygfuse.dll libfuse.a: cygfuse.cpp
	g++ $(Debug) -shared -o cygfuse.dll -Wl,--out-implib=libfuse.a -I../../inc/fuse cygfuse.cpp
	[ -n "$(Debug)" ] || strip cygfuse.dll

cygfuse-test.exe: cygfuse-test.c cygfuse.dll libfuse.a
	gcc $(Debug) -o cygfuse-test.exe -I../../inc/fuse -DCYGFUSE cygfuse-test.c -L$(PWD) -lfuse
