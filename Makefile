VERSION=2.0ud.1

include config.mk

DIRS=library utils

.PHONY : all clean install

all :
	for d in ${DIRS}; do $(MAKE) -C $${d}; done

clean :
	for d in ${DIRS}; do $(MAKE) -C $${d} clean; done

install : all
	for d in ${DIRS}; do $(MAKE) -C $${d} install; done

dist : distclean
	mkdir vxi11-$(VERSION)
	cp -pr library utils vxi11-$(VERSION)/
	mkdir vxi11-$(VERSION)/python
	cp python/setup.py python/vxi11.py vxi11-$(VERSION)/python
	cp -p config.mk Makefile CMakeLists.txt CHANGELOG.txt README.md GNU_General_Public_License.txt vxi11.spec vxi11-$(VERSION)/
	tar -zcf vxi11-$(VERSION).tar.gz vxi11-$(VERSION)

distclean :  clean
	rm -rf vxi11-$(VERSION)
	rm -f vxi11-$(VERSION).tar.gz
