# Linkpulse GBA homebrew
# Requires devkitPro/devkitARM with libgba installed.

TARGET := linkpulse
BUILD := build
SOURCES := source
INCLUDES := include

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
OFILES := $(CFILES:.c=.o)

CFLAGS := -g -Wall -Wextra -O2 -std=c11 -ffast-math
LDFLAGS :=
LIBS := -lmm -lgba

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

.PHONY: all clean
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $@ -f $(CURDIR)/Makefile

clean:
	@rm -rf $(BUILD) $(TARGET).gba $(TARGET).elf $(TARGET).map
else

CFILES := $(notdir $(CFILES))
OFILES := $(CFILES:.c=.o)

endif

include $(DEVKITARM)/gba_rules
