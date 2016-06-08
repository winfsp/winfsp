#!/bin/bash

cd $(dirname "$0")

(
echo '#include <errno.h>'
echo '/*beginbeginbeginbegin*/'
awk '{ printf "case %s: return %s;\n", $1, $2 }' errno.txt
) > errno.src

echo "#if FSP_FUSE_ERRNO == 87 /* Windows */"
echo
vcvars="$(cygpath -aw "$VS140COMNTOOLS/../../VC/vcvarsall.bat")"
cmd /c "call" "$vcvars" "x64" "&&" cl /nologo /EP /C errno.src 2>/dev/null | sed -e '1,/beginbeginbeginbegin/d'
echo
echo "#elif FSP_FUSE_ERRNO == 67 /* Cygwin */"
echo
cpp -C -P errno.src | sed -e '1,/beginbeginbeginbegin/d'
echo
echo "#endif"

rm errno.src
