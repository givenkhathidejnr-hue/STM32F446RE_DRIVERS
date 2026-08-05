#==============================================================================
# Project
#==============================================================================
PROJECT     := firmware

#==============================================================================
# Toolchain
#==============================================================================
CC       	:= arm-none-eabi-gcc
CXX 		:= arm-none-eabi-g++
# NOTE: "-x assembler-with-cpp" is a GCC front-end flag, not something the
# raw GNU assembler (arm-none-eabi-as) understands - passing it straight to
# `as` fails immediately ("unrecognized option '-x'"), so .s files never
# built. Driving the assembler through gcc is the standard way to get .s
# files preprocessed (so #include/#define/#ifdef work in assembly, as this
# project's startup file's structure implies it was meant to support) before
# being assembled - and it also means $(CFLAGS)'s -I/-D flags (passed to $(AS)
# by the build rule below) are honored, not silently ignored.
AS 			:= arm-none-eabi-gcc -x assembler-with-cpp

OBJCOPY  	:= arm-none-eabi-objcopy
SIZE		:= arm-none-eabi-size
FLASH    	:= st-flash
ADDRESS		:= 0x08000000

#==============================================================================
# Directories
#==============================================================================
SRCDIRS     := app platform exceptions system drivers/sources drivers/sources/timers
OBJDIR      := build
BINDIR      := bin
DEPDIR      := dep

#==============================================================================
# CMSIS / Includes
#==============================================================================
CMSIS_CORE  := CMSIS/Core/Include
CMSIS_DEV   := CMSIS/Device/ST/STM32F4xx/Include
HEADERS 	:= drivers/headers drivers/headers/timers

INCDIRS     := $(CMSIS_CORE) $(CMSIS_DEV) $(HEADERS) system platform
				
INCLUDES    := $(addprefix -I,$(INCDIRS))

#==============================================================================
# Output files
#==============================================================================
ELF         := $(BINDIR)/$(PROJECT).elf
BIN         := $(BINDIR)/$(PROJECT).bin
MAP         := $(BINDIR)/$(PROJECT).map

#==============================================================================
# Flags
#==============================================================================
#==============================================================================
# MCU Flags
#==============================================================================
CPU         := -mcpu=cortex-m4 -mthumb
FLOAT_ABI	:= -mfloat-abi=hard
FPU         := -mfpu=fpv4-sp-d16
MCU			:= $(CPU) $(FPU) $(FLOAT_ABI)

OPTIM       := -Os
WARN        := -Wall -Wextra -Wshadow

CFLAGS      := $(MCU) $(OPTIM) $(WARN) \
               -ffreestanding -g \
               $(INCLUDES)

DEPFLAGS    := -MMD -MP -MF $(DEPDIR)/$*.d

LDFLAGS     := $(MCU) \
               -lc --specs=nano.specs\
               -lgcc -lnosys\
               -T platform/stm32f446_linker.ld \
               -Wl,-Map=$(MAP),--gc-sections

#==============================================================================
# Source discovery (recursive)
#==============================================================================
rwildcard 	= $(foreach d,$(wildcard $1*), \
			$(call rwildcard,$d/,$2) \
			$(filter $(subst *,%,$2),$d))

SRCS 		:= \
			$(foreach d,$(SRCDIRS),$(call rwildcard,$d/,*.c)) \
			$(foreach d,$(SRCDIRS),$(call rwildcard,$d/,*.s)) \
			$(foreach d,$(SRCDIRS),$(call rwildcard,$d/,*.S)) \
			$(foreach d,$(SRCDIRS),$(call rwildcard,$d/,*.cpp))


#==============================================================================
# Objects / dependencies (mirrored tree)
#==============================================================================
OBJS := $(SRCS:%.c=$(OBJDIR)/%.o)
OBJS := $(OBJS:%.cpp=$(OBJDIR)/%.o)
OBJS := $(OBJS:%.s=$(OBJDIR)/%.o)
OBJS := $(OBJS:%.S=$(OBJDIR)/%.o)

DEPS := $(OBJS:$(OBJDIR)/%.o=$(DEPDIR)/%.d)

#==============================================================================
# Top-level targets
#==============================================================================
.PHONY: all clean dirs flash size openocd gdb print-link print-objs

.SILENT:

all: dirs $(BIN) size

#==============================================================================
# Binary image
#==============================================================================
$(BIN): $(ELF)
	$(OBJCOPY) $< -O binary $@

#==============================================================================
# Linking
#==============================================================================
$(ELF): $(OBJS)
	$(CXX) $(LDFLAGS) $^ -o $@

#==============================================================================
# Compile rules
#==============================================================================
$(OBJDIR)/%.o: %.c
	mkdir -p $(dir $@) $(dir $(DEPDIR)/$*.d)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.cpp
	mkdir -p $(dir $@) $(dir $(DEPDIR)/$*.d)
	$(CXX) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.s
	mkdir -p $(dir $@)
	$(AS) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: %.S
	mkdir -p $(dir $@)
	$(AS) $(CFLAGS) -c $< -o $@
	
#==============================================================================
# Include auto-generated dependencies
#==============================================================================
-include $(DEPS)

#==============================================================================
# flashing binary
#==============================================================================
flash:	$(BIN)
	@echo "Flashing firmware.bin to the MCU..."
	$(FLASH) --reset --connect-under-reset write $< $(ADDRESS)

#==============================================================================
# Directory creation
#==============================================================================
dirs:
	mkdir -p $(OBJDIR) $(BINDIR) $(DEPDIR)

#==============================================================================
# Size report
#==============================================================================
# NOTE: must run on the ELF, not the flat .bin - a .bin from objcopy -O
# binary is raw bytes with no section headers/format info at all, so `size`
# (which reads ELF section headers to report .text/.data/.bss) can't parse
# it ("file format not recognized"). $(BIN) still depends on $(ELF) via the
# rule above, so this doesn't skip producing the .bin.
size: $(ELF)
	$(SIZE) $<

openocd:	
	openocd \
	-f /usr/share/openocd/scripts/interface/stlink.cfg \
	-c "transport select hla_swd"\
	-f /usr/share/openocd/scripts/target/stm32f4x.cfg \
	-c "adapter speed 1000"\
	-c "init; reset init; stm32f4x mass_erase 0")

gdb:
	gdb-multiarch $(ELF)

print-link:
	@echo "Linker command:"
	@echo $(CC) $(LDFLAGS) $(OBJS) -o $(ELF)

print-objs:
	@echo "Object files:"
	@echo $(OBJS)

#==============================================================================
# Clean
#==============================================================================
clean:
	rm -r -f $(OBJDIR) $(BINDIR) $(DEPDIR)
