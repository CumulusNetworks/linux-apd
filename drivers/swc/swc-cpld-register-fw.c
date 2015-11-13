/*
 * Device Property Register Device Driver
 *
 * Copyright (C) 2014 Cumulus Networks, Inc.
 *
 * Author: Dustin Byford <dustin@cumulusnetworks.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/property.h>
#include <linux/swc/cpld.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include "swc-fw-util.h"

MODULE_AUTHOR("Dustin Byford <dustin@cumulusnetworks.com>");
MODULE_DESCRIPTION("Firmware Defined Switch Complex Register Access Device Driver");
MODULE_LICENSE("GPL v2");

#define SWC_CPLD_REGISTER_MAX_OFFSETS 8
struct swc_cpld_register_data {
	struct device *cpld;

	int num_offsets;
	u8 offsets[SWC_CPLD_REGISTER_MAX_OFFSETS];
	u8 valid_masks[SWC_CPLD_REGISTER_MAX_OFFSETS];
	u8 direction_masks[SWC_CPLD_REGISTER_MAX_OFFSETS];
	u8 readable_masks[SWC_CPLD_REGISTER_MAX_OFFSETS];
	u8 writable_masks[SWC_CPLD_REGISTER_MAX_OFFSETS];
	u8 volatile_masks[SWC_CPLD_REGISTER_MAX_OFFSETS];
	const char *names[SWC_CPLD_REGISTER_MAX_OFFSETS];
	struct device_attribute attrs[SWC_CPLD_REGISTER_MAX_OFFSETS];
};

static const struct platform_device_id swc_cpld_register_ids[] = {
	{ "swc-cpld-register", 0 },
	{ }
};
MODULE_DEVICE_TABLE(platform, swc_cpld_register_ids);

static ssize_t register_show(struct device *dev,
			     struct device_attribute *dattr,
			     char *buf)
{
	struct swc_cpld_register_data *data = dev_get_drvdata(dev);
	struct regmap *regmap;
	char property_name[128];
	int reg;

	swc_fw_util_sysfs_to_property(dattr->attr.name, property_name,
				      sizeof property_name);

	for (reg = 0; reg < data->num_offsets; reg++) {
		int val;
		int err;

		if (strcmp(property_name, data->names[reg]) != 0)
			continue;

		regmap = swc_cpld_get_regmap(data->cpld);
		if (!regmap)
			return -ENODEV;
		err = regmap_read(regmap, data->offsets[reg], &val);
		swc_cpld_put_regmap(data->cpld, regmap);
		if (err)
			return err;

		val &= data->valid_masks[reg];

		return sprintf(buf, "0x%x\n", val);
	}

	return -ENODEV;
}

DEVICE_ATTR(test, 0444, register_show, NULL);

int swc_cpld_register_get(struct device *dev, int reg, int *out)
{
	struct swc_cpld_register_data *data = dev_get_drvdata(dev);
	struct regmap *regmap;
	int val;
	int err;

	regmap = swc_cpld_get_regmap(data->cpld);
	if (!regmap)
		return -ENODEV;

	err = regmap_read(regmap, data->offsets[reg], &val);
	swc_cpld_put_regmap(data->cpld, regmap);
	if (err)
		return err;

	*out = val & data->valid_masks[reg];
	return 0;
}
EXPORT_SYMBOL(swc_cpld_register_get);

int swc_cpld_register_set(struct device *dev, int reg, int val)
{
	struct swc_cpld_register_data *data = dev_get_drvdata(dev);
	struct regmap *regmap;
	int err;

	if (data->writable_masks[reg] != 0xff) {
		dev_err(dev, "partially writable registers not implemented\n");
		return -EINVAL;
	}

	regmap = swc_cpld_get_regmap(data->cpld);
	if (!regmap)
		return -ENODEV;

	err = regmap_write(regmap, data->offsets[reg],
			   val & data->valid_masks[reg]);
	swc_cpld_put_regmap(data->cpld, regmap);
	if (err)
		return err;

	return 0;
}
EXPORT_SYMBOL(swc_cpld_register_set);

static int swc_cpld_register_probe(struct platform_device *pdev)
{
	struct swc_fw_util_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct swc_cpld_register_data *data;
	int num;
	int err;

	dev_info(&pdev->dev, "swc_cpld_register_probe()\n");

	if (!pdata) {
		dev_err(&pdev->dev, "missing platform data\n");
		return -ENODEV;
	}

	data = devm_kzalloc(&pdev->dev, sizeof *data, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	/* Required Properties */
        num = swc_cpld_get_property(&pdev->dev, "offsets", data->offsets, SWC_CPLD_REGISTER_MAX_OFFSETS);
	if (num < 0)
		return num;
	data->num_offsets = num;

        err = swc_cpld_get_property_n(&pdev->dev, "valid-masks", data->valid_masks, num);
	if (err)
		return err;

	/* Optional Properties */
	// assume readable, it's the common case
	memset(data->readable_masks, 0xff, sizeof data->readable_masks);
	if (device_property_present(&pdev->dev, "readable-masks")) {
	        err = swc_cpld_get_property_n(&pdev->dev, "readable-masks", data->readable_masks, num);
		if (err)
			return err;
	}

	if (device_property_present(&pdev->dev, "writable-masks")) {
	        err = swc_cpld_get_property_n(&pdev->dev, "writable-masks", data->writable_masks, num);
		if (err)
			return err;
	}

	// safety first, assume volatile
	memset(data->volatile_masks, 0xff, sizeof data->volatile_masks);
	if (device_property_present(&pdev->dev, "volatile-masks")) {
	        err = swc_cpld_get_property_n(&pdev->dev, "volatile-masks", data->volatile_masks, num);
		if (err)
			return err;
	}

	// XXX - reserved-zeros

	num = device_property_read_string_array(&pdev->dev, "names", NULL, 0);
	if (num == data->num_offsets)
		device_property_read_string_array(&pdev->dev, "names", data->names, num);
	else
		return -EINVAL;

	data->cpld = get_device(pdata->cpld);

	platform_set_drvdata(pdev, data);

	for (num = 0; num < data->num_offsets; num++) {
		struct device_attribute *dev_attr;
		char *sysfs_name;
		size_t sysfs_name_size;
		u8 props = 0;

		/*
		 * XXX - we can't really set these properties per bit.  But, it
		 * might be possible one day using a regmap field to break the
		 * register into fields.
		 *
		 * Fields would encode nicely with multiple offsets[] elements
		 * with the same register offset, but different valid-masks.
		 */
		if (data->valid_masks[num])
			props |= SWC_CPLD_READABLE;
		if (data->writable_masks[num])
			props |= SWC_CPLD_WRITEABLE;
		if (data->volatile_masks[num])
			props |= SWC_CPLD_VOLATILE;
		err = swc_cpld_set_regprops(data->cpld, data->offsets[num], props);
		if (err)
			goto fail_attrs;

		sysfs_name_size = strlen(data->names[num]) + 1;
		sysfs_name = devm_kzalloc(&pdev->dev, sysfs_name_size, GFP_KERNEL);
		if (!sysfs_name) {
			err = -ENOMEM;
			goto fail_attrs;
		}
		swc_fw_util_property_to_sysfs(data->names[num], sysfs_name, sysfs_name_size);
			
		dev_attr = &data->attrs[num];
		sysfs_attr_init(&dev_attr->attr);
		dev_attr->show = register_show;
		dev_attr->attr.name = sysfs_name;
		dev_attr->attr.mode = 0444;

		err = sysfs_create_file(&pdev->dev.kobj, &dev_attr->attr);
		if (err) {
			memset(dev_attr, 0, sizeof *dev_attr);
			goto fail_attrs;
		}
	}

	dev_err(&pdev->dev, "added %d registers\n", data->num_offsets);

	return 0;

fail_attrs:
	for (num = 0; num < data->num_offsets; num++) {
		if (data->attrs[num].attr.name) 
			sysfs_remove_file(&pdev->dev.kobj, &data->attrs[num].attr);
		else
			break;
	}

	put_device(pdata->cpld);

	return err;
}

static int swc_cpld_register_remove(struct platform_device *pdev)
{
	struct swc_cpld_register_data *data = platform_get_drvdata(pdev);
	int num;

	for (num = 0; num < data->num_offsets; num++) {
		sysfs_remove_file(&pdev->dev.kobj, &data->attrs[num].attr);
	}

	put_device(data->cpld);

	dev_info(&pdev->dev, "removed\n");

	return 0;
}

static struct platform_driver swc_cpld_register_driver = {
	.driver = {
		.name		= "swc-cpld-register",
		.owner		= THIS_MODULE,
	},
	.probe = swc_cpld_register_probe,
	.remove = swc_cpld_register_remove,
	.id_table = swc_cpld_register_ids,
};
module_platform_driver(swc_cpld_register_driver);
