/*
 * Class Driver Model for Small Form Factor Pluggable Transceivers
 *
 * Copyright (C) 2014 Cumulus Networks, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 */
#ifndef __LINUX_SFF
#define __LINUX_SFF

#include <linux/device.h>

struct sff_device {
	struct device *dev;
};

extern int sff_device_register(struct device *dev, struct sff_device *sff);
extern void sff_device_unregister(struct sff_device *sff);

#endif
