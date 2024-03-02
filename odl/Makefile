include ../makefile.inc

TARGETS=tr181-xpon.odl tr181-xpon_ANI.odl tr181-xpon_EthernetUNI.odl

ifdef CONFIG_SAH_AMX_TR181_XPON_USE_NETDEV_COUNTERS
M4_OPTS := -DCONFIG_SAH_AMX_TR181_XPON_USE_NETDEV_COUNTERS=y
endif

all: $(TARGETS)

%.odl: %.odl.m4
	m4 $(M4_OPTS) $< > $@

clean:
	rm -f $(TARGETS)

.PHONY: all clean
