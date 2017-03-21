usage:
	@echo "make cygfuse|winfsp-fuse" 1>&2
	@echo "" 1>&2
	@echo "   cygfuse         Link with CYGFUSE" 1>&2
	@echo "   winfsp-fuse     Link with WinFsp-FUSE" 1>&2
	@exit 2

cygfuse: passthrough-cygfuse

winfsp-fuse: passthrough-winfsp-fuse

passthrough-cygfuse: passthrough-fuse.c
	gcc $^ -o $@ -g -Wall `pkg-config fuse --cflags --libs`

passthrough-winfsp-fuse: export PKG_CONFIG_PATH=$(PWD)/winfsp.install/lib
passthrough-winfsp-fuse: passthrough-fuse.c
	ln -nsf "`regtool --wow32 get '/HKLM/Software/WinFsp/InstallDir' | cygpath -au -f -`" winfsp.install
	gcc $^ -o $@ -g -Wall `pkg-config fuse --cflags --libs`
