# Standalone Makefile for building dynostatic-buffer in this repository.
#
# The library's build settings live in a reusable, includable fragment
# (library/dynostatic-buffer.mk) so other projects can vendor the library and
# `include` that fragment directly. This top-level Makefile just consumes the
# same fragment and adds the extras that only make sense in-tree: a shared
# library and the example program.
#
# Common targets:
#   make            # build the static and shared libraries (default)
#   make static     # build only libdynostatic_buffer.a
#   make shared     # build only libdynostatic_buffer.so
#   make example    # build and statically link the example program
#   make clean      # remove all build artefacts
#
# Override the compiler or options from the command line, e.g.:
#   make CC=clang
#   make DS_ZERO_ON_FREE=1

# ---- Layout ----------------------------------------------------------------

BUILD_DIR := build/make

# Point the library fragment at our build directory, then pull it in. This must
# be set before the include because the fragment reads it with ?=.
DYNOSTATIC_BUFFER_BUILD_DIR := $(BUILD_DIR)
include library/dynostatic-buffer.mk

SHARED_LIB := $(BUILD_DIR)/libdynostatic_buffer.so

# One binary per example source. Drop a new .c file into examples/ and it is
# built automatically; the binary keeps the source's base name.
EXAMPLE_SRCS := $(wildcard examples/*.c)
EXAMPLE_HDR  := examples/example_common.h
EXAMPLE_BINS := $(patsubst examples/%.c,$(BUILD_DIR)/%,$(EXAMPLE_SRCS))

# ---- Targets ---------------------------------------------------------------

.PHONY: all static shared examples example clean

all: static shared

# The static library is built entirely by the included fragment.
static: $(DYNOSTATIC_BUFFER_A)
shared: $(SHARED_LIB)

# `examples` builds them all; `example` is kept as a backwards-compatible alias.
examples: $(EXAMPLE_BINS)
example: examples

# Shared library: compiled -fPIC from the same source/flags the fragment uses.
$(SHARED_LIB): $(DYNOSTATIC_BUFFER_SRC) $(DYNOSTATIC_BUFFER_HDR)
	@mkdir -p $(@D)
	$(CC) $(DYNOSTATIC_BUFFER_CPPFLAGS) -DDS_ZERO_ON_FREE=$(DS_ZERO_ON_FREE) \
		$(DYNOSTATIC_BUFFER_CFLAGS) -fPIC -shared $< -o $@

# Each example links the archive directly so the linker cannot substitute the
# shared library, which would leave the binary needing libdynostatic_buffer.so
# at run time. Note it compiles with DYNOSTATIC_BUFFER_CPPFLAGS, exactly as an
# external consumer of the fragment would.
$(BUILD_DIR)/%: examples/%.c $(EXAMPLE_HDR) $(DYNOSTATIC_BUFFER_A)
	@mkdir -p $(@D)
	$(CC) $(DYNOSTATIC_BUFFER_CPPFLAGS) -Iexamples $(DYNOSTATIC_BUFFER_CFLAGS) \
		$< $(DYNOSTATIC_BUFFER_A) -o $@

clean:
	$(RM) -r $(BUILD_DIR)
