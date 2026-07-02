   1 CXX := g++
   2 CXXFLAGS := -std=c++23 -Wall -Wextra -O2
   3 TARGET := yascript
   4 SOURCES := yascript-interface.cpp yascript-parser.cpp yascript-runner.cpp
   5 
   6 all: $(TARGET)
   7 
   8 $(TARGET): $(SOURCES)
   9     $(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)
  10 
  11 clean:
  12     rm -f $(TARGET)
