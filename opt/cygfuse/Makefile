Version = $(shell sed -n '/^VERSION=/s/VERSION=\(.*\)/\1/p' fuse.cygport)
#Debug = -g

cygfuse-$(Version).dll libfuse-$(Version).dll.a fuse.pc: cygfuse.c fuse.pc.in
	gcc $(Debug) -shared -o cygfuse-$(Version).dll -Wl,--out-implib=libfuse-$(Version).dll.a -I../../inc/fuse cygfuse.c
	[ -n "$(Debug)" ] || strip cygfuse-$(Version).dll
	sed "s/@Version@/$(Version)/g" fuse.pc.in > fuse.pc

cygfuse-test.exe: cygfuse-test.c cygfuse-$(Version).dll libfuse-$(Version).dll.a
	gcc $(Debug) -o cygfuse-test.exe -I../../inc/fuse -DCYGFUSE cygfuse-test.c -L$(PWD) -lfuse-$(Version)

cygport:
	git clean -dfx
	(\
		cd `git rev-parse --show-toplevel` &&\
		Stash=`git stash create` &&\
		git archive --prefix=winfsp-work/ --format=tar.gz $${Stash:-HEAD}\
			> opt/cygfuse/winfsp-work.tar.gz\
	)
	CYGPORT_SRC_URI=winfsp-work.tar.gz CYGPORT_SRC_DIR=winfsp-work cygport fuse.cygport download prep compile install package

dist: cygport
	case $(shell uname -m) in \
	x86_64)\
		cp fuse-*/dist/fuse/fuse-*[0-9].tar.xz dist/x64 ;;\
	*)\
		cp fuse-*/dist/fuse/fuse-*[0-9].tar.xz dist/x86 ;;\
	esac
