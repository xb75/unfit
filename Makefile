
all:
	make -C src all
	make -C doc all

clean:
	make -C src clean
	make -C doc clean

install: doc src
	install -m 755 src/unfit /usr/local/bin/unfit
	mkdir -p /usr/local/share/man/man1/
	install -m 644 doc/unfit.1.gz /usr/local/share/man/man1/unfit.1.gz
	mandb
