#
# Makefile for swc drivers.
#

obj-$(CONFIG_SWC)		+= swc.o
obj-$(CONFIG_SWP)		+= swp.o
obj-$(CONFIG_SWC_FW)		+= swc-fw-util.o swc-fw.o
obj-$(CONFIG_SWP_FW)		+= swc-fw-util.o swp-fw.o
obj-$(CONFIG_SFF_FW)		+= swc-fw-util.o sff-fw.o
obj-$(CONFIG_SFF_TWI)		+= sff-twi.o

ccflags-$(CONFIG_SWC_DEBUG) := -DDEBUG
