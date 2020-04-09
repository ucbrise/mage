CXX = g++-9
CXXFLAGS = -std=c++2a -O0 -ggdb3 -pthread -I./src/
LDFLAGS = -pthread

MAGE_DIRS = src src/planner src/platform src/loader src/schemes/ag2pc src/util
MAGE_CPP_SOURCES = $(foreach dir,$(MAGE_DIRS),$(wildcard $(dir)/*.cpp))
MAGE_HEADERS = $(foreach dir,$(MAGE_DIRS),$(wildcard $(dir)/*.hpp))

MAGE_TEST_SOURCES = $(wildcard tests/*.cpp)

BINDIR = bin
MAGE_OBJECTS = $(addprefix $(BINDIR)/,$(MAGE_CPP_SOURCES:.cpp=.o))
TEST_OBJECTS = $(addprefix $(BINDIR)/,$(TEST_CPP_SOURCES:.cpp=.o))

.PHONY: clean

all: mage tests

tests: $(BINDIR)/test_prioqueue

mage: $(BINDIR)/converter $(BINDIR)/allocator

$(BINDIR)/test_prioqueue: $(MAGE_OBJECTS) $(BINDIR)/tests/test_prioqueue.o
	$(CXX) $(LDFLAGS) $+ -lboost_unit_test_framework -o $@

$(BINDIR)/tests/%.o: tests/%.cpp $(MAGE_HEADERS)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BINDIR)/converter: $(MAGE_OBJECTS) $(BINDIR)/src/executables/converter.o
	$(CXX) $(LDFLAGS) $+ -o $@

$(BINDIR)/allocator: $(MAGE_OBJECTS) $(BINDIR)/src/executables/allocator.o
	$(CXX) $(LDFLAGS) $+ -o $@

$(BINDIR)/src/%.o: src/%.cpp $(MAGE_HEADERS)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf bin
