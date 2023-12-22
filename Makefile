help:
	@echo 'Available make targets:'
	@grep PHONY: Makefile | cut -d: -f2 | sed '1d;s/^/make/'


_bld: gen
pdoc_bug_poc.cpython-%: bld

.PHONY: gen               # Generate cmake to _bld directory
gen:
	cmake -B _bld .

.PHONY: bld               # Build module from cmake. Module will be pdoc_bug_poc.cpython-311-x86_64-linux-gnu.so or whatever py version
bld: _bld
	cmake --build _bld
	mv _bld/*.so ./

.PHONY: cln               # Clean build artifacts
cln:
	rm -rf _bld *.so

.PHONY: bug               # Produce bug
bug: bld
	pdoc --html --force pdoc_bug_poc