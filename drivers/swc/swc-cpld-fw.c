/*
 * Device Property Multifunction Device Driver
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

#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/property.h>
#include <linux/mfd/core.h>
#include <linux/swc/cpld.h>
#include <linux/regmap.h>
#include "swc-fw-util.h"

MODULE_AUTHOR("Dustin Byford <dustin@cumulusnetworks.com>");
MODULE_DESCRIPTION("Firmware Defined Switch Complex Multifunction Device Driver");
MODULE_LICENSE("GPL v2");

#define SWC_CPLD_REG_MAX 0xff
struct swc_cpld_data {
	struct regmap *regmap;
	struct mutex regmap_mutex;
	unsigned int max_reg;
	u8 register_props[SWC_CPLD_REG_MAX];
};

static const struct i2c_device_id swc_cpld_ids[] = {
	{ "swc-cpld", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, swc_cpld_ids);

static const struct of_device_id swc_cpld_of_match[] = {
	{ .compatible = "swc-cpld" },
	{ }
};
MODULE_DEVICE_TABLE(of, swc_cpld_of_match);

static bool swc_cpld_writeable_reg(struct device *dev, unsigned int reg)
{
	struct swc_cpld_data *data = dev_get_drvdata(dev);
	if (!data) {
		dev_err(dev, "no drvdata in writeable\n");
		return false;
	}
	if (reg > SWC_CPLD_REG_MAX) {
		dev_err(dev, "reg too large: 0x%x\n", reg);
		return false;
	}
	return (data->register_props[reg] & SWC_CPLD_WRITEABLE) != 0;
}

static bool swc_cpld_readable_reg(struct device *dev, unsigned int reg)
{
	struct swc_cpld_data *data = dev_get_drvdata(dev);
	if (!data) {
		dev_err(dev, "no drvdata in readable\n");
		return false;
	}
	if (reg > SWC_CPLD_REG_MAX) {
		dev_err(dev, "reg too large: 0x%x\n", reg);
		return false;
	}
	return (data->register_props[reg] & SWC_CPLD_READABLE) != 0;
}

static bool swc_cpld_volatile_reg(struct device *dev, unsigned int reg)
{
	struct swc_cpld_data *data = dev_get_drvdata(dev);
	if (!data) {
		dev_err(dev, "no drvdata in volatile\n");
		return false;
	}
	if (reg > SWC_CPLD_REG_MAX) {
		dev_err(dev, "reg too large: 0x%x\n", reg);
		return false;
	}
	return (data->register_props[reg] & SWC_CPLD_VOLATILE) != 0;
}


int swc_cpld_set_regprops(struct device *dev, unsigned int reg, u8 props)
{
	struct swc_cpld_data *data = dev_get_drvdata(dev);
	struct regmap_config config = { 0 };
	struct regmap *regmap;
	int err = 0;

	if (!data) {
		dev_err(dev, "no swc_cpld_data\n");
		return -ENODEV;
	}

	if (reg > SWC_CPLD_REG_MAX) {
		dev_err(dev, "reg too large: %x\n", reg);
		return -EINVAL;
	}

	dev_dbg(dev, "setting register 0x%x%s%s%s\n", reg,
	        (props & SWC_CPLD_READABLE) ? " readable" : "",
	        (props & SWC_CPLD_WRITEABLE) ? " writable" : "",
	        (props & SWC_CPLD_VOLATILE) ? " volatile" : "");

	mutex_lock(&data->regmap_mutex);

	if (props == data->register_props[reg] && reg < data->max_reg)
		goto done;

	if (reg > data->max_reg)
		data->max_reg = reg;

	data->register_props[reg] = props;
	config.max_register = data->max_reg;
	config.writeable_reg = swc_cpld_writeable_reg;
	config.readable_reg = swc_cpld_readable_reg;
	config.volatile_reg = swc_cpld_volatile_reg;
	config.cache_type = REGCACHE_FLAT;

	regmap = dev_get_regmap(dev, NULL);
	if (regmap) {
		regmap_reinit_cache(dev_get_regmap(dev, NULL), &config);
	} else {
		dev_err(dev, "failed to get regmap\n");
		err = -ENODEV;
	}

done:
	mutex_unlock(&data->regmap_mutex);

	return err;
}
EXPORT_SYMBOL(swc_cpld_set_regprops);

int swc_cpld_get_property(struct device *dev, const char *property, u8 *arr,
			  size_t arr_size)

{
	int num;
	int err;

	num = device_property_read_u8_array(dev, property, NULL, 0);
	if (num < 1) {
		dev_err(dev, "failed to count device property: %s\n", property);
		return num;
	}
	if (num > arr_size) {
		dev_err(dev, "too many arguments to %s: %d\n", property, num);
		return -EINVAL;
	}
	err = device_property_read_u8_array(dev, property, arr, num);
	if (err) {
		dev_err(dev, "failed to read device property: %s, err: %d\n",
		        property, err);
		return err;
	}
	
	return num;
}
EXPORT_SYMBOL(swc_cpld_get_property);

int swc_cpld_get_property_n(struct device *dev, const char *property, u8 *arr,
			     size_t num)
{
	int err;

	err = device_property_read_u8_array(dev, property, arr, num);
	if (err) {
		dev_err(dev, "failed to read device property: %s size: %zu\n", property, num);
		return err;
	}

	return 0;
}
EXPORT_SYMBOL(swc_cpld_get_property_n);

struct regmap *swc_cpld_get_regmap(struct device *dev)
{
	struct swc_cpld_data *data = dev_get_drvdata(dev);
	struct regmap *map;

	mutex_lock(&data->regmap_mutex);

	map = dev_get_regmap(dev, NULL);
	if (!map) {
		dev_err(dev, "failed to get regmap\n");
		mutex_unlock(&data->regmap_mutex);
	}

	return map;
}
EXPORT_SYMBOL(swc_cpld_get_regmap);

void swc_cpld_put_regmap(struct device *dev, struct regmap *regmap)
{
	struct swc_cpld_data *data = dev_get_drvdata(dev);
	if (data->regmap != regmap)
		dev_err(dev, "put foreign regmap\n");
	mutex_unlock(&data->regmap_mutex);
}
EXPORT_SYMBOL(swc_cpld_put_regmap);

static int swc_cpld_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct regmap_config regmap_config = { 0 };
	int child_count;
	struct swc_cpld_data *data;
	struct fwnode_handle *child;
	const char *str;
	u32 val;
	int err;

	dev_info(dev, "swc_cpld_probe()\n");

	data = devm_kzalloc(dev, sizeof(struct swc_cpld_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	err = device_property_read_string(dev, "protocol", &str);
	if (err) {
		dev_err(dev, "failed to read device property: protocol\n");
		return -EINVAL;
	}
	if (strcmp(str, "register") != 0) {
		dev_err(dev, "unsupported protocol for i2c device: %s\n", str);
		return -EINVAL;
	}

	err = device_property_read_u32(dev, "register-bits", &val);
	if (err) {
		dev_err(dev, "failed to read device property: register-bits\n");
		return -EINVAL;
	}
	if (val != 8) {
		dev_err(dev, "unsupported register-bits: %u\n", val);
		return -EINVAL;
	}

	regmap_config.reg_bits = 8;
	regmap_config.val_bits = 8;

	mutex_init(&data->regmap_mutex);

	dev_set_drvdata(dev, data);

	data->regmap = devm_regmap_init_i2c(client, &regmap_config);
	if (IS_ERR(data->regmap)) {
		dev_err(dev, "failed to init regmap_i2c\n");
		return -ENODEV;
	}

	child_count = device_get_child_node_count(dev);
	if (child_count) {
		struct swc_fw_util_platform_data platform_data = { 0 };
		int cell_count = 0;
		struct mfd_cell *cells;
		u64 adr;

		platform_data.cpld = dev;

		cells = kzalloc(child_count * sizeof *cells, GFP_KERNEL);

		device_for_each_child_node(dev, child) {
			struct mfd_cell *cell = &cells[cell_count];
			acpi_status status;

			err = fwnode_property_read_string(child, "compatible", &str);
			if (err)
				goto skip;

			status = acpi_evaluate_integer(to_acpi_device_node(child),
						       "_ADR", NULL, &adr);
			if (ACPI_FAILURE(status)) {
				dev_err(dev, "failed to get ACPI address for: %s, err=%d\n",
					dev_name(&to_acpi_device_node(child)->dev), status);
				goto skip;
			}

			cell->name = str;
			cell->name = str;
			cell->acpi_lookup_adr = true;
			cell->acpi_adr = adr;
			cell->platform_data = &platform_data;
			cell->pdata_size = sizeof platform_data;

			dev_info(dev, "hello %s at 0x%llx\n", str, adr);

			cell_count++;
skip:
			fwnode_handle_put(child);
		}

		dev_err(dev, "adding %d cells\n", cell_count);
		err = mfd_add_devices(dev, PLATFORM_DEVID_AUTO, cells, cell_count,
				      NULL, 0, NULL);
		if (err) {
			dev_err(dev, "failed to add cells\n");
		}

		kfree(cells);
	}

	return 0;
}

static int swc_cpld_remove(struct i2c_client *client)
{
	//struct swc_cpld_data *data = i2c_get_clientdata(client);
	mfd_remove_devices(&client->dev);
	dev_info(&client->dev, "removed\n");
	return 0;
}

static struct i2c_driver swc_cpld_driver = {
	.driver = {
		.name = "swc-cpld",
		.owner = THIS_MODULE,
		.of_match_table = swc_cpld_of_match,
	},
	.probe = swc_cpld_probe,
	.remove = swc_cpld_remove,
	.id_table = swc_cpld_ids,
};
module_i2c_driver(swc_cpld_driver);
