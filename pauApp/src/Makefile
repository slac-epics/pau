#-----------------------------------------
# $(PAU)/pauApp/src/Makefile
#
# Author: Kukhee Kim
# Date:   22 Jan 2010
#
#-----------------------------------------

#-----------------------
# Set up the Environment
#

TOP = ../..
SOURCE_DIR := $(patsubst %../.., %., $(TOP))

include $(TOP)/configure/CONFIG

CROSS_COMPILER_TARGET_ARCHS=RTEMS-beatnik

#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

BUILDFOR=_RTEMS

EVR_IOC_LIBS += evrSupport
EVR_IOC_LIBS += generalTime

#=============================
# Build the IOC support library


LIBRARY_IOC$(BUILDFOR) += pau

INC += drvPau.h  drvPauReport.h  drvPauTimer.h

DBD += drvPau.dbd
DBD += devPau.dbd
DBD += drvPauUsr.dbd


# Compile and add the code to the support library
pau_SRCS += drvPau.c
pau_SRCS += drvPauReport.c
pau_SRCS += drvPauTimer.c
pau_SRCS += devPau.c

pau_SRCS += drvPauUsr.c

pau_DBD += drvPau.dbd
pau_DBD += devPau.dbd
pau_DBD += drvPauUsr.dbd

pau_LIBS += $(EVR_IOC_LIBS)




include $(TOP)/configure/RULES
