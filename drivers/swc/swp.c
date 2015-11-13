/*
 *  swp.c - Switch Complex Port Class Driver
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
#include <linux/export.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/swp.h>

MODULE_AUTHOR("Dustin Byford");
MODULE_DESCRIPTION("Network Switch Complex Port Class Driver");
MODULE_LICENSE("GPL");

static struct class swp_class = {
	.name = "swp",
	.owner = THIS_MODULE,
};

static ssize_t swp_label_show(struct device *dev,
			      struct device_attribute *dattr,
			      char *buf)
{
	struct swp_device *swp = dev_get_drvdata(dev);
	return sprintf(buf, "%s\n", swp->label);
}

DEVICE_ATTR(label, 0444, swp_label_show, NULL);

int swp_device_register(struct device *dev, struct swp_device *swp)
{
	struct device *pluggable;
	int err;

	swp->dev = device_create(&swp_class, dev, MKDEV(0, 0), swp, "swp%d",
				 swp->id);
	if (IS_ERR(swp->dev)) {
		err = PTR_ERR(swp->dev);
		goto fail_create;
	}

	err = sysfs_create_file(&swp->dev->kobj, &dev_attr_label.attr);
	if (err)
		goto fail_label;

	pluggable = get_device(swp->pluggable);
	if (pluggable) {
		err = sysfs_create_link(&swp->dev->kobj, &pluggable->kobj,
					"pluggable");
		if (err)
			goto fail_pluggable;
	}

	dev_info(swp->dev, "registered swp\n");

	return 0;

fail_pluggable:
	put_device(pluggable);
	sysfs_remove_file(&swp->dev->kobj, &dev_attr_label.attr);
fail_label:
	device_unregister(swp->dev);
fail_create:
	return err;
}
EXPORT_SYMBOL(swp_device_register);

void swp_device_unregister(struct swp_device *swp)
{
        int id;

        if (likely(sscanf(dev_name(swp->dev), "swp%d", &id) == 1)) {
		if (swp->pluggable) {
			sysfs_remove_link(&swp->dev->kobj, "pluggable");
			put_device(swp->pluggable);
		}
		sysfs_remove_file(&swp->dev->kobj, &dev_attr_label.attr);
                device_unregister(swp->dev);
        } else {
                dev_dbg(swp->dev, "failed to unregister swp\n");
	}
}
EXPORT_SYMBOL(swp_device_unregister);

static int __init swp_init(void)
{
	int err;

	err = class_register(&swp_class);
	if (err) {
		pr_err("swp: failed to create class\n");
		return err;
	}

	pr_info("swp: registered class\n");

	return 0;
}
subsys_initcall(swp_init);

static void __exit swp_exit(void)
{
	class_unregister(&swp_class);
	pr_info("swp: unregistered class\n");
}
module_exit(swp_exit);
