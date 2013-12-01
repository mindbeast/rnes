# tools 
CPP      = g++
LD       = g++

ifdef RELEASE 
CPPFLAGS   = -O3 -fno-omit-frame-pointer -ggdb -march=native
DEFINES    := NDEBUG
else
CPPFLAGS   = -g
endif
CPPFLAGS  += -std=c++11 -Wall -Wno-unused-function

LIBS += SDL2

LDFLAGS = $(addprefix -l, $(LIBS))

BINDIR   = ./bin
VPATH    = $(BINDIR)
TARGET   = rnes
TARGET  := $(BINDIR)/$(TARGET)

# source files 
CPP_FILES += main.cpp
CPP_FILES += ppu.cpp
CPP_FILES += apu.cpp
CPP_FILES += cpu.cpp
CPP_FILES += nes.cpp
CPP_FILES += sdl.cpp
CPP_FILES += mmc.cpp

# targets 
OBJS    += $(addsuffix .o, $(basename $(CPP_FILES)))
OBJS    := $(addprefix $(BINDIR)/, $(OBJS))

DEPS    := $(addsuffix .d, $(basename $(CPP_FILES)))
DEPS    := $(addprefix $(BINDIR)/, $(DEPS))

HEADERS := $(wildcard *.h)

PCHS    := $(addsuffix .gch, $(HEADERS))
PCHS    := $(addprefix $(BINDIR)/, $(PCHS))
DEFINES := $(addprefix -D, $(DEFINES))

.PHONY: all 

# rules 
all : $(TARGET)

# make the bin directory 
$(BINDIR) : 
	mkdir $(BINDIR)

# link the kernel 
$(TARGET) : $(OBJS) 
	$(LD) $(CPPFLAGS) $(OBJS) $(LDFLAGS) -o $@ 

# make sure the bin directory exists before building 
$(OBJS) : | $(BINDIR) 
$(PCHS) : | $(BINDIR) 
$(DEPS) : | $(BINDIR) 

# build all pch's before compiling cpp files 
#$(OBJS) : $(PCHS)

# build rules for all source 
$(BINDIR)/%.o : %.cpp
	$(CPP) $(CPPFLAGS) $(DEFINES) -c $< -o $@

$(BINDIR)/%.d : %.cpp
	$(CPP) -MM $(CPPFLAGS)  -MT $(BINDIR)/$*.o $< > $@

$(BINDIR)/%.s : %.cpp
	$(CPP) $(CPPFLAGS) $< -S -fverbose-asm -o $@
	
$(BINDIR)/%.h.gch : %.h
	$(CPP) $(CPPFLAGS) -x c++-header $< -o $@

# pull in dependency info for *existing* .o files
-include $(DEPS)

# clean up the build 
clean :
	-rm $(DEPS)
	-rm $(OBJS)	
	-rm $(TARGET)
	-rmdir $(BINDIR)

