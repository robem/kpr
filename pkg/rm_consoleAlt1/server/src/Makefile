PKGDIR ?= ../..
L4DIR	 ?= $(PKGDIR)/../..

TARGET = $(PKGNAME)

# list your .c or .cc files here
SRC_CC = consoleAlt1.cc consoleCtrl.cc

# list requirements of your program here
REQUIRES_LIBS = libc_be_mem libgfxbitmap libstdc++ libpthread

# LIBS += -lcxx_thread_e

L4_MULTITHREADED = y

include $(L4DIR)/mk/prog.mk
