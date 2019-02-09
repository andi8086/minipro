# Install Configuration

# Normally minipro is installed to /usr/local.  If you want to put it
# somewhere else, define that location here.
PREFIX=/usr/local

# Some older releases of MacOS need some extra library flags.
#EXTRA_LIBS += "-framework Foundation -framework IOKit"


#########################################################################
# This section is where minipro is actually built.
# Under normal circumstances, nothing below this point should be changed.
##########################################################################

# If we're working from git, we have access to proper variables. If
# not, make it clear that we're working from a release.
GIT_DIR ?= .git
ifneq ($(and $(wildcard $(GIT_DIR)),$(shell which git)),)
        GIT_BRANCH = $(shell git rev-parse --abbrev-ref HEAD)
        GIT_HASH = $(shell git rev-parse HEAD)
        GIT_HASH_SHORT = $(shell git rev-parse --short HEAD)
        GIT_TAG = $(shell git describe --abbrev=0 --tags)
else
        GIT_BRANCH = none
        GIT_HASH = none
        GIT_HASH_SHORT = none
        GIT_TAG = none
endif
BUILD_DATE_TIME = $(shell date +%Y%m%d.%k%M%S | sed s/\ //g)
VERSION_HEADER = version.h
VERSION_STRINGS = version.c

PKG_CONFIG := $(shell which pkg-config 2>/dev/null)
ifeq ($(PKG_CONFIG),)
        ERROR := $(error "pkg-config utility not found")
endif

COMMON_OBJECTS=byte_utils.o database.o minipro.o fuses.o easyconfig.o version.o
OBJECTS=$(COMMON_OBJECTS) main.o
PROGS=minipro
MINIPRO=minipro
MINIPROHEX=miniprohex
TESTS=$(wildcard tests/test_*.c);
OBJCOPY=objcopy

DIST_DIR = $(MINIPRO)-$(GIT_TAG)
BIN_INSTDIR=$(DESTDIR)$(PREFIX)/bin
MAN_INSTDIR=$(DESTDIR)$(PREFIX)/share/man/man1

UDEV_DIR=$(shell pkg-config --define-variable=prefix=$(PREFIX) --silence-errors --variable=udevdir udev)
UDEV_RULES_INSTDIR=$(DESTDIR)$(UDEV_DIR)/rules.d

COMPLETIONS_DIR=$(shell pkg-config --define-variable=prefix=$(PREFIX) --silence-errors --variable=completionsdir bash-completion)
COMPLETIONS_INSTDIR=$(DESTDIR)$(COMPLETIONS_DIR)

libusb_CFLAGS := $(shell $(PKG_CONFIG) --cflags libusb-1.0)
libusb_LIBS := $(shell $(PKG_CONFIG) --libs libusb-1.0)

CFLAGS = -g -O0 -Wall
override CFLAGS += $(libusb_CFLAGS)
override LIBS += $(libusb_LIBS) $(EXTRA_LIBS)

all: $(PROGS)

version_header: $(VERSION_HEADER)
$(VERSION_HEADER):
	@echo "Creating $@"
	@echo "/*" > $@
	@echo " * This file is automatically generated.  Do not edit." >> $@
	@echo " */" >> $@
	@echo "extern const char build_timestamp[];" >> $@
	@echo "#define GIT_BRANCH \"$(GIT_BRANCH)\"" >> $@
	@echo "#define GIT_HASH \"$(GIT_HASH)\"" >> $@
	@echo "#define GIT_HASH_SHORT \"$(GIT_HASH_SHORT)\"" >> $@
	@echo "#define GIT_TAG \"$(GIT_TAG)\"" >> $@

version_strings: $(VERSION_STRINGS)
$(VERSION_STRINGS):
	@echo "Creating $@"
	@echo "/*" > $@
	@echo " * This file is automatically generated.  Do not edit." >> $@
	@echo " */" >> $@
	@echo "#include \"minipro.h\"" >> $@
	@echo "#include \"version.h\"" >> $@
	@echo "const char build_timestamp[] = \"$(BUILD_DATE_TIME)\";" >> $@

minipro: $(VERSION_HEADER) $(VERSION_STRINGS) $(COMMON_OBJECTS) main.o
	$(CC) $(COMMON_OBJECTS) main.o $(LIBS) -o $(MINIPRO)

clean:
	rm -f $(OBJECTS) $(PROGS)
	rm -f version.h version.c version.o

distclean: clean
	rm -rf minipro-$(GIT_TAG)*

install:
	mkdir -p $(BIN_INSTDIR)
	mkdir -p $(MAN_INSTDIR)
	cp $(MINIPRO) $(BIN_INSTDIR)/
	cp $(MINIPROHEX) $(BIN_INSTDIR)/
	cp man/minipro.1 $(MAN_INSTDIR)/
	if [ -n "$(UDEV_DIR)" ]; then \
		mkdir -p $(UDEV_RULES_INSTDIR); \
		cp udev/rules.d/80-minipro.rules $(UDEV_RULES_INSTDIR)/; \
	fi
	if [ -n "$(COMPLETIONS_DIR)" ]; then \
		mkdir -p $(COMPLETIONS_INSTDIR); \
		cp bash_completion.d/minipro $(COMPLETIONS_INSTDIR)/; \
	fi

uninstall:
	rm -f $(BIN_INSTDIR)/$(MINIPRO)
	rm -f $(BIN_INSTDIR)/$(MINIPROHEX)
	rm -f $(MAN_INSTDIR)/minipro.1
	if [ -n "$(UDEV_DIR)" ]; then rm -f $(UDEV_RULES_INSTDIR)/80-minipro.rules; fi
	if [ -n "$(COMPLETIONS_DIR)" ]; then rm -f $(COMPLETIONS_INSTDIR)/minipro; fi

dist: distclean version-info
	git archive --format=tar --prefix=minipro-$(GIT_TAG)/ HEAD | tar xf -
	sed -i "s/GIT_BRANCH = none/GIT_BRANCH = $(GIT_BRANCH)/" minipro-$(GIT_TAG)/Makefile
	sed -i "s/GIT_HASH = none/GIT_HASH = $(GIT_HASH)/" minipro-$(GIT_TAG)/Makefile
	sed -i "s/GIT_HASH_SHORT = none/GIT_HASH_SHORT = $(GIT_HASH_SHORT)/" minipro-$(GIT_TAG)/Makefile
	sed -i "s/GIT_TAG = none/GIT_TAG = $(GIT_TAG)/" minipro-$(GIT_TAG)/Makefile
	tar zcf minipro-$(GIT_TAG).tar.gz minipro-$(GIT_TAG)
	rm -rf minipro-$(GIT_TAG)
	@echo Created minipro-$(GIT_TAG).tar.gz


.PHONY: all dist distclean clean install test version-info
