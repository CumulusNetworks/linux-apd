/*
 * Class Driver Model for Network Switch Complex
 *
 * Copyright (C) 2014 Cumulus Networks, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 */
#ifndef __LINUX_SWC
#define __LINUX_SWC

#include <linux/device.h>
#include <linux/swp.h>

struct swc_device {
	struct device *dev;
	struct device *onie_eeprom;
	struct device *mgmt_eth;
	struct device *mgmt_serial;
	struct device *leds;
};

extern struct class swc_class;

extern int swc_device_register(struct device *dev, struct swc_device *swc);
extern void swc_device_unregister(struct swc_device *swc);
extern int swc_add_swp(struct device *swc, struct device *swp, int id);
extern void swc_del_swp(struct device *swc, struct device *swp, int id);

#endif
