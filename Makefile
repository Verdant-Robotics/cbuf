# Makefile for VRM

TARGET := vrm
CFLAGS = 
LFLAGS = 
LIBS = 
SOURCES = 
INCLUDES = -Iinclude
CC = g++
BUILDDIR = obj
TARGETDIR = bin


SOURCES += src/AstPrinter.cpp src/CPrinter.cpp src/FileData.cpp src/Lexer.cpp
SOURCES += src/Parser.cpp src/PoolAllocator.cpp src/StringBuffer.cpp src/SymbolTable.cpp
SOURCES += src/TextType.cpp src/Token.cpp src/vrm.cpp

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

# Generic rule for compilation
%.o : %.cpp
$(BUILDDIR)/%.o : src/%.cpp | directories
	$(CC) -c $(CFLAGS) $(DEPFLAGS) $< -o $@
	$(POSTCOMPILE)

$(TARGETDIR)/$(TARGET): $(OBJECTS) | directories
	$(CC) $(CFLAGS) $(LFLAGS) $(OBJECTS) -o $@

$(BUILDDIR)/%.d: ;
.PRECIOUS: $(BUILDDIR)/%.d
.PHONY: all remake clean cleaner

include $(wildcard $(patsubst %,$(BUILDDIR)/%.d,$(basename $(SOURCES))))
