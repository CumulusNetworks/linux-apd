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
#include <linux/swc/register.h>
#include <linux/regmap.h>
#include "swc-fw-util.h"

MODULE_AUTHOR("Dustin Byford");
MODULE_DESCRIPTION("Firmware Defined Small Form Factor Pluggable Transceiver Driver");
MODULE_LICENSE("GPL");

struct sff_fw_data {
  struct device *dev;
	struct device *twi;
	struct device *qsfp_present;
	int    qsfp_present_offset;
	struct device *qsfp_tx_fault;
	int    qsfp_tx_fault_offset;
	struct device *qsfp_tx_enable;
	int    qsfp_tx_enable_offset;
	struct device *qsfp_rx_los;
	int    qsfp_rx_los_offset;
	struct device *qsfp_reset;
	int    qsfp_reset_offset;
	struct device *qsfp_module_select;
	int    qsfp_module_select_offset;
	
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
	else if (strcmp(name, "tx_fault") == 0)
		return data->tx_fault;
	else if (strcmp(name, "tx_enable") == 0)
		return data->tx_enable;
	else if (strcmp(name, "rx_los") == 0)
		return data->rx_los;
	else if (strcmp(name, "low_power") == 0)
		return data->low_power;
	else if (strcmp(name, "reset") == 0)
		return data->reset;

	return NULL;
}
static ssize_t show_qsfp_present(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sff_fw_data *data = dev_get_drvdata(dev);
	struct swc_cpld_register_data *swc_data = dev_get_drvdata(data->qsfp_present);
	int val;
	int err;
	int mask=0;
	int check, shift=0;

	err = swc_cpld_register_get(data->qsfp_present, data->qsfp_present_offset, &val);
	if (err)
	{
		dev_err(dev, "failed to get fan present\n");
		return -EFAULT;
	}	
	
	err = swc_cpld_register_mask_get(data->qsfp_present, data->qsfp_present_offset, &mask);
	if (err)
	{
		dev_err(dev, "failed to get fan present\n");
		return -EFAULT;
	}
	
	for(shift=0;shift<8;shift++)
	{
	    check=mask&(1<<shift);
	    if(check)
	        break;
	}
	val=val>>shift;
	if(val==0)
	    val=1;
	else
	    val=0;
	   
	return sprintf(buf, "%d\n", val);
}
static DEVICE_ATTR(present, S_IRUGO, show_qsfp_present, NULL);

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
    data->qsfp_present = swc_fw_util_get_ref_physical(&pdev->dev, "present");
    if (PTR_ERR(data->qsfp_present) == -ENODEV) {	  
        return -EPROBE_DEFER;
    } else if (IS_ERR(data->qsfp_present)) {
        data->qsfp_present = NULL;
    } else {
        struct acpi_reference_args ref;
        u32 range[2];

        err = acpi_node_get_property_reference(
        acpi_fwnode_handle(ACPI_COMPANION(&pdev->dev)),
            "present", 0, &ref);
        if (err) {
            dev_err(&pdev->dev, "failed to get present device\n");
            return -EINVAL;
        }
        if (ref.nargs > 1) {
            dev_err(&pdev->dev, "too many args to 'present'\n");
            return -EINVAL;
        }    

        if (ref.nargs == 0)
            data->qsfp_present_offset = 0;
        else
            data->qsfp_present_offset = ref.args[0];
		
    }
	
    data->dev = &pdev->dev;
    platform_set_drvdata(pdev, data);
    data->qsfp_present = get_device(data->qsfp_present);
    platform_set_drvdata(pdev, data);
	
    if(data->qsfp_present)
        sff_fw_add_attr(data, &dev_attr_present.attr);

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
    put_device(data->qsfp_present);
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
