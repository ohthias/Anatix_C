# Testes no PC (sem placa): make -C extras/test
CXX      ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -Wno-unused-parameter
INC      = -I. -I../../src
BUILD    = build

test: $(BUILD)/test_anatix $(BUILD)/test_disable
	./$(BUILD)/test_anatix
	./$(BUILD)/test_disable && echo "ANATIX_DISABLE compila sem codigo de telemetria"

$(BUILD)/test_anatix: test_anatix.cpp ../../src/Anatix.cpp ../../src/Anatix.h Arduino.h
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INC) test_anatix.cpp ../../src/Anatix.cpp -o $@

$(BUILD)/test_disable: test_disable.cpp ../../src/Anatix.h Arduino.h
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INC) test_disable.cpp -o $@

clean:
	rm -rf $(BUILD)

.PHONY: test clean
