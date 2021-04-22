.PHONY: clean distclean

default: all

all :
	cd src && $(MAKE) $@

clean :
	cd src && $(MAKE) $@

distclean :
	cd src && $(MAKE) $@
