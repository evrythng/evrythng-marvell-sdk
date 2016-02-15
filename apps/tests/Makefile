# Copyright (C) 2008-2012 Marvell International Ltd.
# All Rights Reserved.
#
# Application Makefile
#
#    Builds and Installs Application ELF Executable Image (.axf)
#    and image suitable for flashing (.bin).
#
#    The Makefile is built using Makefile.targets
#    available in $(TOOLCHAIN_DIR).
#
# Usage:
#
#     Targets:
#
#	  all: Builds the firmware binary image in both ELF (axf) format
#          (suitable for loading into RAM) and bin format (suitable
#          for flashing)
#
#          Should be called with SDK_PATH set to root of a pre-built
#          SDK against which the application should be built.
#
# 		   If BOARD_FILE is set (to the absolute path to the file),
#		   then the file is copied locally (to $(SRC_DIR)/board.c) and
#          then used for build. If its not present, then the application
#          sources should have the needed board-specific functions.
#
#     clean: Cleans all the build artifacts
#
#     install: Installs ELF image, Bin Image, and MAP file to
#              $(INSTALL_DIR). By default INSTALL_DIR = ./bin.
#
# Description:
#
#   Minimally, in this file, only the following need to be specified.
#
#   SRCS     = list of source files
#   DST_NAME = prefix to be used for generated build artifcats.
#
#   Default Variables:
#
#   A number of variables are used in Makefile.targets that can be
#   overridden here.
#
#   SRC_DIR:  directory for source files (default ./src)
#   OBJ_DIR:  directory for intermediate object files (default ./obj)
#   BIN_DIR:  directory for final build artifacts (default ./bin)
#
#   LDSCRIPT: Linker script (default $(TOOLCHAIN_DIR)/mc200.ld)
#   EXTRACFLAGS: pass any additional CFLAGS to be passed to the C Compiler.

LDSCRIPT = $(TOOLCHAIN_DIR)/$(ARCH_NAME-y).ld

DST_NAME = evrythng_tests

$(shell cp $(PROJECT_ROOT)/lib/core/tests/tests.c src)
$(shell cp $(PROJECT_ROOT)/lib/core/tests/CuTest.c src)

SRCS = main.c CuTest.c tests.c $(SRCS-OPT) board.c

EXTRACFLAGS += -D APPCONFIG_DEBUG_ENABLE=1 \
			   -I $(SDK_PATH)/incl/evrythng \
			   -I $(PROJECT_ROOT)/lib/core/tests \
			   $(EXTRACFLAGS-y) \

SRCS += $(SRCS-y)

include $(TOOLCHAIN_DIR)/targets.mk
include $(TOOLCHAIN_DIR)/rules.mk
