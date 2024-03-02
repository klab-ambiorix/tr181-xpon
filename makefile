include makefile.inc

NOW = $(shell date +"%Y-%m-%d(%H:%M:%S %z)")

# Extra destination directories
PKGDIR = ./output/$(MACHINE)/pkg/

define create_changelog
	@$(ECHO) "Update changelog"
	mv CHANGELOG.md CHANGELOG.md.bak
	head -n 9 CHANGELOG.md.bak > CHANGELOG.md
	$(ECHO) "" >> CHANGELOG.md
	$(ECHO) "## Release $(VERSION) - $(NOW)" >> CHANGELOG.md
	$(ECHO) "" >> CHANGELOG.md
	$(GIT) log --pretty=format:"- %s" $$($(GIT) describe --tags | grep -v "merge" | cut -d'-' -f1)..HEAD  >> CHANGELOG.md
	$(ECHO) "" >> CHANGELOG.md
	tail -n +10 CHANGELOG.md.bak >> CHANGELOG.md
	rm CHANGELOG.md.bak
endef

# targets
all:
	$(MAKE) -C odl all
	$(MAKE) -C src all

clean:
	$(MAKE) -C odl clean
	$(MAKE) -C src clean

install: all
	$(INSTALL) -D -p -m 0644 odl/$(COMPONENT).odl $(DEST)/etc/amx/$(COMPONENT)/$(COMPONENT).odl
	$(INSTALL) -D -p -m 0644 odl/$(COMPONENT)_definition.odl $(DEST)/etc/amx/$(COMPONENT)/$(COMPONENT)_definition.odl
	$(INSTALL) -d -m 0755 $(DEST)//etc/amx/tr181-xpon/defaults.d
	$(foreach odl,$(wildcard odl/defaults.d/*.odl), $(INSTALL) -D -p -m 0644 $(odl) $(DEST)/etc/amx/tr181-xpon/defaults.d/;)
	$(INSTALL) -D -p -m 0644 odl/$(COMPONENT)_ANI.odl $(DEST)/etc/amx/$(COMPONENT)/$(COMPONENT)_ANI.odl
	$(INSTALL) -D -p -m 0644 odl/$(COMPONENT)_EthernetUNI.odl $(DEST)/etc/amx/$(COMPONENT)/$(COMPONENT)_EthernetUNI.odl
	$(INSTALL) -D -p -m 0644 odl/$(COMPONENT)_SoftwareImage.odl $(DEST)/etc/amx/$(COMPONENT)/$(COMPONENT)_SoftwareImage.odl
	$(INSTALL) -D -p -m 0755 output/$(MACHINE)/$(COMPONENT).so $(DEST)/usr/lib/amx/$(COMPONENT)/$(COMPONENT).so
	$(INSTALL) -d -m 0755 $(DEST)$(BINDIR)
	ln -sfr $(DEST)$(BINDIR)/amxrt $(DEST)$(BINDIR)/$(COMPONENT)
	$(INSTALL) -D -p -m 0755 scripts/$(COMPONENT).sh $(DEST)$(INITDIR)/$(COMPONENT)

package: all
	$(INSTALL) -D -p -m 0644 odl/$(COMPONENT).odl $(PKGDIR)/etc/amx/$(COMPONENT)/$(COMPONENT).odl
	$(INSTALL) -D -p -m 0644 odl/$(COMPONENT)_definition.odl $(PKGDIR)/etc/amx/$(COMPONENT)/$(COMPONENT)_definition.odl
	$(INSTALL) -d -m 0755 $(PKGDIR)//etc/amx/tr181-xpon/defaults.d
	$(INSTALL) -D -p -m 0644 odl/defaults.d/*.odl $(PKGDIR)/etc/amx/tr181-xpon/defaults.d/
	$(INSTALL) -D -p -m 0644 odl/$(COMPONENT)_ANI.odl $(PKGDIR)/etc/amx/$(COMPONENT)/$(COMPONENT)_ANI.odl
	$(INSTALL) -D -p -m 0644 odl/$(COMPONENT)_EthernetUNI.odl $(PKGDIR)/etc/amx/$(COMPONENT)/$(COMPONENT)_EthernetUNI.odl
	$(INSTALL) -D -p -m 0644 odl/$(COMPONENT)_SoftwareImage.odl $(PKGDIR)/etc/amx/$(COMPONENT)/$(COMPONENT)_SoftwareImage.odl
	$(INSTALL) -D -p -m 0755 output/$(MACHINE)/$(COMPONENT).so $(PKGDIR)/usr/lib/amx/$(COMPONENT)/$(COMPONENT).so
	$(INSTALL) -d -m 0755 $(PKGDIR)$(BINDIR)
	ln -sfr $(PKGDIR)$(BINDIR)/amxrt $(PKGDIR)$(BINDIR)/$(COMPONENT)
	$(INSTALL) -D -p -m 0755 scripts/$(COMPONENT).sh $(PKGDIR)$(INITDIR)/$(COMPONENT)
	cd $(PKGDIR) && $(TAR) -czvf ../$(COMPONENT)-$(VERSION).tar.gz .
	cp $(PKGDIR)../$(COMPONENT)-$(VERSION).tar.gz .
	make -C packages

changelog:
	$(call create_changelog)

doc:
	$(eval ODLFILES += odl/$(COMPONENT).odl)
	$(eval ODLFILES += odl/$(COMPONENT)_definition.odl)
	# expand/substitute source wildcard instead of using destination directory: the destination directory can contain files from another artifact not intended for pcb_docgen use
	$(eval ODLFILES += $(wildcard odl/defaults.d/*.odl))
	$(eval ODLFILES += odl/$(COMPONENT)_ANI.odl)
	$(eval ODLFILES += odl/$(COMPONENT)_EthernetUNI.odl)
	$(eval ODLFILES += odl/$(COMPONENT)_SoftwareImage.odl)

	mkdir -p output/xml
	mkdir -p output/html
	mkdir -p output/confluence
	amxo-cg -Gxml,output/xml/$(COMPONENT).xml $(or $(ODLFILES), "")
	amxo-xml-to -x html -o output-dir=output/html -o title="$(COMPONENT)" -o version=$(VERSION) -o sub-title="Datamodel reference" output/xml/*.xml
	amxo-xml-to -x confluence -o output-dir=output/confluence -o title="$(COMPONENT)" -o version=$(VERSION) -o sub-title="Datamodel reference" output/xml/*.xml

.PHONY: all clean changelog install package doc