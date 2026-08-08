# Reusable Makefile fragment for the dynostatic-buffer library.
#
# Drop dynostatic-buffer into another project (submodule, vendored copy, ...)
# and `include` this file from that project's Makefile to build and link the
# library without duplicating its compile settings:
#
#     include third_party/dynostatic-buffer/library/dynostatic-buffer.mk
#
#     # Compile YOUR translation units that include the header with the flags
#     # below: the DS_* defines change sizeof(dynostatic_buffer_t), so every
#     # consumer of the header MUST see the same values as the library.
#     app.o: app.c
#         $(CC) $(DYNOSTATIC_BUFFER_CPPFLAGS) -c $< -o $@
#
#     # Link against the archive this fragment knows how to build.
#     app: app.o $(DYNOSTATIC_BUFFER_A)
#         $(CC) $^ -o $@
#
# Everything is namespaced with the DYNOSTATIC_BUFFER_ / DS_ prefixes so it will
# not collide with the including project's own variables and targets.
#
# Knobs (override before or after the include, or on the command line):
#   DYNOSTATIC_BUFFER_BUILD_DIR   where the archive/objects are written
#   DYNOSTATIC_BUFFER_CFLAGS      compile flags used to build the library
#   DS_ZERO_ON_FREE               1 to zero freed blocks (library-private)
#   DS_BUFFER_MEMORY_SIZE, DS_LOG_ENABLE,
#   DS_MAX_ALLOCATION_COUNT, DS_MAX_ALLOCATION_SIZE   layout defines
#
# Exported for the including project to use:
#   DYNOSTATIC_BUFFER_DIR         directory of this library
#   DYNOSTATIC_BUFFER_INCLUDES    -I flags for the public header
#   DYNOSTATIC_BUFFER_DEFINES     -D flags every consumer must compile with
#   DYNOSTATIC_BUFFER_CPPFLAGS    INCLUDES + DEFINES, for consumer compiles
#   DYNOSTATIC_BUFFER_A           path to the built static library

# Absolute path to the directory holding this fragment, computed the moment the
# fragment is included so it is correct no matter where the parent Makefile
# lives or which directory make runs from. Must use := (immediate) and be the
# first thing evaluated so $(lastword $(MAKEFILE_LIST)) is still this file.
DYNOSTATIC_BUFFER_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

# ---- Toolchain (only set if the parent hasn't) -----------------------------

CC ?= cc
AR ?= ar
RM ?= rm -f

# ---- Sources & outputs -----------------------------------------------------

DYNOSTATIC_BUFFER_SRC := $(DYNOSTATIC_BUFFER_DIR)/dynostatic-buffer.c
DYNOSTATIC_BUFFER_HDR := $(DYNOSTATIC_BUFFER_DIR)/dynostatic-buffer.h

# Where build artefacts land. Relative to the parent's working directory by
# default; override to keep several consumers isolated.
DYNOSTATIC_BUFFER_BUILD_DIR ?= build/dynostatic-buffer

DYNOSTATIC_BUFFER_OBJ := $(DYNOSTATIC_BUFFER_BUILD_DIR)/dynostatic-buffer.o
DYNOSTATIC_BUFFER_A   := $(DYNOSTATIC_BUFFER_BUILD_DIR)/libdynostatic_buffer.a

# ---- Public configuration (layout-affecting; keep consumer == library) -----

DS_BUFFER_MEMORY_SIZE   ?= 1024
DS_LOG_ENABLE           ?= 1u
DS_MAX_ALLOCATION_COUNT ?= 10
DS_MAX_ALLOCATION_SIZE  ?= 512

DYNOSTATIC_BUFFER_INCLUDES := -I$(DYNOSTATIC_BUFFER_DIR)

DYNOSTATIC_BUFFER_DEFINES := \
	-DDS_BUFFER_MEMORY_SIZE=$(DS_BUFFER_MEMORY_SIZE) \
	-DDS_LOG_ENABLE=$(DS_LOG_ENABLE) \
	-DDS_MAX_ALLOCATION_COUNT=$(DS_MAX_ALLOCATION_COUNT) \
	-DDS_MAX_ALLOCATION_SIZE=$(DS_MAX_ALLOCATION_SIZE)

# Flags a consumer must use when compiling its own code that includes the
# public header (the defines above alter the struct layout).
DYNOSTATIC_BUFFER_CPPFLAGS := $(DYNOSTATIC_BUFFER_INCLUDES) $(DYNOSTATIC_BUFFER_DEFINES)

# ---- Private configuration -------------------------------------------------

# DS_ZERO_ON_FREE only changes ds_free()'s runtime behaviour, not the struct
# layout, so it is applied to the library translation unit alone.
DS_ZERO_ON_FREE ?= 0

# Flags used to build the library itself. Self-contained (C11 is required by
# the header) so it does not inherit a possibly C++-oriented parent CFLAGS.
DYNOSTATIC_BUFFER_CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Wpedantic

# ---- Rules -----------------------------------------------------------------

.PHONY: dynostatic-buffer dynostatic-buffer-clean

# Convenience alias so `make dynostatic-buffer` builds the archive.
dynostatic-buffer: $(DYNOSTATIC_BUFFER_A)

$(DYNOSTATIC_BUFFER_OBJ): $(DYNOSTATIC_BUFFER_SRC) $(DYNOSTATIC_BUFFER_HDR)
	@mkdir -p $(@D)
	$(CC) $(DYNOSTATIC_BUFFER_CPPFLAGS) -DDS_ZERO_ON_FREE=$(DS_ZERO_ON_FREE) \
		$(DYNOSTATIC_BUFFER_CFLAGS) -c $< -o $@

$(DYNOSTATIC_BUFFER_A): $(DYNOSTATIC_BUFFER_OBJ)
	$(AR) rcs $@ $^

dynostatic-buffer-clean:
	$(RM) $(DYNOSTATIC_BUFFER_OBJ) $(DYNOSTATIC_BUFFER_A)
