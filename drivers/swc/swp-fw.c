/*
 *  swp-fw.c - Firmware Backed Switch Port Driver
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
#include <linux/swp.h>
#include "swc-fw-util.h"

MODULE_AUTHOR("Dustin Byford");
MODULE_DESCRIPTION("Firmware Defined Switch Port Device Driver");
MODULE_LICENSE("GPL");

#define SWP_FW_MAX_LANES 32

struct swp_fw_data {
	struct swc_device *swc;
	struct swp_device swp;
};

static const struct platform_device_id swp_fw_ids[] = {
	{ "switch-complex-port", 0 },
	{ }
};
MODULE_DEVICE_TABLE(platform, swp_fw_ids);

static const struct of_device_id swp_fw_of_match[] = {
	{ .compatible = "switch-complex-port" },
	{ }
};
MODULE_DEVICE_TABLE(of, swp_fw_of_match);

static int swp_fw_probe(struct platform_device *pdev)
{
	struct swp_fw_data *data;
	u64 adr;
	int err;

	dev_info(&pdev->dev, "swp_fw_probe()\n");

	data = devm_kzalloc(&pdev->dev, sizeof *data, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	err = swc_fw_util_acpi_get_adr(ACPI_COMPANION(&pdev->dev), &adr);
	if (err)
		return -ENODEV;

	data->swp.id = (unsigned int)adr + 1;

	platform_set_drvdata(pdev, data);

	device_property_read_string(&pdev->dev, "label", &data->swp.label);
	data->swp.pluggable = swc_fw_util_get_ref_physical(&pdev->dev, "pluggable");
	if (PTR_ERR(data->swp.pluggable) == -ENODEV)
		return -EPROBE_DEFER;
	else if (IS_ERR(data->swp.pluggable))
		return -ENODEV;

	swp_device_register(&pdev->dev, &data->swp);

	return 0;
}

static int swp_fw_remove(struct platform_device *pdev)
{
	struct swp_fw_data *data = platform_get_drvdata(pdev);

	swp_device_unregister(&data->swp);

	dev_info(&pdev->dev, "removed\n");

	return 0;
}

static struct platform_driver swp_fw_driver = {
	.probe = swp_fw_probe,
	.remove = swp_fw_remove,
	.driver = {
		.name = "switch-complex-port",
		.of_match_table = swp_fw_of_match,
	},
	.id_table = swp_fw_ids,
};
module_platform_driver(swp_fw_driver);
