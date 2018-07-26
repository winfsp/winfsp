Version	= $(shell sed -n '/^VERSION=/s/VERSION=\(.*\)/\1/p' fuse.cygport)
Arch	= $(shell uname -m)
Build	= build/$(Arch)
#Debug	= -g

.PHONY: build test
build: $(Build)/cygfuse-$(Version).dll $(Build)/fuse.pc
test: $(Build)/cygfuse-test.exe

$(Build)/cygfuse-$(Version).dll: cygfuse.c fuse.cygport
	@mkdir -p $(Build)
	gcc $(Debug) \
		-shared -o $(Build)/cygfuse-$(Version).dll \
		-Wl,--out-implib=$(Build)/libfuse-$(Version).dll.a \
		-I../../../inc/fuse \
		cygfuse.c
	[ -n "$(Debug)" ] || strip $(Build)/cygfuse-$(Version).dll

$(Build)/fuse.pc: fuse.pc.in fuse.cygport
	@mkdir -p $(Build)
	sed "s/@Version@/$(Version)/g" fuse.pc.in > $(Build)/fuse.pc

$(Build)/cygfuse-test.exe: cygfuse-test.c $(Build)/cygfuse-$(Version).dll
	@mkdir -p $(Build)
	gcc $(Debug) \
		-o $(Build)/cygfuse-test.exe \
		-I../../../inc/fuse -DCYGFUSE \
		cygfuse-test.c \
		-L$(PWD)/$(Build) -lfuse-$(Version)
