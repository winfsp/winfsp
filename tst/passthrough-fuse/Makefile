MINGW_CC ?= x86_64-w64-mingw32-gcc

usage:
	@echo "make cygfuse|winfsp-fuse|winfsp-fuse-mingw" 1>&2
	@echo "" 1>&2
	@echo "   cygfuse             Link with CYGFUSE" 1>&2
	@echo "   winfsp-fuse         Link with WinFsp-FUSE" 1>&2
	@echo "   winfsp-fuse-mingw   Link with WinFsp-FUSE using MinGW" 1>&2
	@exit 2

cygfuse: passthrough-cygfuse

winfsp-fuse: passthrough-winfsp-fuse

winfsp-fuse-mingw: passthrough-winfsp-fuse-mingw

passthrough-cygfuse: passthrough-fuse.c
	gcc $^ -o $@ -g -Wall `pkg-config fuse --cflags --libs`

passthrough-winfsp-fuse: export PKG_CONFIG_PATH=$(PWD)/winfsp.install/lib
passthrough-winfsp-fuse: passthrough-fuse.c
	ln -nsf "`regtool --wow32 get '/HKLM/Software/WinFsp/InstallDir' | cygpath -au -f -`" winfsp.install
	gcc $^ -o $@ -g -Wall `pkg-config fuse --cflags --libs`

passthrough-winfsp-fuse-mingw: export PKG_CONFIG_PATH=$(PWD)/winfsp.install/lib
passthrough-winfsp-fuse-mingw: passthrough-fuse.c winposix.c
	ln -nsf "$$(powershell.exe -NoProfile -Command "(Get-ItemProperty -LiteralPath 'HKLM:\Software\WOW6432Node\WinFsp').InstallDir" | tr -d '\r' | cygpath -au -f -)" winfsp.install
	$(MINGW_CC) $^ -o $@ -g -Wall -DFSP_FUSE_USE_STAT_EX `pkg-config fuse --cflags --libs` -lntdll
