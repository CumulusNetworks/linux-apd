/*
 * Device Property I2C Mux Device Driver
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
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/acpi.h>
#include <linux/property.h>
#include <linux/swc/cpld.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c-mux.h>
#include "swc-fw-util.h"

MODULE_AUTHOR("Dustin Byford <dustin@cumulusnetworks.com>");
MODULE_DESCRIPTION("Firmware Defined Switch Complex I2C Multiplexer Device Driver");
MODULE_LICENSE("GPL v2");

#define SWC_CPLD_I2C_MUX_MAX_CHANNELS 256
struct swc_cpld_i2c_mux_data {
	struct i2c_adapter *parent_adap;
	struct i2c_adapter *virt_adaps[SWC_CPLD_I2C_MUX_MAX_CHANNELS];
	struct device *cpld;

	u8 offset;
	u8 deselect_value;
	bool deselect_on_exit;
	u8 last_chan;
};

static const struct platform_device_id swc_cpld_i2c_mux_ids[] = {
	{ "swc-cpld-i2c-mux", 0 },
	{ }
};
MODULE_DEVICE_TABLE(platform, swc_cpld_i2c_mux_ids);

/*
 *  Write to mux register. We can't use the cpld's write function because we
 *  can't control its i2c locking.
 *
 *  XXX - we're assuming the mfd host is an i2c device to do this.  This should
 *  get abstracted away by the swc-cpld device using regmap!
 */
static int swc_cpld_i2c_mux_write(struct device *dev, u8 reg, u8 val)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_adapter *adapter = client->adapter;
	int err;

	if (adapter->algo->master_xfer) {
		struct i2c_msg msg = { 0 };
		char buf[2];

		msg.addr = client->addr;
		msg.flags = 0;
		msg.len = 2;
		buf[0] = reg;
		buf[1] = val;
		msg.buf = buf;
		err = adapter->algo->master_xfer(adapter, &msg, 1);
	} else {
		union i2c_smbus_data data;
		data.byte = val;
		err = adapter->algo->smbus_xfer(adapter, client->addr,
						client->flags,
						I2C_SMBUS_WRITE,
						reg, I2C_SMBUS_BYTE_DATA, &data);
	}

	return err;
}

static int swc_cpld_i2c_mux_select_chan(struct i2c_adapter *adap,
					void *mux, u32 chan)
{
	struct swc_cpld_i2c_mux_data *data = mux;
	int err = 0;
	u8 reg, val;

	if (!data) {
		pr_err("no mux data\n");
		return -ENODEV;
	}

	if (!data->cpld) {
		pr_err("swc-cpld-i2c-mux: no cpld data\n");
		return -ENODEV;
	}

	reg = data->offset;
	val = (u8)chan;

	/* Only select the channel if its different from the last channel */
	if (data->last_chan != val) {
		data->last_chan = val;
		err = swc_cpld_i2c_mux_write(data->cpld, reg, val);
	}

	if (err)
		data->last_chan = data->deselect_value;

	return err;
}

static int swc_cpld_i2c_mux_deselect(struct i2c_adapter *adap,
				     void *mux, u32 chan)
{
	struct swc_cpld_i2c_mux_data *data = mux;
	u8 reg, val;

	if (!data) {
		pr_err("no mux data\n");
		return -ENODEV;
	}

	if (!data->cpld) {
		pr_err("no cpld data\n");
		return -ENODEV;
	}

	reg = data->offset;
	val = data->deselect_value;

	data->last_chan = val;
	return swc_cpld_i2c_mux_write(data->cpld, reg, val);
}

