#!/bin/bash

cd $(dirname "$0")/../..

PRETTYDOC="$PWD/../prettydoc/prettydoc"

if [[ $# -eq 0 ]]; then
    echo "usage: $(basename $0) {asciidoc|html|markdown}" 1>&2
    exit 1
fi

"$PRETTYDOC" -f $1 -t --no-timestamp -H=--outer-names-only -o doc inc/winfsp/winfsp.h inc/winfsp/launch.h

if [[ "$1" == "asciidoc" ]]; then
    mv doc/winfsp.h.asciidoc doc/WinFsp-API-winfsp.h.asciidoc
    mv doc/launch.h.asciidoc doc/WinFsp-API-launch.h.asciidoc
fi

if [[ "$1" == "markdown" ]]; then
    mv doc/winfsp.h.markdown doc/WinFsp-API-winfsp.h.md
    mv doc/launch.h.markdown doc/WinFsp-API-launch.h.md
fi
