gadotmake: gadot.c
	gcc -o gad gadot.c -I.

.PHONY: install

install:
	mv gad /usr/local/bin/gad
