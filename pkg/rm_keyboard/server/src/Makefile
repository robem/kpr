PKGDIR		?= ../..
L4DIR		?= $(PKGDIR)/../..

TARGET		= $(PKGNAME)

# list your .c or .cc files here
SRC_CC    = keyboard.cc

# list requirements of your program here
REQUIRES_LIBS   = libc_be_mem libio-io libstdc++ libpthread 

L4_MULTITHREADED=y

include $(L4DIR)/mk/prog.mk
