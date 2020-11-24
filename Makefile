CXX = clang++
CXXFLAGS = -std=c++2a -Ofast -DNDEBUG -march=native -ggdb3 -pthread -I./src/
LDFLAGS = -pthread -laio -lssl -lcrypto -lyaml-cpp

# Uncomment for tfhe support
CXXFLAGS += -DTFHE
LDFLAGS += -ltfhe-spqlios-fma

# Uncomment for ckks support
CXXFLAGS += -DCKKS -I/usr/local/include/SEAL-3.6
LDFLAGS += -lseal

MAGE_DIRS = src src/dsl src/platform src/crypto src/crypto/ot src/memprog src/engine src/programs src/schemes src/util
MAGE_CPP_SOURCES = $(foreach dir,$(MAGE_DIRS),$(wildcard $(dir)/*.cpp))
MAGE_HEADERS = $(foreach dir,$(MAGE_DIRS),$(wildcard $(dir)/*.hpp))

MAGE_TEST_SOURCES = $(wildcard tests/*.cpp)
MAGE_EXECUTABLE_SOURCES = $(wildcard src/executables/*.cpp)
MAGE_EXECUTABLE_NAMES = $(foreach file,$(MAGE_EXECUTABLE_SOURCES),$(notdir $(basename $(file))))

BINDIR = bin
MAGE_OBJECTS = $(addprefix $(BINDIR)/,$(MAGE_CPP_SOURCES:.cpp=.o))
TEST_OBJECTS = $(addprefix $(BINDIR)/,$(MAGE_TEST_SOURCES:.cpp=.o))
EXECUTABLES = $(addprefix $(BINDIR)/,$(MAGE_EXECUTABLE_NAMES))

.PHONY: clean

default: $(EXECUTABLES)

all: $(EXECUTABLES) tests

tests: $(BINDIR)/test

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
