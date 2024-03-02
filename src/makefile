include ../makefile.inc

# build destination directories
OUTPUTDIR = ../output/$(MACHINE)
OBJDIR = $(OUTPUTDIR)

# TARGETS
TARGET_SO = $(OBJDIR)/$(COMPONENT).so

# directories
# source directories
SRCDIR = .
INCDIR_PRIV = ../include_priv
INCDIRS = $(INCDIR_PRIV)  $(if $(STAGINGDIR), $(STAGINGDIR)/include) $(if $(STAGINGDIR), $(STAGINGDIR)/usr/include)
STAGING_LIBDIR = $(if $(STAGINGDIR), -L$(STAGINGDIR)/lib) $(if $(STAGINGDIR), -L$(STAGINGDIR)/usr/lib)

SOURCES := $(wildcard $(SRCDIR)/*.c)
OBJECTS := $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))

# compilation and linking flags
CFLAGS += -Werror -Wall -Wextra \
          -Wformat=2 -Wshadow \
          -Wwrite-strings -Wredundant-decls \
          -Wno-attributes \
          -Wno-format-nonliteral \
          -fPIC -g3 $(addprefix -I ,$(INCDIRS)) \
          -DSAHTRACES_ENABLED -DSAHTRACES_LEVEL=500

# Many plugins use following flag. I put it in comment because I do not always
# provide a declaration. E.g. there's no declaration for _onu_enable_changed().
# IMO that's not a problem as it's not needed.
#CFLAGS += -Wmissing-declarations

ifeq ($(CC_NAME),g++)
CFLAGS += -std=c++2a
else
CFLAGS += -Wstrict-prototypes -Wold-style-definition -Wnested-externs -std=c11
endif

ifneq ($(CONFIG_SAH_AMX_TR181_XPON_MAX_ONUS),)
CFLAGS += -DMAX_NR_OF_ONUS=$(CONFIG_SAH_AMX_TR181_XPON_MAX_ONUS)
else
$(error CONFIG_SAH_AMX_TR181_XPON_MAX_ONUS is not defined)
endif

# Uncomment next CFLAGS line to have extra logging to stdout
#CFLAGS += -D_DEBUG_

LDFLAGS += -shared -fPIC $(STAGING_LIBDIR) \
           -lamxb -lamxc -lamxp -lamxd -lamxo -lamxm -lsahtrace

# Link to dl because of dlerror() call
LDFLAGS += -ldl

# targets
all: $(TARGET_SO)

$(TARGET_SO): $(OBJECTS)
	$(CC) -Wl,-soname,$(COMPONENT).so -o $@ $(OBJECTS) $(LDFLAGS)

-include $(OBJECTS:.o=.d)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<
	@$(CC) $(CFLAGS) -MM -MP -MT '$(@) $(@:.o=.d)' -MF $(@:.o=.d) $(<)

$(OBJDIR)/:
	$(MKDIR) -p $@

clean:
	rm -rf ../output/ ../$(COMPONENT)-*.* ../$(COMPONENT)_*.*

.PHONY: all clean
