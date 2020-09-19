CXX = clang++
CXXFLAGS = -std=c++2a -Ofast -DNDEBUG -march=native -ggdb3 -pthread -I./src/
LDFLAGS = -pthread -laio -lssl -lcrypto -lyaml-cpp

MAGE_DIRS = src src/dsl src/platform src/crypto src/crypto/ot src/memprog src/engine src/schemes src/util
MAGE_CPP_SOURCES = $(foreach dir,$(MAGE_DIRS),$(wildcard $(dir)/*.cpp))
MAGE_HEADERS = $(foreach dir,$(MAGE_DIRS),$(wildcard $(dir)/*.hpp))

MAGE_TEST_SOURCES = $(wildcard tests/*.cpp)

BINDIR = bin
MAGE_OBJECTS = $(addprefix $(BINDIR)/,$(MAGE_CPP_SOURCES:.cpp=.o))
TEST_OBJECTS = $(addprefix $(BINDIR)/,$(MAGE_TEST_SOURCES:.cpp=.o))

.PHONY: clean

default: mage

all: mage tests

tests: $(BINDIR)/test

mage: $(BINDIR)/mage $(BINDIR)/aspirin_input $(BINDIR)/aspirin

$(BINDIR)/test: $(MAGE_OBJECTS) $(TEST_OBJECTS)
	$(CXX) $(LDFLAGS) $+ -lboost_unit_test_framework -o $@

$(BINDIR)/tests/%.o: tests/%.cpp $(MAGE_HEADERS)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BINDIR)/%: $(MAGE_OBJECTS) $(BINDIR)/src/executables/%.o
	$(CXX) $(LDFLAGS) $+ -o $@

$(BINDIR)/src/%.o: src/%.cpp $(MAGE_HEADERS)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf bin
