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

#=============================
# Build the IOC support library


LIBRARY_IOC$(BUILDFOR) += pau

INC += drvPau.h  drvPauReport.h  drvPauTimer.h 

DBD += drvPau.dbd
DBD += devPau.dbd
DBD += drvPauUsr.dbd
DBD += sncPau.dbd


# Compile and add the code to the support library
pau_SRCS += drvPau.c
pau_SRCS += drvPauReport.c
pau_SRCS += drvPauTimer.c
pau_SRCS += devPau.c
pau_SRCS += drvPauUsr.c
pau_SRCS += sncPau.stt
pau_SRCS += sncPauFcomPush.stt

pau_DBD += drvPau.dbd
pau_DBD += devPau.dbd
pau_DBD += drvPauUsr.dbd
pau_DBD += sncPau.dbd

pau_LIBS += $(EVR_IOC_LIBS)
pau_LIBS += seq pv




include $(TOP)/configure/RULES
