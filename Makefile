
CC=clang-8
LD=ld.lld-8

.PHONY: all \
		clean \
		image \
		kernel \
		bootloader \
		qemu

all: bin/tomatos.img

#########################
# Creating a VDI
#########################

vdi: bin/tomatos.vdi

bin/tomatos.vdi: bin/tomatos.img
	cd bin && vboxmanage convertfromraw --format VDI tomatos.img tomatos.vdi

#########################
# Creating an HDD
#########################

hdd: bin/tomatos.hdd

bin/tomatos.hdd: bin/tomatos.img
	cd bin && cp tomatos.img tomatos.hdd

#########################
# Compiling the kernel
#########################

# Real mode blobs, needed for some stuff
REALFILES += $(shell find src -type f -name '*.real')
BINS := $(REALFILES:%.real=build/%.bin)

# Sources, all c files basically
SRCS += $(shell find src -type f -name '*.c')
SRCS += $(shell find src -type f -name '*.asm')
SRCS += $(shell find lib -type f -name '*.c')
OBJS := $(SRCS:%=build/%.o)

# All the dirs
OBJDIRS := $(dir $(OBJS) $(BINS))

# include dirs
INCLUDE_DIRS += lib/
INCLUDE_DIRS += lib/libc
INCLUDE_DIRS += src/

# Set the flags
CFLAGS += \
	-target x86_64-unknown-elf \
	-mno-sse \
	-ffreestanding \
	-nostdlib \
	-nostdinc \
	-fno-stack-check \
	-fno-stack-protector \
	-mno-red-zone \
	-mcmodel=kernel \
	-fno-omit-frame-pointer \
	-mno-omit-leaf-frame-pointer \
	-Wall \
	-fno-pie \
	-static \
	-g \
	-DSTB_SPRINTF_NOFLOAT \

#	-flto \
#	-Ofast \


# Set the include dirs
CFLAGS += $(INCLUDE_DIRS:%=-I%)

LOGGERS ?= E9 VBOX

# Set the loggers
CFLAGS += $(LOGGERS:%=-D%_LOGGER)

# Set the linking flags
LDFLAGS += \
	--oformat elf_amd64 \
	--Bstatic \
	--nostdlib \
	-T config/linker.ld

NASMFLAGS += \
	-g \
	-F dwarf \
	-f elf64

kernel: bin/image/tomatos.elf

bin/image/tomatos.elf: $(OBJDIRS) $(BINS) $(OBJS)
	mkdir -p bin/image
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

build/%.bin: %.real
	nasm -f bin -o $@ $<

build/%.c.o: %.c
	$(CC) $(CFLAGS) -D __FILENAME__="\"$<\"" -c -o $@ $<

build/%.asm.o: %.asm
	nasm $(NASMFLAGS) -o $@ $<

build/%:
	mkdir -p $@

#########################
# Bootloader compilation
#########################

bin/image/EFI/BOOT/BOOTX64.EFI:
	$(MAKE) -C ./boot/
	mkdir -p bin/image/EFI/BOOT/
	cp boot/bin/BOOTX64.EFI bin/image/EFI/BOOT/

#########################
# Creating a bootable
# image
#########################

# Shortcut
image: bin/tomatos.img

# Create the image
bin/tomatos.img: \
		bin/image/EFI/BOOT/BOOTX64.EFI \
		bin/image/tomatos.elf \
		tools/image-builder.py
	cp ./config/tomatboot.cfg ./bin/image/
	cp ./boot/bin/BOOTX64.EFI ./bin/image/
	cd bin && ../tools/image-builder.py ../config/image.yaml

#########################
# Tools we need
#########################

tools/image-builder.py:
	mkdir -p tools
	cd tools && wget https://raw.githubusercontent.com/TomatOrg/image-builder/master/image-builder.py
	chmod +x tools/image-builder.py

#########################
# Running in QEMU
#########################

QEMU_PATH ?= qemu-system-x86_64
#QEMU_PATH = ~/Documents/coding/qemu/bin/debug/native/x86_64-softmmu/qemu-system-x86_64

# Run qemu
qemu: bin/tomatos.img
	$(QEMU_PATH) \
		-drive if=pflash,format=raw,readonly,file=tools/OVMF.fd \
		-drive file=bin/tomatos.img,media=disk,format=raw \
		-no-reboot -no-shutdown \
		-machine q35 $(QEMU_ARGS)

#		-netdev tap,id=mynet0,ifname=tap0,script=no,downscript=no \
#		-device rtl8139,netdev=mynet0 \


qemu-debug: tools/OVMF.fd $(TOMATBOOT_UEFI_DIR_BIN)/BOOTX64.EFI $(TOMATBOOT_SHUTDOWN_DIR_BIN)/shutdown.elf bin/image/tomatos.elf bin/image/kbootcfg.bin
	$(QEMU_PATH) -drive if=pflash,format=raw,readonly,file=tools/OVMF.fd -net none -drive file=fat:rw:bin/image,media=disk,format=raw -no-reboot -no-shutdown -machine q35 -s -S $(QEMU_ARGS)

# Get the bios
tools/OVMF.fd:
	mkdir -p tools
	cd tools && wget http://downloads.sourceforge.net/project/edk2/OVMF/OVMF-X64-r15214.zip
	cd tools && unzip OVMF-X64-r15214.zip OVMF.fd
	rm tools/OVMF-X64-r15214.zip

#########################
# Clean
#########################

clean:
	rm -rf build bin

clean-all:
	$(MAKE) -C ./boot/ clean
	rm -rf build bin

# Delete all the tools
clean-tools:
	rm -rf tools
