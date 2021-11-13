usage:
	@echo "make cygfuse|winfsp-fuse" 1>&2
	@echo "" 1>&2
	@echo "   cygfuse         Link with CYGFUSE" 1>&2
	@echo "   winfsp-fuse     Link with WinFsp-FUSE" 1>&2
	@exit 2

cygfuse: memfs-cygfuse

winfsp-fuse: memfs-winfsp-fuse

memfs-cygfuse: memfs-fuse.cpp
	g++ $^ -o $@ -g -Wall -std=gnu++17 `pkg-config fuse --cflags --libs`

memfs-winfsp-fuse: export PKG_CONFIG_PATH=$(PWD)/winfsp.install/lib
memfs-winfsp-fuse: memfs-fuse.cpp
	ln -nsf "`regtool --wow32 get '/HKLM/Software/WinFsp/InstallDir' | cygpath -au -f -`" winfsp.install
	g++ $^ -o $@ -g -Wall -std=gnu++17 `pkg-config fuse --cflags --libs`
