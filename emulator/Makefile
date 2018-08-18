EMULATOR := 1

OBJS += setup.o

OBJS += flash.o
OBJS += oled.o
OBJS += rng.o
OBJS += timer.o

ifdef PIZERO
OBJS += usb.o
OBJS += oled_drivers.o
else
OBJS += buttons.o
OBJS += udp.o
endif

OBJS += strl.o

libemulator.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

include ../Makefile.include
