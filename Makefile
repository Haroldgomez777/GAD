OBJS = gadot.o convert.o markdown.o html_sniff.o html_tag.o html_attr.o html_scan.o gad_util.o gad_input.o gad_pdf.o gad_pdf_native.o

gadotmake: gad
	@true

gad: $(OBJS)
	gcc -o gad $(OBJS) -Wall -Wextra -std=c99

%.o: %.c
	gcc -c $< -o $@ -Wall -Wextra -std=c99 -I.

.PHONY: install install-user clean

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

install: gad
	sudo install -m 755 gad $(BINDIR)/gad

# User-local install (no sudo); ensure ~/bin is on your PATH
install-user: gad
	mkdir -p $(HOME)/bin
	install -m 755 gad $(HOME)/bin/gad
	@echo 'Installed to $(HOME)/bin/gad — add to PATH if needed: export PATH="$(HOME)/bin:$$PATH"'

clean:
	rm -f gad $(OBJS)
