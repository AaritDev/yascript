CXX := g++

CXXFLAGS := -std=c++23 -O3 -Wall -Wextra -fno-exceptions -Iinclude -pipe -flto
LDFLAGS := 	-flto
MAKEFLAGS += -j$(shell nproc)

ifeq ($(NATIVE),1)
CXXFLAGS += -march=native
endif

SRCDIR := src
OBJDIR := build
BINDIR := bin

TARGET := $(BINDIR)/yascript

SOURCES := $(wildcard $(SRCDIR)/*.cpp)
OBJECTS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SOURCES))
DEPS := $(OBJECTS:.o=.d)

.PHONY: all clean test install uninstall directories

all: $(TARGET)

directories:
	mkdir -p $(OBJDIR) $(BINDIR)

$(TARGET): directories $(OBJECTS)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | directories
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

test: all
	./tests/run_tests.sh

install: all
	install -Dm755 $(TARGET) /usr/local/bin/yascript

uninstall:
	rm -f /usr/local/bin/yascript

clean:
	rm -rf $(OBJDIR) $(BINDIR)

-include $(DEPS)