static int swc_cpld_i2c_mux_probe(struct platform_device *pdev)
{
	struct swc_fw_util_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct swc_cpld_i2c_mux_data *data;
	struct acpi_reference_args ref;
	struct fwnode_handle *child;
	const char *str;
	u8 val;
	int count;
	int err;

	dev_info(&pdev->dev, "swc_cpld_i2c_mux_probe()\n");

	if (!pdata) {
		dev_err(&pdev->dev, "missing platform data\n");
		return -ENODEV;
	}

	data = devm_kzalloc(&pdev->dev, sizeof *data, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	err = device_property_read_u8(&pdev->dev, "offset", &data->offset);
	if (err) {
		dev_err(&pdev->dev, "failed to read device property: offset\n");
		return err;
	}

	err = device_property_read_string(&pdev->dev, "encoding", &str);
	if (err) {
		dev_err(&pdev->dev, "failed to read device property: encoding\n");
		return err;
	}
	if (strcmp(str, "index") != 0) {
		dev_err(&pdev->dev, "unsupported register encoding: %s\n", str);
		return err;
	}

	err = acpi_node_get_property_reference(
		acpi_fwnode_handle(ACPI_COMPANION(&pdev->dev)),
				   "i2c-parent", 0, &ref);
	if (err) {
	        dev_err(&pdev->dev, "failed to read device property: i2c-parent\n");
	        return err;
	}
	data->parent_adap = acpi_find_i2c_adapter(ref.adev);
	data->parent_adap = i2c_get_adapter(data->parent_adap->nr);
	if (!data->parent_adap) {
	        dev_err(&pdev->dev, "failed to find i2c-parent adapter\n");
	        return -EPROBE_DEFER;
	}

	dev_info(&pdev->dev, "i2c parent adapter: %s\n",
		 dev_name(&data->parent_adap->dev));

	/* Optional Properties */
	err = device_property_read_u8(&pdev->dev, "deselect-value", &val);
	if (err) {
		data->last_chan = 0;
	} else {
		data->last_chan = val;
		data->deselect_value = val;
	}

	// XXX - configure based on deselect-on-exit property
	data->deselect_on_exit = false;

	data->cpld = get_device(pdata->cpld);
	if (!data->cpld) {
		dev_err(&pdev->dev, "failed to get parent cpld\n");
		err = -ENODEV;
		goto fail;
	}
	dev_info(&pdev->dev, "parent cpld is %s\n", dev_name(data->cpld));

	platform_set_drvdata(pdev, data);

	swc_cpld_i2c_mux_deselect(NULL, data, 0);

	dev_info(&pdev->dev, "parent cpld after deselect %p\n", data->cpld);

	count = 0;
	device_for_each_child_node(&pdev->dev, child) {
		struct i2c_adapter *virt_adap;
		acpi_status status;
		u32 force_nr = 0;
		unsigned int class = 0;
		u64 adr;

		status = acpi_evaluate_integer(to_acpi_device_node(child)->handle,
					       "_ADR", NULL, &adr);
		if (ACPI_FAILURE(status)) {
			dev_err(&pdev->dev, "failed to get ACPI address for: %s, err=%d\n",
				dev_name(&to_acpi_device_node(child)->dev), status);
			goto skip;
		}

		dev_info(&pdev->dev, "configuring mux channel: 0x%llx\n", adr);

		virt_adap = 
			i2c_add_mux_adapter(data->parent_adap, &pdev->dev, data,
					    force_nr, adr, class,
					    swc_cpld_i2c_mux_select_chan, 
					    data->deselect_on_exit ?
					    	swc_cpld_i2c_mux_deselect : NULL);
		if (!virt_adap) {
			dev_err(&pdev->dev, "failed to register i2c bus for"
					    "channel: 0x%llx\n", adr);

			goto skip;
		}
		dev_info(&pdev->dev, "added mux with data: %p cpld: %p\n", data, data->cpld);

		data->virt_adaps[count++] = virt_adap;

skip:
		fwnode_handle_put(child);
	}

	swc_cpld_set_regprops(data->cpld, data->offset, SWC_CPLD_READABLE|SWC_CPLD_WRITEABLE);
	
	dev_err(&pdev->dev, "registered %d muxes\n", count);

	return 0;

fail:
	if (data->parent_adap)
		i2c_put_adapter(data->parent_adap);
	if (data->cpld)
		put_device(pdata->cpld);
	return err;
}

static int swc_cpld_i2c_mux_remove(struct platform_device *pdev)
{
	struct swc_cpld_i2c_mux_data *data = platform_get_drvdata(pdev);
	int count;
	int i;

	count = 0;
	for (i = 0; i < SWC_CPLD_I2C_MUX_MAX_CHANNELS; i++) {
		if (data->virt_adaps[i]) {
			i2c_del_mux_adapter(data->virt_adaps[i]);
			data->virt_adaps[i] = NULL;
			count++;
		}
	}

	i2c_put_adapter(data->parent_adap);
	put_device(data->cpld);

	dev_info(&pdev->dev, "removed\n");

	return 0;
}

static struct platform_driver swc_cpld_i2c_mux_driver = {
	.driver = {
		.name		= "swc-cpld-i2c-mux",
		.owner		= THIS_MODULE,
	},
	.probe = swc_cpld_i2c_mux_probe,
	.remove = swc_cpld_i2c_mux_remove,
	.id_table = swc_cpld_i2c_mux_ids,
};
module_platform_driver(swc_cpld_i2c_mux_driver);
