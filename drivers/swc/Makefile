#
# Makefile for swc drivers.
#

obj-$(CONFIG_SWC)		+= swc.o
obj-$(CONFIG_SWP)		+= swp.o
obj-$(CONFIG_SWC_FW)		+= swc-fw-util.o swc-fw.o
obj-$(CONFIG_SWC_CPLD)		+= swc-fw-util.o swc-cpld-fw.o
obj-$(CONFIG_SWC_CPLD_REGISTER)	+= swc-fw-util.o swc-cpld-register-fw.o
obj-$(CONFIG_SWC_CPLD_GPIO)	+= swc-fw-util.o swc-cpld-gpio-fw.o
obj-$(CONFIG_SWC_CPLD_I2C_MUX)	+= swc-fw-util.o swc-cpld-i2c-mux-fw.o
obj-$(CONFIG_SWP_FW)		+= swc-fw-util.o swp-fw.o
obj-$(CONFIG_SFF_FW)		+= swc-fw-util.o sff-fw.o
obj-$(CONFIG_SFF_TWI)		+= sff-twi.o
obj-$(CONFIG_SWC_FAN)		+= swc-fw-util.o swc-fan.o
obj-$(CONFIG_SWC_LED)		+= swc-fw-util.o swc-led.o
obj-$(CONFIG_SWC_PSU)		+= swc-fw-util.o swc-psu.o
obj-$(CONFIG_SWC_THERMAL)	+= swc-fw-util.o swc-thermal.o

ccflags-$(CONFIG_SWC_DEBUG) := -DDEBUG
