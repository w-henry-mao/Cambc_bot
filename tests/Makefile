# Host-side unit tests for code under src/combat (no cambc SDK). Run: make -C tests test
CXX ?= clang++
CXXFLAGS = -std=c++20 -O1 -Wall -Wextra -I../src
TEST_SRCS = $(wildcard test_*.cpp)
COMBAT_SRCS = $(filter-out ../src/combat/sdk_%.cpp,$(wildcard ../src/combat/*.cpp))
build/tests: $(TEST_SRCS) $(COMBAT_SRCS) $(wildcard ../src/combat/*.h) test.h
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(TEST_SRCS) $(COMBAT_SRCS) -o $@
test: build/tests
	./build/tests
.PHONY: test
