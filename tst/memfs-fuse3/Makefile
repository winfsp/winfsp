usage:
	@echo "make cygfuse3|winfsp-fuse3" 1>&2
	@echo "" 1>&2
	@echo "   cygfuse3        Link with CYGFUSE3" 1>&2
	@echo "   winfsp-fuse3    Link with WinFsp-FUSE3" 1>&2
	@exit 2

cygfuse3: memfs-cygfuse3

winfsp-fuse3: memfs-winfsp-fuse3

memfs-cygfuse3: memfs-fuse3.cpp
	g++ $^ -o $@ -g -Wall -std=gnu++17 `pkg-config fuse3 --cflags --libs`

memfs-winfsp-fuse3: export PKG_CONFIG_PATH=$(PWD)/winfsp.install/lib
memfs-winfsp-fuse3: memfs-fuse3.cpp
	ln -nsf "`regtool --wow32 get '/HKLM/Software/WinFsp/InstallDir' | cygpath -au -f -`" winfsp.install
	g++ $^ -o $@ -g -Wall -std=gnu++17 `pkg-config fuse3 --cflags --libs`
