CXX=g++
RM=rm -f
CXXFLAGS=-I./ -I./imgui/ $(shell pkg-config --cflags glfw3 glu gl) -Wall -Wformat -D_GLIBCXX_USE_CXX11_ABI=0 -std=c++11 -w -g
CFLAGS = $(CXXFLAGS)
LDFLAGS=-g $(shell pkg-config --libs glfw3 glu gl)

SRCS=imgui_dock.cpp imgui_widgets.cpp
OBJS=$(subst .cpp,.o,$(SRCS))

all: .depend $(OBJS)
	@true

.depend: $(SRCS)
	$(RM) ./.depend
	$(CXX) $(CXXFLAGS) -MM $^>>./.depend;

clean:
	$(RM) $(OBJS)

distclean: clean
	$(RM) *~ .depend

include .depend
