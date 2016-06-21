Version = $(shell cat version)
#Debug = -g

cygfuse-$(Version).dll libfuse-$(Version).dll.a fuse.pc: cygfuse.cpp fuse.pc.in
	g++ $(Debug) -shared -o cygfuse-$(Version).dll -Wl,--out-implib=libfuse-$(Version).dll.a -I../../inc/fuse cygfuse.cpp
	[ -n "$(Debug)" ] || strip cygfuse-$(Version).dll
	sed "s/@Version@/$(Version)/g" fuse.pc.in > fuse.pc

cygfuse-test.exe: cygfuse-test.c cygfuse-$(Version).dll libfuse-$(Version).dll.a
	gcc $(Debug) -o cygfuse-test.exe -I../../inc/fuse -DCYGFUSE cygfuse-test.c -L$(PWD) -lfuse-$(Version)
