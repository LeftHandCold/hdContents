# GNU Makefile

build ?= release

OUT := build/$(build)

default: all


# Do not specify CFLAGS or LIBS on the make invocation line - specify
# XCFLAGS or XLIBS instead. Make ignores any lines in the makefile that
# set a variable that was set on the command line.
CFLAGS += $(XCFLAGS) -Iinclude
LIBS += $(XLIBS) -lm

LIBS += $(ZLIB_LIBS)

CFLAGS += $(ZLIB_CFLAGS)

ALL_DIR += $(OUT)/source/hdtd
ALL_DIR += $(OUT)/source/pdf

# --- Commands ---

ifneq "$(verbose)" "yes"
QUIET_AR = @ echo ' ' ' ' AR $@ ;
QUIET_CC = @ echo ' ' ' ' CC $@ ;
QUIET_CXX = @ echo ' ' ' ' CXX $@ ;
QUIET_GEN = @ echo ' ' ' ' GEN $@ ;
QUIET_LINK = @ echo ' ' ' ' LINK $@ ;
QUIET_MKDIR = @ echo ' ' ' ' MKDIR $@ ;
QUIET_RM = @ echo ' ' ' ' RM $@ ;
QUIET_TAGS = @ echo ' ' ' ' TAGS $@ ;
QUIET_WINDRES = @ echo ' ' ' ' WINDRES $@ ;
endif

CC_CMD = $(QUIET_CC) $(CC) $(CFLAGS) -o $@ -c $<
CXX_CMD = $(QUIET_CXX) $(CXX) $(filter-out -Wdeclaration-after-statement,$(CFLAGS)) -o $@ -c $<
AR_CMD = $(QUIET_AR) $(AR) cr $@ $^
LINK_CMD = $(QUIET_LINK) $(CC) $(LDFLAGS) -o $@ $^ $(LIBS)
MKDIR_CMD = $(QUIET_MKDIR) mkdir -p $@
RM_CMD = $(QUIET_RM) rm -f $@
TAGS_CMD = $(QUIET_TAGS) ctags $^
WINDRES_CMD = $(QUIET_WINDRES) $(WINDRES) $< $@

# --- Rules ---

$(ALL_DIR) $(OUT) :
	$(MKDIR_CMD)

$(OUT)/%.a :
	$(AR_CMD)

$(OUT)/%.exe: $(OUT)/%.o | $(ALL_DIR)
	$(LINK_CMD)


$(OUT)/%.o : %.c | $(ALL_DIR)
	$(CC_CMD)

.PRECIOUS : $(OUT)/%.o # Keep intermediates from chained rules

# --- File lists ---

HDTDHDR := include/hdtd.h $(wildcard include/hdtd/*.h)
PDF_HDR := include/pdf.h $(wildcard include/pdf/*.h)

HDTDSRC := $(sort $(wildcard source/hdtd/*.c))
PDF_SRC := $(sort $(wildcard source/pdf/*.c))

HDTDSRC_HDR := $(wildcard source/hdtd/*.h)
PDF_SRC_HDR := $(wildcard source/pdf/*.h)

HDTDOBJ := $(HDTDSRC:%.c=$(OUT)/%.o)
PDF_OBJ := $(PDF_SRC:%.c=$(OUT)/%.o)

$(HDTDOBJ) : $(HDTDHDR) $(HDTDSRC_HDR)
$(PDF_OBJ) : $(HDTDHDR) $(PDF_HDR) $(PDF_SRC_HDR)
$(PDF_OBJ) : $(HDTDSRC_HDR) 


# --- Library ---

HDCONTENTS_LIB = $(OUT)/libhdcontents.a

MHDCONTENTS_OBJ := \
	$(HDTDOBJ) \
	$(PDF_OBJ) 

$(HDCONTENTS_LIB) : $(MHDCONTENTS_OBJ)

INSTALL_LIBS := $(HDCONTENTS_LIB)

libs: $(HDCONTENTS_LIB)


# --- Clean and Default ---


all: libs

clean:
	rm -rf $(OUT)

release:
	$(MAKE) build=release
debug:
	$(MAKE) build=debug

.PHONY: all clean libs
