## Logger Makefile
##
## Copyright 2025-2026, Dru Satori @ Druware Software Designs.
## All rights reserved. Distributed under the terms of the MIT License.
##
## Builds Logger from the command line with Haiku's makefile-engine:
##	make				builds a release build
##	make DEBUGGER=TRUE	builds with debug information
##	make clean			removes the objects and the application
##	make install		copies Logger to INSTALL_DIR

NAME = Logger
TYPE = APP
APP_MIME_SIG = application/x-vnd.Logger

SRCS = \
	src/main.cpp \
	src/LogRecord.cpp \
	src/LogParser.cpp \
	src/LogTailer.cpp \
	src/LogFilter.cpp \
	src/LogColumns.cpp \
	src/FilterView.cpp \
	src/LoggerApp.cpp \
	src/LogWindow.cpp \
	src/LogListView.cpp

RDEFS = Logger.rdef

LIBS = be tracker columnlistview $(STDCPPLIBS)

LOCAL_INCLUDE_PATHS = src

OPTIMIZE := FULL
WARNINGS = ALL
## C++ only: the rdef is run through the C preprocessor.
CXXFLAGS += -std=c++17

INSTALL_DIR = $(shell finddir B_USER_NONPACKAGED_DIRECTORY)/apps

## Include the makefile-engine
DEVEL_DIRECTORY := \
	$(shell findpaths -r "makefile_engine" B_FIND_PATH_DEVELOP_DIRECTORY)
include $(DEVEL_DIRECTORY)/etc/makefile-engine
