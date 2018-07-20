usage:
	@echo "make cygfuse3|winfsp-fuse3" 1>&2
	@echo "" 1>&2
	@echo "   cygfuse3        Link with CYGFUSE3" 1>&2
	@echo "   winfsp-fuse3    Link with WinFsp-FUSE3" 1>&2
	@exit 2

cygfuse3: passthrough-cygfuse3

winfsp-fuse3: passthrough-winfsp-fuse3

passthrough-cygfuse3: passthrough-fuse3.c
	gcc $^ -o $@ -g -Wall `pkg-config fuse3 --cflags --libs`

passthrough-winfsp-fuse3: export PKG_CONFIG_PATH=$(PWD)/winfsp.install/lib
passthrough-winfsp-fuse3: passthrough-fuse3.c
	ln -nsf "`regtool --wow32 get '/HKLM/Software/WinFsp/InstallDir' | cygpath -au -f -`" winfsp.install
	gcc $^ -o $@ -g -Wall `pkg-config fuse3 --cflags --libs`
