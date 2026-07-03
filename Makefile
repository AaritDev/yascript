CXX = g++
CXXFLAGS = -std=c++23 -O3 -march=native -Wall -Wextra -fno-exceptions -Iinclude
LDFLAGS = -O3

SRCDIR = src
OBJDIR = build
BINDIR = bin

SOURCES = $(SRCDIR)/yascript-lexer.cpp $(SRCDIR)/yascript-parser.cpp $(SRCDIR)/yascript-runner.cpp $(SRCDIR)/yascript-interface.cpp
HEADERS = include/yascript-lexer.hpp include/yascript-parser.hpp include/yascript-runner.hpp include/yascript-interface.hpp
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
TARGET = $(BINDIR)/yascript

.PHONY: all clean directories

all: directories $(TARGET)

directories:
	@mkdir -p $(OBJDIR) $(BINDIR)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJDIR) $(BINDIR)

.SILENT: clean directories
