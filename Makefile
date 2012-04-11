# http://odkq.com/simplebf.html
VERSION=0.0.1
CFLAGS?=-O -Wall -Werror 
NAME=simplebf
OBJECTFILES=simplebf.o
DISTFILES=Makefile simplebf.c README INSTALL
$NAME: $(OBJECTFILES)
	gcc $(CFLAGS) $(LDFLAGS) $(OBJECTFILES) -o $(NAME)
clean:
	-rm $(NAME)
	-rm $(NAME)-$(VERSION).tar.gz
	-rm $(OBJECTFILES)
dist: $(DISTFILES)
	mkdir $(NAME)-$(VERSION)
	cp -a $(DISTFILES) $(NAME)-$(VERSION)
	tar czvf $(NAME)-$(VERSION).tar.gz $(NAME)-$(VERSION)
	rm -fR $(NAME)-$(VERSION)

.PHONY: clean dist
