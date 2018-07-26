Arch	= $(shell uname -m)
Build	= build

.PHONY: usage
usage:
	@echo "make cygport|dist" 1>&2
	@exit 2

.PHONY: cygport dist clean
cygport: clean cygport2 cygport3
dist: cygport dist2 dist3
clean:
	rm -rf $(Build)

.PHONY: cygport2
cygport2: $(Build)/winfsp-work-$(Arch).tar.gz
	cp fuse/fuse.cygport $(Build)/fuse.cygport
	CYGPORT_SRC_URI=winfsp-work-$(Arch).tar.gz CYGPORT_SRC_DIR=winfsp-work-$(Arch) \
		cygport $(Build)/fuse.cygport download prep compile install package

.PHONY: cygport3
cygport3: $(Build)/winfsp-work-$(Arch).tar.gz
	cp fuse3/fuse3.cygport $(Build)/fuse3.cygport
	CYGPORT_SRC_URI=winfsp-work-$(Arch).tar.gz CYGPORT_SRC_DIR=winfsp-work-$(Arch) \
		cygport $(Build)/fuse3.cygport download prep compile install package

$(Build)/winfsp-work-$(Arch).tar.gz:
	mkdir -p $(Build)
	( \
		cd `git rev-parse --show-toplevel` && \
		Stash=`git stash create` && \
		git archive --prefix=winfsp-work-$(Arch)/ --format=tar.gz $${Stash:-HEAD} \
	) > $(Build)/winfsp-work-$(Arch).tar.gz

.PHONY: dist2
dist2: cygport2
	case $(Arch) in \
	x86_64) \
		mkdir -p dist/x64 && \
		rm -f dist/x64/fuse-*[0-9].tar.xz && \
		cp build/fuse-*[0-9].$(Arch)/dist/fuse/fuse-*[0-9].tar.xz dist/x64 ;; \
	i686) \
		mkdir -p dist/x86 && \
		rm -f dist/x86/fuse-*[0-9].tar.xz && \
		cp build/fuse-*[0-9].$(Arch)/dist/fuse/fuse-*[0-9].tar.xz dist/x86 ;; \
	esac

.PHONY: dist3
dist3: cygport3
	case $(Arch) in \
	x86_64) \
		mkdir -p dist/x64 && \
		rm -f dist/x64/fuse3-*[0-9].tar.xz && \
		cp build/fuse3-*[0-9].$(Arch)/dist/fuse3/fuse3-*[0-9].tar.xz dist/x64 ;; \
	i686) \
		mkdir -p dist/x86 && \
		rm -f dist/x86/fuse3-*[0-9].tar.xz && \
		cp build/fuse3-*[0-9].$(Arch)/dist/fuse3/fuse3-*[0-9].tar.xz dist/x86 ;; \
	esac
