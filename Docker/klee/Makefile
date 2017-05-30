BUILD ?= $(HOME)/build

all: regular calc klee.bc help

.PHONY: help
help:
	@ echo "Most important make targets:"
	@ echo " klee   -> Analyze the example function with KLEE"
	@ echo " max    -> generates test cases for max"
	@ echo " simple -> generates test cases for simpleCalc"
	@ echo " five   -> generates a small array result"


%: %.c
	$(BUILD)/llvm/Release/bin/clang -O3 $< -o $@

.PHONY: klee
klee: klee.bc
	$(BUILD)/klee/Release+Asserts/bin/klee $<
	@ echo "Press any key to show testcases"
	for ktest in ./klee-last/*.ktest; do $(BUILD)/klee/Release+Asserts/bin/ktest-tool --write-ints $$ktest; echo ""; done

%.bc: %.c
	$(BUILD)/llvm/Release/bin/clang -c -g -O0 $< -emit-llvm -I$(BUILD)/klee/include/ -o $@

.PHONY: max
max: calc.c
	python3 syminputC.py max $<

.PHONY: simple
simple: calc.c
	python3 syminputC.py simpleCalc $<

.PHONY: five
five: calc.c
	python3 syminputC.py fiveInts $<

PHONY: clean
clean:
	@ rm -f regular
	@ rm -f calc
	@ rm -f *.bc
	@ rm -f klee-last
	@ rm -rf klee-out-*
	@ rm -rf __pycache__
