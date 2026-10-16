cd "$(dirname "$0")"
if [ -n "${MSYSTEM:-}" ] && command -v pacman >/dev/null 2>&1; then
    pacman -R --noconfirm winfsp-fuse3 winfsp-fuse
    echo FUSE for MSYS2 uninstalled.
    exit
fi

case $(uname -m) in
x86_64)
    tar -taf x64/fuse-*.tar.xz | sed -e '/\/$/d' -e 's/.*/\/&/' | xargs rm -f
    tar -taf x64/fuse3-*.tar.xz | sed -e '/\/$/d' -e 's/.*/\/&/' | xargs rm -f
    ;;
i686)
    tar -taf x86/fuse-*.tar.xz | sed -e '/\/$/d' -e 's/.*/\/&/' | xargs rm -f
    tar -taf x86/fuse3-*.tar.xz | sed -e '/\/$/d' -e 's/.*/\/&/' | xargs rm -f
    ;;
*)
    echo unsupported architecture 1>&2
    exit 1
    ;;
esac
echo FUSE for Cygwin uninstalled.
