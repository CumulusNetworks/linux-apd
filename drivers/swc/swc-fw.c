/*
 *  swc-fw.c - Firmware Backed Switch Complex Driver
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
#include <linux/swc.h>
#include <linux/swp.h>
#include "swc-fw-util.h"

MODULE_AUTHOR("Dustin Byford");
MODULE_DESCRIPTION("Firmware Defined Switch Complex Device Driver");
MODULE_LICENSE("GPL");

struct swc_fw_data {
	struct swc_device swc;
};

static const struct platform_device_id swc_fw_ids[] = {
	{ "switch-complex", 0 },
	{ }
};
MODULE_DEVICE_TABLE(platform, swc_fw_ids);

static const struct of_device_id swc_fw_of_match[] = {
	{ .compatible = "switch-complex" },
	{ }
};
MODULE_DEVICE_TABLE(of, swc_fw_of_match);

static int swc_fw_probe(struct platform_device *pdev)
{
	struct swc_fw_data *data;

	if (!try_module_get(swc_class.owner)) {
		dev_err(&pdev->dev, "swc unavailable\n");
		return -ENODEV;
	}

	data = devm_kzalloc(&pdev->dev, sizeof *data, GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	/*
	 * XXX - is there something less clumsy than deferring probing a whole
	 * bunch of times while other devices physical_nodes are being created?
	 * What if the devices never show up?
	 *
	 * ACPI _DEP should help, but Linux doesn't seem to do anything with it.
	 *
	 * It may also be a good idea to build a dependency tree based on
	 * finding device references in _DSD properties.
	 *
	 * Or can I just mark this Linux driver as a late binder?
	 */
	data->swc.onie_eeprom = swc_fw_util_get_ref_physical(&pdev->dev, "onie-eeprom");
	if (PTR_ERR(data->swc.onie_eeprom) == -ENODEV)
		goto probe_again;
	else if (IS_ERR(data->swc.onie_eeprom))
		data->swc.onie_eeprom = NULL;

	data->swc.mgmt_eth = swc_fw_util_get_ref_physical(&pdev->dev, "management-ethernet");
	if (PTR_ERR(data->swc.mgmt_eth) == -ENODEV)
		goto probe_again;
	else if (IS_ERR(data->swc.mgmt_eth))
		data->swc.mgmt_eth = NULL;

	data->swc.mgmt_serial = swc_fw_util_get_ref_physical(&pdev->dev, "management-serial");
	if (PTR_ERR(data->swc.mgmt_serial) == -ENODEV)
		goto probe_again;
	else if (IS_ERR(data->swc.mgmt_serial))
		data->swc.mgmt_serial = NULL;

	data->swc.leds = swc_fw_util_get_ref_physical(&pdev->dev, "leds");
	if (PTR_ERR(data->swc.leds) == -ENODEV)
		goto probe_again;
	else if (IS_ERR(data->swc.leds))
		data->swc.leds = NULL;

	platform_set_drvdata(pdev, data);

	swc_device_register(&pdev->dev, &data->swc);

	return 0;

probe_again:
	return -EPROBE_DEFER;
}

static int swc_fw_remove(struct platform_device *pdev)
{
	struct swc_fw_data *data = platform_get_drvdata(pdev);
	swc_device_unregister(&data->swc);
	module_put(swc_class.owner);
	dev_info(&pdev->dev, "removed\n");
	return 0;
}

static struct platform_driver swc_fw_driver = {
	.probe = swc_fw_probe,
	.remove = swc_fw_remove,
	.driver = {
		.name = "switch-complex",
		.of_match_table = swc_fw_of_match,
	},
	.id_table = swc_fw_ids,
};
module_platform_driver(swc_fw_driver);
