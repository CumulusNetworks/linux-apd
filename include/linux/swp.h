/*
 * Class Driver Model for Network Switch Complex Ports
 *
 * Copyright (C) 2014 Cumulus Networks, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 */
#ifndef __LINUX_SWP
#define __LINUX_SWP

#include <linux/device.h>

#define SWP_MAX_LANES 10

struct swp_device {
	struct device *dev;
	int id;

	const char *label;
	struct device *pluggable;
};

extern int swp_device_register(struct device *dev, struct swp_device *swp);
extern void swp_device_unregister(struct swp_device *swp);

#endif
