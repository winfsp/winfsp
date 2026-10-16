cd "$(dirname "$0")"
if [ -n "${MSYSTEM:-}" ] && command -v pacman >/dev/null 2>&1; then
    case $(uname -m) in
    x86_64) msys2dir=msys2/x64 ;;
    i686) msys2dir=msys2/x86 ;;
    *) msys2dir= ;;
    esac
    if [ -n "$msys2dir" ] &&
        ls "$msys2dir"/winfsp-fuse-*.pkg.tar.* "$msys2dir"/winfsp-fuse3-*.pkg.tar.* >/dev/null 2>&1
    then
        pacman -U --noconfirm "$msys2dir"/winfsp-fuse-*.pkg.tar.* \
            "$msys2dir"/winfsp-fuse3-*.pkg.tar.*
        echo FUSE for MSYS2 installed.
        exit
    fi
fi

case $(uname -m) in
x86_64)
    tar -C/ -xaf x64/fuse-*.tar.xz
    tar -C/ -xaf x64/fuse3-*.tar.xz
    ;;
i686)
    tar -C/ -xaf x86/fuse-*.tar.xz
    tar -C/ -xaf x86/fuse3-*.tar.xz
    ;;
*)
    echo unsupported architecture 1>&2
    exit 1
    ;;
esac
echo FUSE for Cygwin installed.
