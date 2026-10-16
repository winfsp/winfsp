MINGW_CXX ?= x86_64-w64-mingw32-g++

usage:
	@echo "make cygfuse3|winfsp-fuse3|winfsp-fuse3-mingw" 1>&2
	@echo "" 1>&2
	@echo "   cygfuse3             Link with CYGFUSE3" 1>&2
	@echo "   winfsp-fuse3         Link with WinFsp-FUSE3" 1>&2
	@echo "   winfsp-fuse3-mingw   Link with WinFsp-FUSE3 using MinGW" 1>&2
	@exit 2

cygfuse3: memfs-cygfuse3

winfsp-fuse3: memfs-winfsp-fuse3

winfsp-fuse3-mingw: memfs-winfsp-fuse3-mingw

memfs-cygfuse3: memfs-fuse3.cpp
	g++ $^ -o $@ -g -Wall -std=gnu++17 `pkg-config fuse3 --cflags --libs`

memfs-winfsp-fuse3: export PKG_CONFIG_PATH=$(PWD)/winfsp.install/lib
memfs-winfsp-fuse3: memfs-fuse3.cpp
	ln -nsf "`regtool --wow32 get '/HKLM/Software/WinFsp/InstallDir' | cygpath -au -f -`" winfsp.install
	g++ $^ -o $@ -g -Wall -std=gnu++17 `pkg-config fuse3 --cflags --libs`

memfs-winfsp-fuse3-mingw: export PKG_CONFIG_PATH=$(PWD)/winfsp.install/lib
memfs-winfsp-fuse3-mingw: memfs-fuse3.cpp
	ln -nsf "$$(powershell.exe -NoProfile -Command "(Get-ItemProperty -LiteralPath 'HKLM:\Software\WOW6432Node\WinFsp').InstallDir" | tr -d '\r' | cygpath -au -f -)" winfsp.install
	$(MINGW_CXX) $^ -o $@ -g -Wall -std=gnu++17 `pkg-config fuse3 --cflags --libs`
