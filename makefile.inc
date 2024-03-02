# check pre-requisites
EXECUTABLES = install strip ln tar $(CC) $(AR) mkdir git echo cat
K := $(foreach exec,$(EXECUTABLES),\
        $(if $(shell which $(exec)),some string,$(error "No $(exec) in PATH")))

# tools used
INSTALL = $(shell which install)
STRIP = $(shell which strip)
LN = $(shell which ln)
TAR = $(shell which tar)
MKDIR = $(shell which mkdir)
GIT = $(shell which git)
ECHO = $(shell which echo)
CAT = $(shell which cat)

# the current version
RAW_VERSION ?= $(if $(shell git describe --tags 2> /dev/null),$(shell git describe --tags),$(VERSION_PREFIX)v0.0.0)
VERSION = $(if $(findstring .,$(RAW_VERSION)),$(subst $(VERSION_PREFIX)v,,$(strip $(RAW_VERSION))),$(strip $(RAW_VERSION)))
VERSION_PARTS = $(if $(findstring .,$(RAW_VERSION)),$(subst ., ,$(VERSION)),0 0 0)
VMAJOR = $(word 1,$(VERSION_PARTS))
VMINOR = $(word 2,$(VERSION_PARTS))
VBUILD_FULL = $(word 3,$(VERSION_PARTS))
VBUILD = $(word 1,$(subst -, ,$(VBUILD_FULL)))

# the processor architecture
MACHINE ?= $(shell $(CC) -dumpmachine)
CC_PARTS = $(subst -, ,$(CC))
CC_NAME = $(word $(words $(CC_PARTS)),$(CC_PARTS))

# Target
COMPONENT = tr181-xpon
ifeq ($(CONFIG_GATEWAY)$(CONFIG_ACCESSPOINT),)
CONFIG_GATEWAY=y
endif

# install directories
PREFIX ?= /usr/
LIBDIR ?= $(PREFIX)lib/$(MACHINE)
BINDIR ?= $(PREFIX)bin
INCLUDEDIR ?= $(PREFIX)include
INITDIR ?= /etc/init.d
ACLDIR ?= /etc/acl
DOCDIR ?= $(PREFIX)share/doc/tr181-xpon
PROCMONDIR ?= $(PREFIX)lib/processmonitor/scripts

# default config options
CONFIG_SAH_AMX_TR181_XPON_ORDER ?= 30
CONFIG_SAH_AMX_TR181_XPON_MAX_ONUS ?= 1
