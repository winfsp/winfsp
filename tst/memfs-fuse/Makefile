MINGW_CXX ?= x86_64-w64-mingw32-g++

usage:
	@echo "make cygfuse|winfsp-fuse|winfsp-fuse-mingw" 1>&2
	@echo "" 1>&2
	@echo "   cygfuse             Link with CYGFUSE" 1>&2
	@echo "   winfsp-fuse         Link with WinFsp-FUSE" 1>&2
	@echo "   winfsp-fuse-mingw   Link with WinFsp-FUSE using MinGW" 1>&2
	@exit 2

cygfuse: memfs-cygfuse

winfsp-fuse: memfs-winfsp-fuse

winfsp-fuse-mingw: memfs-winfsp-fuse-mingw

memfs-cygfuse: memfs-fuse.cpp
	g++ $^ -o $@ -g -Wall -std=gnu++17 `pkg-config fuse --cflags --libs`

memfs-winfsp-fuse: export PKG_CONFIG_PATH=$(PWD)/winfsp.install/lib
memfs-winfsp-fuse: memfs-fuse.cpp
	ln -nsf "`regtool --wow32 get '/HKLM/Software/WinFsp/InstallDir' | cygpath -au -f -`" winfsp.install
	g++ $^ -o $@ -g -Wall -std=gnu++17 `pkg-config fuse --cflags --libs`

memfs-winfsp-fuse-mingw: export PKG_CONFIG_PATH=$(PWD)/winfsp.install/lib
memfs-winfsp-fuse-mingw: memfs-fuse.cpp
	ln -nsf "$$(powershell.exe -NoProfile -Command "(Get-ItemProperty -LiteralPath 'HKLM:\Software\WOW6432Node\WinFsp').InstallDir" | tr -d '\r' | cygpath -au -f -)" winfsp.install
	$(MINGW_CXX) $^ -o $@ -g -Wall -std=gnu++17 -DFSP_FUSE_USE_STAT_EX `pkg-config fuse --cflags --libs`
