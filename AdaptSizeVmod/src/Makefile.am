AM_CPPFLAGS = @VMOD_INCLUDES@ -Wall -Werror

vmoddir = @VMOD_DIR@
vmod_LTLIBRARIES = libvmod_autoparam.la

libvmod_autoparam_la_LDFLAGS = -module -export-dynamic -avoid-version -shared

libvmod_autoparam_la_SOURCES = \
	vmod_autoparam.c

nodist_libvmod_autoparam_la_SOURCES = \
	vcc_if.c \
	vcc_if.h

vmod_autoparam.lo: vcc_if.c vcc_if.h

vcc_if.c: vcc_if.h

vcc_if.h: @VMODTOOL@ $(top_srcdir)/src/vmod_autoparam.vcc
	@VMODTOOL@ $(top_srcdir)/src/vmod_autoparam.vcc

VMOD_TESTS = $(top_srcdir)/src/tests/*.vtc
.PHONY: $(VMOD_TESTS)

$(top_srcdir)/src/tests/*.vtc: libvmod_autoparam.la
	@VARNISHTEST@ -Dvarnishd=@VARNISHD@ -Dvmod_topbuild=$(abs_top_builddir) $@

check: $(VMOD_TESTS)

EXTRA_DIST = \
	vmod_autoparam.vcc \
	$(VMOD_TESTS)

CLEANFILES = \
	$(builddir)/vcc_if.c \
	$(builddir)/vcc_if.h \
	$(builddir)/vmod_autoparam.rst \
	$(builddir)/vmod_autoparam.man.rst
