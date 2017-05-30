all: simple complex insane help

.PHONY: clean
clean:
	@ rm -f simple complex insane

.PHONY: help
help:
	@ echo "Most important make targets:"
	@ echo " angr-simple   -> run angr on a simple example program"
	@ echo " angr-complex  -> run angr on am more complex program"
	@ echo " angr-insane   -> run angr on an obvuscated program"

%: %.c
	clang-3.9 -O3 $< -o $@

.PHONY: angr-simple
angr-simple: simple
	python2 run-angr.py -v $<

.PHONY: angr-complex
angr-complex: complex
	python2 run-angr.py $<

.PHONY: angr-insane
angr-insane: insane
	python2 run-angr.py $<
