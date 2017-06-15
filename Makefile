ENABLE_VNC	:= y
ENABLE_SDL	:= y
ENABLE_GTK	:= y
ENABLE_ZLIB	:= y
ENABLE_AIO	:= y
ENABLE_STATIC	:= y
PIE_FLAGS	= -no-pie

# ARCH = x86_64, powerpc, arm64, mips
ARCH	?= $(shell uname -m | sed -e s/ppc.*/powerpc/ -e s/aarch64.*/arm64/ -e s/mips64/mips/)
CFLAGS	:= -fPIC
LDFLAGS	:=

KVMC_SRC = $(wildcard src/cmds/*.c) $(wildcard hw/virtio/*.c) $(wildcard src/net/*.c) $(wildcard util/*.c) $(wildcard hw/*.c) $(wildcard hw/disk/*.c)
OBJS	+= src/devices.o util/guest/compat.o src/irq.o src/cpu.o src/kvm.o src/term.o src/ioeventfd.o src/cmds.o src/ipc.o ${KVMC_SRC:.c=.o}

ifeq ($(ARCH),x86_64)
	DEFINES += -DCONFIG_X86_64 -DCONFIG_X86
	OBJS	+= arch/x86/boot.o arch/x86/cpuid.o arch/x86/interrupt.o arch/x86/ioport.o arch/x86/irq.o arch/x86/kvm.o arch/x86/kvm-cpu.o arch/x86/mptable.o
	# Exclude BIOS object files from header dependencies.
	OTHEROBJS	+= arch/x86/bios.o arch/x86/bios/bios-rom.o
	ARCH_INCLUDE := arch/x86/include
	ARCH_HAS_FRAMEBUFFER := y
	ARCH_PRE_INIT = arch/x86/init.S
endif
# POWER/ppc:  Actually only support ppc64 currently.
ifeq ($(ARCH), powerpc)
	DEFINES += -DCONFIG_PPC
	SRCS_PPC = $(wildcard arch/powerpc/*.c)
	OBJS	+= ${SRCS_PPC:.c=.o}
	ARCH_INCLUDE := arch/powerpc/include
	ARCH_WANT_LIBFDT := y
endif

ifeq ($(ARCH), arm64)
	DEFINES		+= -DCONFIG_ARM64
	SRCS_ARM	:= $(wildcard arch/arm/*.c)
	OBJS		+= $(SRCS_ARM:.c=.o) arch/arm/aarch64/arm-cpu.o arch/arm/aarch64/kvm-cpu.o
	ARCH_INCLUDE	:= arch/arm/include -Iarch/arm/aarch64/include
	ARCH_WANT_LIBFDT := y
endif

ifeq ($(ARCH),mips)
	DEFINES		+= -DCONFIG_MIPS
	ARCH_INCLUDE	:= arch/mips/include
	OBJS		+= arch/mips/kvm.o arch/mips/kvm-cpu.o arch/mips/irq.o
endif

ifeq (,$(ARCH_INCLUDE))
        $(error This architecture ($(ARCH)) is not supported in kvmc)
endif

# Detect optional features.
ifeq (y,$(ARCH_HAS_FRAMEBUFFER))
	CFLAGS_GTK3 := $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
	LDFLAGS_GTK3 := $(shell pkg-config --libs gtk+-3.0 2>/dev/null)
	ifeq (${ENABLE_GTK}, y)
		OBJS_DYNOPT	+= hw/ui/gtk3.o
		CFLAGS_DYNOPT	+= -DCONFIG_HAS_GTK3 $(CFLAGS_GTK3)
		LIBS_DYNOPT	+= $(LDFLAGS_GTK3)
	else
		NOTFOUND	+= GTK3
	endif

	ifeq (${ENABLE_VNC}, y)
		OBJS_DYNOPT	+= hw/ui/vnc.o
		CFLAGS_DYNOPT	+= -DCONFIG_HAS_VNCSERVER
		LIBS_DYNOPT	+= -lvncserver
	else
		NOTFOUND	+= vncserver
	endif

	ifeq (${ENABLE_SDL}, y)
		OBJS_DYNOPT	+= hw/ui/sdl.o
		CFLAGS_DYNOPT	+= -DCONFIG_HAS_SDL
		LIBS_DYNOPT	+= -lSDL
	else
		NOTFOUND	+= SDL
	endif
endif

ifeq (${ENABLE_ZLIB}, y)
	CFLAGS_DYNOPT	+= -DCONFIG_HAS_ZLIB
	LIBS_DYNOPT	+= -lz
else
	NOTFOUND	+= zlib
endif

ifeq (${ENABLE_AIO}, y)
	CFLAGS_DYNOPT	+= -DCONFIG_HAS_AIO
	LIBS_DYNOPT	+= -laio
else
	NOTFOUND	+= aio
endif

CFLAGS		+= -DCONFIG_GUEST_INIT
GUEST_OBJS	= util/guest/guest_init.o
ifeq ($(ARCH_PRE_INIT),)
	GUEST_INIT_FLAGS	+= -static# $(PIE_FLAGS)
else
	CFLAGS			+= -DCONFIG_GUEST_PRE_INIT
	GUEST_INIT_FLAGS	+= -DCONFIG_GUEST_PRE_INIT
	GUEST_OBJS		+= util/guest/guest_pre_init.o
endif

ifeq (y,$(ARCH_WANT_LIBFDT))
	CFLAGS_DYNOPT	+= -DCONFIG_HAS_LIBFDT
	LIBS_DYNOPT	+= -lfdt
endif

ifneq ($(NOTFOUND),)
        $(warning Skipping optional libraries: $(NOTFOUND))
endif

DEFINES	+= -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -DBUILD_ARCH='"$(ARCH)"'

CFLAGS	+= $(DEFINES) -Iinclude -I$(ARCH_INCLUDE) -fno-strict-aliasing -O2
CFLAGS	+= -Werror -Wall -Wformat=2 -Winit-self -Wnested-externs -Wno-system-headers -Wundef -Wunused -Wold-style-definition \
	-Wredundant-decls -Wsign-compare -Wstrict-prototypes -Wvolatile-register-var -Wwrite-strings -Wno-format-nonliteral

all: util/guest/init util/guest/pre_init libkvmc.so kvmc

kvmc: binding/sh.o libkvmc.so
	$(CC) -o $@ $< -L. -lkvmc -lreadline

libkvmc.so: $(OBJS) $(OBJS_DYNOPT) $(OTHEROBJS) ${GUEST_OBJS}
	$(CC) -shared $(CFLAGS) $(OBJS) $(OBJS_DYNOPT) $(OTHEROBJS) $(GUEST_OBJS) $(LDFLAGS) $(LIBS_DYNOPT) -o $@ -lrt -pthread -lutil

ifneq ($(ARCH_PRE_INIT),)
util/guest/pre_init: $(ARCH_PRE_INIT)
	$(CC) -s $(PIE_FLAGS) -nostdlib $(ARCH_PRE_INIT) -o $@
	$(LD) -r -b binary -o util/guest/guest_pre_init.o $@
endif

util/guest/init: util/guest/init.c
	$(CC) $(GUEST_INIT_FLAGS) $^ -o $@

util/guest/guest_init.o: util/guest/init
	$(LD) -r -b binary -o $@ $^

%.s: %.c
	$(CC) -o $@ -S $(CFLAGS) -fverbose-asm $<

%.o: %.c
	$(CC) -c $(CFLAGS) $(CFLAGS_DYNOPT) $< -o $@

# BIOS assembly weirdness
BIOS_CFLAGS += -m32 -march=i386 -mregparm=3 -fno-stack-protector -fno-pic

arch/x86/bios.o: arch/x86/bios/bios.bin arch/x86/bios/bios-rom.h

arch/x86/bios/bios.bin.elf: arch/x86/bios/entry.S arch/x86/bios/e820.c arch/x86/bios/int10.c arch/x86/bios/int15.c arch/x86/bios/rom.ld.S
	$(CC) -include include/code16gcc.h $(CFLAGS) $(BIOS_CFLAGS) -c arch/x86/bios/memcpy.c -o arch/x86/bios/memcpy.o
	$(CC) -include include/code16gcc.h $(CFLAGS) $(BIOS_CFLAGS) -c arch/x86/bios/e820.c -o arch/x86/bios/e820.o
	$(CC) -include include/code16gcc.h $(CFLAGS) $(BIOS_CFLAGS) -c arch/x86/bios/int10.c -o arch/x86/bios/int10.o
	$(CC) -include include/code16gcc.h $(CFLAGS) $(BIOS_CFLAGS) -c arch/x86/bios/int15.c -o arch/x86/bios/int15.o
	$(CC) $(CFLAGS) $(BIOS_CFLAGS) -c arch/x86/bios/entry.S -o arch/x86/bios/entry.o
	$(LD) -T arch/x86/bios/rom.ld.S -o $@ arch/x86/bios/memcpy.o arch/x86/bios/entry.o arch/x86/bios/e820.o arch/x86/bios/int10.o arch/x86/bios/int15.o

arch/x86/bios/bios.bin: arch/x86/bios/bios.bin.elf
	objcopy -O binary -j .text arch/x86/bios/bios.bin.elf arch/x86/bios/bios.bin

arch/x86/bios/bios-rom.o: arch/x86/bios/bios-rom.S arch/x86/bios/bios.bin arch/x86/bios/bios-rom.h
	$(CC) -c $(CFLAGS) arch/x86/bios/bios-rom.S -o arch/x86/bios/bios-rom.o

arch/x86/bios/bios-rom.h: arch/x86/bios/bios.bin.elf
	cd arch/x86/bios && sh gen-offsets.sh > bios-rom.h && cd -

.PHONY: clean check strip
strip: all
	@strip kvmc libkvmc.so

check: all
	#$(MAKE) -C tests
	#./kvmc start tests/pit/tick.bin
	./kvmc start --disk ../linux.img --kernel ../bzImage --network virtio --sdl

clean:
	rm -fr arch/x86/bios/*.bin arch/x86/bios/*.elf arch/x86/bios/*.o arch/x86/bios/bios-rom.h
	rm -f $(OBJS) $(OTHEROBJS) $(OBJS_DYNOPT) binding/sh.o kvmc libkvmc.so util/guest/init util/guest/pre_init $(GUEST_OBJS)
