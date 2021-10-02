# Makefile for VRM

TARGET := vrm_test
CFLAGS =
LFLAGS =
LIBS =
SOURCES =
INCLUDES = -I../include
CC = g++
BUILDDIR = obj
TARGETDIR = bin

VRM = ../bin/vrm

SOURCES += test_main.cpp
INCLUDES += -I$(BUILDDIR)

# ------------------------------------ COMMON SET OF RULES ------------------------
MAKEFLAGS += --jobs=8
CFLAGS += $(INCLUDES)

DEPFLAGS = -MT $@ -MMD -MP -MF $(BUILDDIR)/$*.Td
POSTCOMPILE = @mv -f $(BUILDDIR)/$*.Td $(BUILDDIR)/$*.d && touch $@

SRC_STRIP = $(notdir $(SOURCES))
OBJ_STRIP = $(SRC_STRIP:.cpp=.o)
OBJECTS = $(addprefix $(BUILDDIR)/,$(OBJ_STRIP))

#Default make, first rule
all: $(TARGETDIR)/$(TARGET)

run_test: $(TARGETDIR)/$(TARGET)
	$(TARGETDIR)/$(TARGET)

#Remake
remake: cleaner all

#Make the directories
directories:
	@mkdir -p $(TARGETDIR)
	@mkdir -p $(BUILDDIR)

# Clean objs
clean:
	@rm -rf $(BUILDDIR)

# Full Clean
cleaner: clean
	@rm -rf $(TARGETDIR)

#ensure vrm exists
$(VRM):
	$(MAKE) -C ../

# Special rule for VRM headers. Maybe generic rule one day
$(BUILDDIR)/image.h: $(VRM) samples/image.vrm | directories
	$(VRM) samples/image.vrm > $@

# Ensure there is a dependency
$(BUILDDIR)/test_main.o: $(BUILDDIR)/image.h

# Generic rule for compilation
%.o : %.cpp
$(BUILDDIR)/%.o : %.cpp | directories
	$(CC) -c $(CFLAGS) $(DEPFLAGS) $< -o $@
	$(POSTCOMPILE)

$(TARGETDIR)/$(TARGET): $(OBJECTS) | directories
	$(CC) $(CFLAGS) $(LFLAGS) $(OBJECTS) -o $@

$(BUILDDIR)/%.d: ;
.PRECIOUS: $(BUILDDIR)/%.d
.PHONY: all remake clean cleaner

include $(wildcard $(patsubst %,$(BUILDDIR)/%.d,$(basename $(SOURCES))))
