# tools 
CPP      = g++
LD       = g++
PBC      = protoc

ifdef RELEASE 
    CPPFLAGS   = -O3 -flto
    DEFINES    := NDEBUG
ifdef PROFILE 
    CPPFLAGS   += -fno-omit-frame-pointer -ggdb
endif 
else
    CPPFLAGS   = -g
endif

CPPFLAGS  += -std=c++17 -Wall -Wno-unused-function
#CPPFLAGS  += -H

LIBS += SDL2
LIBS += boost_filesystem
LIBS += boost_system
LIBS += boost_iostreams
LIBS += protobuf
LIBS += crypt

LIBDIRS += 

INCDIR += ./bin/
CPPFLAGS += $(addprefix -I, $(INCDIR))

LDFLAGS += $(addprefix -L, $(LIBDIRS))
LDFLAGS += $(addprefix -l, $(LIBS))

BINDIR   = ./bin
VPATH    = $(BINDIR)
TARGET   = rnes
TARGET  := $(BINDIR)/$(TARGET)

# source files 
CPP_FILES += main.cpp
CPP_FILES += ppu.cpp
CPP_FILES += apu.cpp
CPP_FILES += apuunit.cpp
CPP_FILES += cpu.cpp
CPP_FILES += nes.cpp
CPP_FILES += sdl.cpp
CPP_FILES += mmc.cpp
CPP_FILES += memory.cpp

PROTO_FILES += save.proto

# targets 
OBJS    += $(addsuffix .o, $(basename $(CPP_FILES)))
OBJS    += $(addsuffix .pb.o, $(basename $(PROTO_FILES)))
OBJS    := $(addprefix $(BINDIR)/, $(OBJS))

DEPS    += $(addsuffix .d, $(basename $(CPP_FILES)))
DEPS    += $(addsuffix .pb.d, $(basename $(PROTO_FILES)))
DEPS    := $(addprefix $(BINDIR)/, $(DEPS))

HEADERS := $(wildcard *.h)

PCHS    := $(addsuffix .gch, $(HEADERS))
PCHS    := $(addprefix $(BINDIR)/, $(PCHS))

GEN_CPP := $(addsuffix .pb.cc, $(basename $(PROTO_FILES)))
GEN_CPP := $(addprefix $(BINDIR)/, $(GEN_CPP))

GEN_HDR := $(addsuffix .pb.h, $(basename $(PROTO_FILES)))
GEN_HDR := $(addprefix $(BINDIR)/, $(GEN_HDR))

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

# make sure chained files aren't removed
$(TARGET) : $(GEN_CPP) $(GEN_HDR)

# make sure the bin directory exists before building 
$(GEN_CPP) : | $(BINDIR)
$(GEN_HDR) : | $(BINDIR)
$(OBJS) : | $(BINDIR) 
$(PCHS) : | $(BINDIR) 
$(DEPS) : | $(BINDIR) 

# build all pch's before compiling cpp files 
# $(OBJS) : $(PCHS)

# build rules for all source 
$(BINDIR)/%.pb.cc $(BINDIR)/%.pb.h : %.proto
	$(PBC) $< --cpp_out=$(BINDIR)

$(BINDIR)/%.pb.o : $(BINDIR)/%.pb.cc $(BINDIR)/%.pb.h
	$(CPP) $(CPPFLAGS) $(DEFINES) -c $< -o $@

$(BINDIR)/%.d : $(BINDIR)/%.cc
	$(CPP) -MM $(CPPFLAGS) -MT $(BINDIR)/$*.o $< > $@

$(BINDIR)/%.o : %.cpp
	$(CPP) $(CPPFLAGS) $(DEFINES) -c $< -o $@

$(BINDIR)/%.d : %.cpp
	$(CPP) -MM $(CPPFLAGS) -MT $(BINDIR)/$*.o $< > $@

$(BINDIR)/%.h.gch : %.h
	$(CPP) $(CPPFLAGS) -x c++-header $< -o $@

    
# pull in dependency info for *existing* .o files
-include $(DEPS)

# clean up the build 
clean :
	-rm $(DEPS)
	-rm $(OBJS)	
	-rm $(GEN_CPP)
	-rm $(GEN_HDR)
#	-rm $(PCHS)
	-rm $(TARGET)
	-rmdir $(BINDIR)

