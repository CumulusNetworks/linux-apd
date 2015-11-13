/*
 *  sff-fw.c - Firmware Backed Switch Port Driver
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
#include <linux/acpi.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include "swc-fw-util.h"

MODULE_AUTHOR("Dustin Byford");
MODULE_DESCRIPTION("Firmware Defined Small Form Factor Pluggable Transceiver Driver");
MODULE_LICENSE("GPL");

struct sff_fw_data {
	struct device *twi;
	struct gpio_desc *present;
	struct gpio_desc *tx_fault;
	struct gpio_desc *tx_enable;
	struct gpio_desc *rx_los;
	struct gpio_desc *low_power;
	struct gpio_desc *reset;
	struct gpio_desc *module_select;

	int num_attrs;
	struct attribute *sff_fw_attrs[16];
	struct attribute_group sff_fw_attr_group;
	struct attribute_group *sff_fw_groups[2];
};

static const struct platform_device_id sff_fw_ids[] = {
	{ "sff-sfpp-fw", 0 },
	{ "sff-qsfpp-fw", 0 },
	{ }
};
MODULE_DEVICE_TABLE(platform, sff_fw_ids);

static const struct of_device_id sff_fw_of_match[] = {
	{ .compatible = "sff-sfpp-fw", },
	{ .compatible = "sff-qsfpp-fw", },
	{ },
};
MODULE_DEVICE_TABLE(of, sff_fw_of_match);


static struct gpio_desc *get_gpiod(struct sff_fw_data *data, const char *name)
{
	if (strcmp(name, "present") == 0)
		return data->present;
	else if (strcmp(name, "tx-fault") == 0)
		return data->tx_fault;
	else if (strcmp(name, "tx-enable") == 0)
		return data->tx_enable;
	else if (strcmp(name, "rx-los") == 0)
		return data->rx_los;
	else if (strcmp(name, "low-power") == 0)
		return data->low_power;
	else if (strcmp(name, "reset") == 0)
		return data->reset;

	return NULL;
}

static ssize_t set_gpio(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct sff_fw_data *data = dev_get_drvdata(dev);
	struct gpio_desc *gpiod;
	unsigned long val;

	gpiod = get_gpiod(data, attr->attr.name);
	if (!gpiod) {
		dev_err(dev, "failed to get gpiod for %s\n", attr->attr.name);
		return -EINVAL;
	}

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	gpiod_set_value(gpiod, !!val);

	return count;
}

static ssize_t show_gpio(struct device *dev,
			 struct device_attribute *attr, char *buf)
{
	struct sff_fw_data *data = dev_get_drvdata(dev);
	struct gpio_desc *gpiod;
	unsigned long val;

	gpiod = get_gpiod(data, attr->attr.name);
	if (!gpiod) {
		dev_err(dev, "failed to get gpiod for %s\n", attr->attr.name);
		return -EINVAL;
	}

	val = gpiod_get_value(gpiod);

	return sprintf(buf, "%d\n", !!val);
}

static DEVICE_ATTR(present, S_IRUGO | S_IWUSR, show_gpio, set_gpio);
static DEVICE_ATTR(tx_fault, S_IRUGO | S_IWUSR, show_gpio, set_gpio);
static DEVICE_ATTR(tx_enable, S_IRUGO | S_IWUSR, show_gpio, set_gpio);
static DEVICE_ATTR(rx_los, S_IRUGO | S_IWUSR, show_gpio, set_gpio);
static DEVICE_ATTR(low_power, S_IRUGO | S_IWUSR, show_gpio, set_gpio);
static DEVICE_ATTR(reset, S_IRUGO | S_IWUSR, show_gpio, set_gpio);

static int sff_fw_get_gpio(struct device *dev, const char *name,
			   struct gpio_desc **dst)
{
	struct gpio_desc *gpiod;

	gpiod = devm_gpiod_get(dev, name, 0);
	if (IS_ERR(gpiod)) {
		dev_err(dev, "failed to get gpiod for %s\n", name);
		return -ENODEV;
	}

	*dst = gpiod;

	return 0;
}

static void sff_fw_add_attr(struct sff_fw_data *data,
			     struct attribute *attr)
{
	if (data->num_attrs >= ARRAY_SIZE(data->sff_fw_attrs)) {
		pr_err("attr array too small\n");
		return;
	}

	data->sff_fw_attrs[data->num_attrs++] = attr;
}

static int sff_fw_probe(struct platform_device *pdev)
{
	struct sff_fw_data *data;
	int err;

	dev_info(&pdev->dev, "sff_fw_probe()\n");

	data = devm_kzalloc(&pdev->dev, sizeof *data, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	platform_set_drvdata(pdev, data);

	data->twi = swc_fw_util_get_ref_physical(&pdev->dev, "serial-interface");
	if (PTR_ERR(data->twi) == -ENODEV)
		return -EPROBE_DEFER;
	else if (IS_ERR(data->twi))
		return -ENODEV;

	if (!get_device(data->twi))
		return -ENODEV;

	err = sysfs_create_link(&pdev->dev.kobj, &data->twi->kobj,
				"serial-interface");
	if (err)
		goto fail;

	data->num_attrs = 0;

	err = sff_fw_get_gpio(&pdev->dev, "present", &data->present);
	if (!err)
		sff_fw_add_attr(data, &dev_attr_present.attr);

	err = sff_fw_get_gpio(&pdev->dev, "tx-fault", &data->tx_fault);
	if (!err)
		sff_fw_add_attr(data, &dev_attr_tx_fault.attr);

	err = sff_fw_get_gpio(&pdev->dev, "tx-enable", &data->tx_enable);
	if (!err)
		sff_fw_add_attr(data, &dev_attr_tx_enable.attr);

	err = sff_fw_get_gpio(&pdev->dev, "rx-los", &data->rx_los);
	if (!err)
		sff_fw_add_attr(data, &dev_attr_rx_los.attr);

	err = sff_fw_get_gpio(&pdev->dev, "low-power", &data->low_power);
	if (!err)
		sff_fw_add_attr(data, &dev_attr_low_power.attr);

	err = sff_fw_get_gpio(&pdev->dev, "reset", &data->reset);
	if (!err)
		sff_fw_add_attr(data, &dev_attr_reset.attr);

	if (data->num_attrs) {
		data->sff_fw_attr_group.attrs = data->sff_fw_attrs;

		err = sysfs_create_group(&pdev->dev.kobj, &data->sff_fw_attr_group);
		if (err)
			goto fail;
	}

	dev_info(&pdev->dev, "added sff with %d attrs\n", data->num_attrs);

	return 0;

fail:
	put_device(data->twi);
	return err;
}

static int sff_fw_remove(struct platform_device *pdev)
{
	struct sff_fw_data *data = platform_get_drvdata(pdev);

	sysfs_remove_link(&pdev->dev.kobj, "serial-interface");

	if (data->num_attrs)
		sysfs_remove_group(&pdev->dev.kobj,
				   &data->sff_fw_attr_group);

	put_device(data->twi);

	dev_info(&pdev->dev, "removed\n");

	return 0;
}

static struct platform_driver sff_fw_driver = {
	.probe = sff_fw_probe,
	.remove = sff_fw_remove,
	.driver = {
		.name = "sff-sfpp-fw",
		.of_match_table = sff_fw_of_match,
	},
	.id_table = sff_fw_ids,
};
module_platform_driver(sff_fw_driver);
