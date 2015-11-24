/*
 * sff-twi.c - handle the serial interface to SFF SFP+ and QSFP+ modules.
 *
 * Copyright (C) 2015 Cumulus networks Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Freeoftware Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/mod_devicetable.h>
#include <linux/i2c.h>
#include <linux/types.h>
#include <linux/memory.h>
#include "sff-twi-dummy.h"

static const struct i2c_device_id sff_twi_ids[] = {
	{ "sff-twi", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sff_twi_ids);

static const struct of_device_id sff_twi_of_match[] = {
	{ .compatible = "sff-twi" },
	{ }
};
MODULE_DEVICE_TABLE(of, sff_twi_of_match);

struct sff_twi_data {
	struct bin_attribute sysfs_mem;
};

static ssize_t sff_twi_sysfs_mem_read(struct file *filp, struct kobject *kobj,
				      struct bin_attribute *attr,
				      char *buf, loff_t off, size_t count)
{
	if (off + count > sizeof(sff_twi_dummy_data))
		return -EINVAL;

	memcpy(buf, &sff_twi_dummy_data[off], count);
	return count;
}

static ssize_t sff_twi_sysfs_mem_write(struct file *filp, struct kobject *kobj,
				       struct bin_attribute *attr,
				       char *buf, loff_t off, size_t count)
{
	return count;
}

static int sff_twi_probe(struct i2c_client *client,
                         const struct i2c_device_id *id)
{
	struct sff_twi_data *data;
	int err;

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	sysfs_bin_attr_init(&data->sysfs_mem);
	data->sysfs_mem.attr.name = "mem";
	data->sysfs_mem.attr.mode = S_IWUSR;
	data->sysfs_mem.read = sff_twi_sysfs_mem_read;
	data->sysfs_mem.write = sff_twi_sysfs_mem_write;
	data->sysfs_mem.size = sizeof(sff_twi_dummy_data);

	err = sysfs_create_bin_file(&client->dev.kobj, &data->sysfs_mem);
	if (err)
		return -ENODEV;

	i2c_set_clientdata(client, data);

	dev_err(&client->dev, "loaded dummy driver\n");
	return 0;
}

static int sff_twi_remove(struct i2c_client *client)
{
	struct sff_twi_data *data;

	data = i2c_get_clientdata(client);
	sysfs_remove_bin_file(&client->dev.kobj, &data->sysfs_mem);
	i2c_unregister_device(client);
	return 0;
}

static struct i2c_driver sff_twi_driver = {
	.driver = {
		.name	= "sff-twi",
		.owner  = THIS_MODULE,
		.of_match_table = sff_twi_of_match,
	},
	.probe		= sff_twi_probe,
	.remove		= sff_twi_remove,
	.id_table	= sff_twi_ids,
};
module_i2c_driver(sff_twi_driver);

MODULE_AUTHOR("Dustin Byford <dustin@cumulusnetworks.com>");
MODULE_DESCRIPTION("SFF pluggable serial interface driver");
MODULE_LICENSE("GPL");
