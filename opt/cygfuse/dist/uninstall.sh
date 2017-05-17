cd "$(dirname "$0")"
case $(uname -m) in
x86_64)
    tar -taf x64/fuse-2.8-*.tar.xz | sed -e '/\/$/d' -e 's/.*/\/&/' | xargs rm -f ;;
*)
    tar -taf x86/fuse-2.8-*.tar.xz | sed -e '/\/$/d' -e 's/.*/\/&/' | xargs rm -f ;;
esac
