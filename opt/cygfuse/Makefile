Arch	= $(shell uname -m)
Build	= build

.PHONY: usage
usage:
	@echo "make cygport|msys2|dist|dist-msys2" 1>&2
	@exit 2

.PHONY: cygport msys2 dist dist-msys2 clean
cygport: clean cygport2 cygport3
msys2: clean msys2pkg2 msys2pkg3
dist: cygport dist2 dist3
dist-msys2: clean msys2dist2 msys2dist3
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

.PHONY: msys2pkg2
msys2pkg2: $(Build)/winfsp-work-$(Arch).tar.gz
	mkdir -p $(Build)/msys2-fuse
	cp msys2/fuse/PKGBUILD $(Build)/msys2-fuse/PKGBUILD
	cd $(Build)/msys2-fuse && \
		WINFSP_SRC_URI=../winfsp-work-$(Arch).tar.gz WINFSP_SRC_DIR=winfsp-work-$(Arch) \
		makepkg --noconfirm --syncdeps --cleanbuild

.PHONY: msys2pkg3
msys2pkg3: $(Build)/winfsp-work-$(Arch).tar.gz
	mkdir -p $(Build)/msys2-fuse3
	cp msys2/fuse3/PKGBUILD $(Build)/msys2-fuse3/PKGBUILD
	cd $(Build)/msys2-fuse3 && \
		WINFSP_SRC_URI=../winfsp-work-$(Arch).tar.gz WINFSP_SRC_DIR=winfsp-work-$(Arch) \
		makepkg --noconfirm --syncdeps --cleanbuild

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

.PHONY: msys2dist2
msys2dist2: msys2pkg2
	case $(Arch) in \
	x86_64) \
		mkdir -p dist/msys2/x64 && \
		rm -f dist/msys2/x64/winfsp-fuse-*.pkg.tar.* && \
		cp build/msys2-fuse/winfsp-fuse-*.pkg.tar.* dist/msys2/x64 ;; \
	i686) \
		mkdir -p dist/msys2/x86 && \
		rm -f dist/msys2/x86/winfsp-fuse-*.pkg.tar.* && \
		cp build/msys2-fuse/winfsp-fuse-*.pkg.tar.* dist/msys2/x86 ;; \
	esac

.PHONY: msys2dist3
msys2dist3: msys2pkg3
	case $(Arch) in \
	x86_64) \
		mkdir -p dist/msys2/x64 && \
		rm -f dist/msys2/x64/winfsp-fuse3-*.pkg.tar.* && \
		cp build/msys2-fuse3/winfsp-fuse3-*.pkg.tar.* dist/msys2/x64 ;; \
	i686) \
		mkdir -p dist/msys2/x86 && \
		rm -f dist/msys2/x86/winfsp-fuse3-*.pkg.tar.* && \
		cp build/msys2-fuse3/winfsp-fuse3-*.pkg.tar.* dist/msys2/x86 ;; \
	esac
