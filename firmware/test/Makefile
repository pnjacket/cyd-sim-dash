# Host build of the display-state engine's unit tier.
#
# The engine is deliberately free of Arduino headers so it compiles here with nothing but a C++
# compiler — which is also what lets E2E-STANDARD clause 6 import it to compute expectations.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O1

# Two binaries, because they exercise two separable things: the display-state engine, and the
# API-STATE projection that the end-to-end tests read. Both compile here with nothing but a C++
# compiler, which is the property that keeps the unit tier off the critical path of rig access.
ENGINE_SRC = ../src/display_state.cpp test_display_state.cpp
ENGINE_OUT = test_display_state

JSON_SRC   = ../src/state_json.cpp test_state_json.cpp
JSON_OUT   = test_state_json

.PHONY: all test clean
all: $(ENGINE_OUT) $(JSON_OUT)

$(ENGINE_OUT): $(ENGINE_SRC) ../src/display_state.h
	$(CXX) $(CXXFLAGS) -o $(ENGINE_OUT) $(ENGINE_SRC)

$(JSON_OUT): $(JSON_SRC) ../src/state_json.h ../src/net_counters.h ../src/display_state.h
	$(CXX) $(CXXFLAGS) -o $(JSON_OUT) $(JSON_SRC)

test: $(ENGINE_OUT) $(JSON_OUT)
	./$(ENGINE_OUT)
	./$(JSON_OUT)

clean:
	rm -f $(ENGINE_OUT) $(ENGINE_OUT).exe $(JSON_OUT) $(JSON_OUT).exe
