Version	= $(shell sed -n '/^VERSION=/s/VERSION=\(.*\)/\1/p' fuse3.cygport)
Arch	= $(shell uname -m)
Build	= build/$(Arch)
#Debug	= -g

.PHONY: build test
build: $(Build)/cygfuse-$(Version).dll $(Build)/fuse3.pc
test: $(Build)/cygfuse-test.exe

$(Build)/cygfuse-$(Version).dll: cygfuse.c fuse3.cygport
	@mkdir -p $(Build)
	gcc $(Debug) \
		-shared -o $(Build)/cygfuse-$(Version).dll \
		-Wl,--out-implib=$(Build)/libfuse-$(Version).dll.a \
		-I../../../inc/fuse3 \
		cygfuse.c
	[ -n "$(Debug)" ] || strip $(Build)/cygfuse-$(Version).dll

$(Build)/fuse3.pc: fuse3.pc.in fuse3.cygport
	@mkdir -p $(Build)
	sed "s/@Version@/$(Version)/g" fuse3.pc.in > $(Build)/fuse3.pc

$(Build)/cygfuse-test.exe: cygfuse-test.c $(Build)/cygfuse-$(Version).dll
	@mkdir -p $(Build)
	gcc $(Debug) \
		-o $(Build)/cygfuse-test.exe \
		-I../../../inc/fuse3 -DCYGFUSE \
		cygfuse-test.c \
		-L$(PWD)/$(Build) -lfuse-$(Version)
