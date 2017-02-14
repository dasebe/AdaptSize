TARGET = adaptsizetuner
OBJS += ../varnish-4.1.2/lib/libvarnishtools/vut.o
OBJS += ../varnish-4.1.2/lib/libvarnish/vas.o	    
OBJS +=	../varnish-4.1.2/lib/libvarnish/flopen.o 
OBJS += ../varnish-4.1.2/lib/libvarnish/version.o
OBJS +=	../varnish-4.1.2/lib/libvarnish/vsb.o    
OBJS += ../varnish-4.1.2/lib/libvarnish/vpf.o    
OBJS += ../varnish-4.1.2/lib/libvarnish/vtim.o
OBJS += adaptsize.o
LIBS += -lrt -lm -pthread

VLIB += ../varnish-4.1.2/lib/libvarnishcompat/libvarnishcompat.la
VLIB += ../varnish-4.1.2/lib/libvarnishapi/libvarnishapi.la
CXX = gcc
CXXFLAGS += -std=gnu99
CXXFLAGS += -DHAVE_CONFIG_H
CXXFLAGS += -MD -MP
CXXFLAGS += -I. -I../varnish-4.1.2 -I../varnish-4.1.2/include
CXXFLAGS += -Wall -Werror -Wno-error=unused-result -MT
LDFLAGS += $(LIBS)
LD = /bin/sh ../varnish-4.1.2/libtool --silent --tag=CC --mode=link gcc # use varnish libtool
all: CXXFLAGS += -O2
all:		$(TARGET)

$(TARGET):	$(OBJS)
	$(LD) $(CXXFLAGS) -o $@ $^ $(VLIB) $(LDFLAGS)

%.o: %.c
	$(CXX) $(CXXFLAGS) -c -o $@ $<

DEPS = $(OBJS:%.o=%.d)
-include $(DEPS)

clean:
	-rm $(TARGET) $(OBJS) $(DEPS)
