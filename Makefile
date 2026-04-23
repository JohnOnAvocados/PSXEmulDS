#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/ds_rules

#---------------------------------------------------------------------------------
# Target and source layout - Made by JohnOnAvocados
#---------------------------------------------------------------------------------
TARGET		:=	PSXEmulDS
BUILD		:=	build
SOURCES		:=	arm9
DATA		:=	data
INCLUDES		:=	include

#---------------------------------------------------------------------------------
# NDS icon (supports BMP and PNG)
#---------------------------------------------------------------------------------
ifneq ($(wildcard icon.bmp),)
ICONFLAGS	:=	--icon icon.bmp
else
ifneq ($(wildcard icon.png),)
 ICONFLAGS	:=	--icon icon.png
endif
endif

#---------------------------------------------------------------------------------
# Build flags
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv5te -mtune=arm946e-s -mthumb

CFLAGS	:=	-g -Wall -O2 -ffunction-sections -fdata-sections \
		$(ARCH)

CFLAGS	+=	$(INCLUDE) -DARM9
CXXFLAGS	:=	$(CFLAGS) -fno-rtti -fno-exceptions

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

#---------------------------------------------------------------------------------
# Libraries
#---------------------------------------------------------------------------------
LIBS	:=	-lfat -lnds9
LIBDIRS	:=	$(LIBNDS)

#---------------------------------------------------------------------------------
# Standard recursive build rules used by devkitPro NDS examples
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT	:=	$(CURDIR)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
			$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C "$(BUILD)" -f "$(CURDIR)/Makefile"

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nds $(TARGET).ds.gba

else

DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).nds	:	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)

%.bin.o	:	%.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

endif
