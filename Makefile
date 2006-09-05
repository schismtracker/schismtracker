# this is a hack so that ./configure && make will actually work.
_default_rule:
	$(MAKE) -C build
%::
	$(MAKE) -C build $@
