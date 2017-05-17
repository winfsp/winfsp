cd "$(dirname "$0")"
case $(uname -m) in
x86_64)
    tar -C/ -xaf x64/fuse-2.8-*.tar.xz ;;
*)
    tar -C/ -xaf x86/fuse-2.8-*.tar.xz ;;
esac
