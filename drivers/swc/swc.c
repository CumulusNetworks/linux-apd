/*
 *  swc.c - Switch Complex Class Driver
 *
 *  Copyright (C) 2014, Cumulus Networks, Inc.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/kernel.h>
#include <linux/kdev_t.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/swc.h>
#include <linux/swp.h>

MODULE_AUTHOR("Dustin Byford");
MODULE_DESCRIPTION("Network Switch Complex Class Driver");
MODULE_LICENSE("GPL");

struct class swc_class = {
	.name = "swc",
	.owner = THIS_MODULE,
};
EXPORT_SYMBOL(swc_class);

DEFINE_IDA(swc_ida);

int swc_device_register(struct device *dev, struct swc_device *swc)
{
        int id;
	int err;

        id = ida_simple_get(&swc_ida, 0, 0, GFP_KERNEL);
        if (id < 0) {
		err = id;
		goto fail_ida;
	}

        swc->dev = device_create(&swc_class, dev, MKDEV(0, 0),
                                 swc, "swc%d", id);
	if (IS_ERR(swc->dev)) {
                err = -ENODEV;
                goto fail_dev;
        }

	if (swc->onie_eeprom) {
		err = sysfs_create_link(&swc->dev->kobj, &swc->onie_eeprom->kobj,
					"onie_eeprom");
	}

	if (swc->mgmt_eth) {
		err = sysfs_create_link(&swc->dev->kobj, &swc->mgmt_eth->kobj,
					"management_ethernet");
	}

	if (swc->mgmt_serial) {
		err = sysfs_create_link(&swc->dev->kobj, &swc->mgmt_serial->kobj,
					"management_serial");
	}

	if (swc->leds) {
		err = sysfs_create_link(&swc->dev->kobj, &swc->leds->kobj,
					"leds");
	}

	dev_info(dev, "registered switch complex %s\n", dev_name(swc->dev));

	return 0;

fail_dev:
        ida_simple_remove(&swc_ida, id);
fail_ida:
        return err;
}
EXPORT_SYMBOL(swc_device_register);

void swc_device_unregister(struct swc_device *swc)
{
        int id;

	dev_info(swc->dev, "unregistering\n");
       
        if (likely(sscanf(dev_name(swc->dev), "swc%d", &id) == 1)) {
                device_unregister(swc->dev);
                ida_simple_remove(&swc_ida, id);
        } else {
                dev_err(swc->dev, "failed to unregister swc device\n");
	}
}
EXPORT_SYMBOL(swc_device_unregister);

int swc_add_swp(struct device *swc, struct device *swp, int id)
{
	char name[32];
	int err;

	dev_info(swc, "registering %s\n", dev_name(swp));

	sprintf(name, "swp%u", id);

	err = sysfs_create_link(&swc->kobj, &swp->kobj, name);
	if (err)
		return err;

	get_device(swp);
	get_device(swc);

	dev_info(swc, "registered %s\n", dev_name(swp));

	return 0;
}
EXPORT_SYMBOL(swc_add_swp);

void swc_del_swp(struct device *swc, struct device *swp, int id)
{
	char name[32];

	sprintf(name, "swp%u", id);

	sysfs_remove_link(&swc->kobj, name);

	put_device(swc);
	put_device(swp);

	dev_info(swc, "unregistered %s\n", dev_name(swp));
}
EXPORT_SYMBOL(swc_del_swp);

static int __init swc_init(void)
{
	int err;

	err = class_register(&swc_class);
	if (err) {
		pr_err("swc: failed to create class\n");
		return err;
	}

	pr_info("swc: registered class\n");

	return 0;
}
subsys_initcall(swc_init);

static void __exit swc_exit(void)
{
	class_unregister(&swc_class);
	pr_info("swc: unregistered class\n");
}
module_exit(swc_exit);
