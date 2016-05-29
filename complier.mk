CROSS :=
CC    :=$(CROSS)gcc
CPP   :=$(CROSS)g++
AR    :=$(CROSS)ar
STRIP :=$(CROSS)strip

ARFLAGS = -rv
STRIPFLAGS = -xXg
CFLAGS = -Wall
CPPFLAGS = -Wall
