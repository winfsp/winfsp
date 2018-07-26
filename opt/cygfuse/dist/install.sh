cd "$(dirname "$0")"
case $(uname -m) in
x86_64)
    tar -C/ -xaf x64/fuse-2.*.tar.xz
    tar -C/ -xaf x64/fuse-3.*.tar.xz
    ;;
i686)
    tar -C/ -xaf x86/fuse-2.*.tar.xz
    tar -C/ -xaf x86/fuse-3.*.tar.xz
    ;;
*)
    echo unsupported architecture 1>&2
    exit 1
esac
echo FUSE for Cygwin installed.
