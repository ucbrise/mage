CXX = g++-9
CXXFLAGS = -std=c++2a -O0 -ggdb3 -pthread -I./src/
LDFLAGS = -pthread

MAGE_DIRS = src src/planner src/platform src/loader src/schemes/ag2pc
MAGE_CPP_SOURCES = $(foreach dir,$(MAGE_DIRS),$(wildcard $(dir)/*.cpp))
MAGE_HEADERS = $(foreach dir,$(MAGE_DIRS),$(wildcard $(dir)/*.hpp))

BINDIR = bin
MAGE_OBJECTS = $(addprefix $(BINDIR)/,$(MAGE_CPP_SOURCES:.cpp=.o))

.PHONY: clean

all: $(BINDIR)/converter $(BINDIR)/allocator

$(BINDIR)/converter: $(MAGE_OBJECTS) $(BINDIR)/src/executables/converter.o
	$(CXX) $(LDFLAGS) $+ -o $@

$(BINDIR)/allocator: $(MAGE_OBJECTS) $(BINDIR)/src/executables/allocator.o
	$(CXX) $(LDFLAGS) $+ -o $@

$(BINDIR)/%.o: %.cpp $(MAGE_HEADERS)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf bin
