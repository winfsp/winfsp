tar -taf fuse-2.8-*.tar.xz | sed -e '/\/$/d' -e 's/.*/\/&/' | xargs rm -f
