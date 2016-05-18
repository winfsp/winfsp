web: web/winfsp-design.html web/service-architecture.html web/winfsp.h.html

web/winfsp-design.html:
	mkdir -p web
	asciidoc -b html4 -a hr= -s -o $@ winfsp-design.adoc

web/service-architecture.html:
	mkdir -p web
	asciidoc -b html4 -a hr= -s -o $@ service-architecture.adoc

web/winfsp.h.html:
	mkdir -p web
	prettydoc -H-O -o web ../inc/winfsp/winfsp.h
