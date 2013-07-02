PKGDIR		?= .
L4DIR		?= $(PKGDIR)/../..

vpath %.bmp ..

MODE		= static
#MODE		= shared
TARGET	= rm_pong-client
SRC_CC	= pong_client.cc
LIBS   += -lcxx_thread_e

REQUIRES_LIBS += libc_be_mem libstdc++

L4_MULTITHREADED =y

include $(L4DIR)/mk/prog.mk
