/*
 * Device Property GPIO Device Driver
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
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/property.h>
#include <linux/swc/cpld.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include "swc-fw-util.h"

MODULE_AUTHOR("Dustin Byford <dustin@cumulusnetworks.com>");
MODULE_DESCRIPTION("Firmware Defined Switch Complex GPIO Device Driver");
MODULE_LICENSE("GPL v2");

#define SWC_CPLD_GPIO_MAX_OFFSETS 8
struct swc_cpld_gpio_data {
	struct gpio_chip chip;
	struct device *cpld;

	int num_offsets;
	int num_gpios;
	u8 offsets[SWC_CPLD_GPIO_MAX_OFFSETS];
	u8 valid_masks[SWC_CPLD_GPIO_MAX_OFFSETS];
	u8 direction_masks[SWC_CPLD_GPIO_MAX_OFFSETS];
	const char *names[SWC_CPLD_GPIO_MAX_OFFSETS * 8];
};

static const struct platform_device_id swc_cpld_gpio_ids[] = {
	{ "swc-cpld-gpio", 0 },
	{ }
};
MODULE_DEVICE_TABLE(platform, swc_cpld_gpio_ids);

static inline struct swc_cpld_gpio_data *to_swc_cpld_gpio_data(struct gpio_chip *chip)
{
	return container_of(chip, struct swc_cpld_gpio_data, chip);
}

static int gpio_to_offsets_bit(struct gpio_chip *chip, unsigned offset)
{
	struct swc_cpld_gpio_data *data = to_swc_cpld_gpio_data(chip);
	int count, idx;

	count = 0;
	for (idx = 0; idx < data->num_offsets * 8; idx++) {
		if (data->valid_masks[idx / 8] & BIT(idx % 8)) {
			if (count == offset) {
				return idx;
			}
			count++;
		}
	}

	return -ENODEV;
}

static int swc_cpld_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct swc_cpld_gpio_data *data = to_swc_cpld_gpio_data(chip);
	struct regmap *regmap = dev_get_regmap(data->cpld, NULL);
	int offset_bit;
	int val;
	int err;

	offset_bit = gpio_to_offsets_bit(chip, offset);
	if (offset_bit < 0) {
		pr_err("offset bit < 0: %d\n", offset_bit);
		return -EINVAL;
	}

	regmap = swc_cpld_get_regmap(data->cpld);
	if (!regmap)
		return -ENODEV;

	err = regmap_read(regmap, data->offsets[offset_bit / 8], &val);
	swc_cpld_put_regmap(data->cpld, regmap);
	if (err)
		return err;

	dev_info(data->cpld, "read reg 0x%x bit %u val %u\n",
	         data->offsets[offset_bit / 8],
		 offset_bit % 8,
		 (val & BIT(offset_bit % 8)) != 0);

	return (val & BIT(offset_bit % 8)) != 0;
}

static void swc_cpld_gpio_set(struct gpio_chip *chip, unsigned offset, int val)
{
	struct swc_cpld_gpio_data *data = to_swc_cpld_gpio_data(chip);
	struct regmap *regmap;
	int offset_bit;

	offset_bit = gpio_to_offsets_bit(chip, offset);
	if (offset_bit < 0) {
		pr_err("offset bit < 0: %d\n", offset_bit);
		return;
	}
	
	regmap = swc_cpld_get_regmap(data->cpld);
	if (!regmap)
		return;

	dev_info(data->cpld, "write reg 0x%x bit %u val %lu\n",
	         data->offsets[offset_bit / 8],
		 offset_bit % 8,
		 val ? BIT(offset_bit % 8) : 0);

	regmap_update_bits(regmap, data->offsets[offset_bit / 8], BIT(offset_bit % 8),
			   val ? BIT(offset_bit % 8) : 0);

	swc_cpld_put_regmap(data->cpld, regmap);
}

static int swc_cpld_gpio_get_dir(struct gpio_chip *chip, unsigned offset)
{
	struct swc_cpld_gpio_data *data = to_swc_cpld_gpio_data(chip);
	int offset_bit;

	offset_bit = gpio_to_offsets_bit(chip, offset);
	if (offset_bit < 0) {
		pr_err("offset bit < 0: %d\n", offset_bit);
		return -EINVAL;
	}

	dev_info(chip->dev, "get dir offset 0x%x bit 0x%x dir %lu\n",
		 offset_bit / 8,
		 offset_bit % 8,
		 (data->direction_masks[offset_bit / 8] & BIT(offset_bit % 8)));

	if (data->direction_masks[offset_bit / 8] & BIT(offset_bit % 8))
		return GPIOF_DIR_OUT;
	else
		return GPIOF_DIR_IN;
}

static int swc_cpld_gpio_dir_input(struct gpio_chip *chip,
				   unsigned offset)
{
	int dir = swc_cpld_gpio_get_dir(chip, offset);
	dev_info(chip->dev, "direction: %d setting: input\n", dir);
	if (dir == GPIOF_DIR_IN)
		return 0;
	else
		return -EINVAL;
}

static int swc_cpld_gpio_dir_output(struct gpio_chip *chip,
				    unsigned offset, int val)
{
	int dir = swc_cpld_gpio_get_dir(chip, offset);
	dev_info(chip->dev, "direction: %d setting: output\n", dir);

	if (dir == GPIOF_DIR_IN)
		return -EINVAL;

	swc_cpld_gpio_set(chip, offset, val);
	return 0;
}

static int swc_cpld_gpio_probe(struct platform_device *pdev)
{
	struct swc_fw_util_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct swc_cpld_gpio_data *data;
	int num;
	int err;

	dev_info(&pdev->dev, "swc_cpld_gpio_probe()\n");

	if (!pdata) {
		dev_err(&pdev->dev, "missing platform data\n");
		return -ENODEV;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(struct swc_cpld_gpio_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

        num = swc_cpld_get_property(&pdev->dev, "offsets",
				    data->offsets, SWC_CPLD_GPIO_MAX_OFFSETS);
	if (num < 1) {
		dev_err(&pdev->dev, "no offsets\n");
		return -EINVAL;
	}
	data->num_offsets = num;

        err = swc_cpld_get_property_n(&pdev->dev, "valid-masks",
				      data->valid_masks, data->num_offsets);
	if (err) {
		dev_err(&pdev->dev, "failed to get valid-masks\n");
		return err;
	}

	for (num = 0; num < data->num_offsets; num++)
		data->num_gpios += hweight8(data->valid_masks[num]);

	err = swc_cpld_get_property_n(&pdev->dev, "direction-masks",
				      data->direction_masks, data->num_offsets);

	if (err) {
		dev_err(&pdev->dev, "failed to get direction-masks\n");
		return err;
	}

	err = device_property_read_string_array(&pdev->dev, "names",
						data->names, data->num_gpios);

	if (err) {
		dev_err(&pdev->dev, "failed to get names\n");
		return -EINVAL;
	}

	platform_set_drvdata(pdev, data);

	for (num = 0; num < data->num_offsets; num++) {
		u8 props = 0;
		int err;

		/*
		 * XXX - we can't really set these properties per bit.  But, it
		 * might be possible one day using a regmap field to break the
		 * register into fields.  I'm not sure what the advantage would
		 * be for a gpio device though.  We can enforce permissions in
		 * this driver.
		 */
		if (data->valid_masks[num])
			props |= SWC_CPLD_READABLE;
		if (data->direction_masks[num])
			props |= SWC_CPLD_WRITEABLE;
		err = swc_cpld_set_regprops(pdata->cpld, data->offsets[num], props);
		if (err)
			return err;
	}

	data->chip.dev = &pdev->dev;
	data->chip.owner = THIS_MODULE;
	data->chip.label = dev_name(&pdev->dev);
	data->chip.base = -1;
	data->chip.get = swc_cpld_gpio_get;
	data->chip.set = swc_cpld_gpio_set;
	data->chip.get_direction = swc_cpld_gpio_get_dir;
	data->chip.direction_input = swc_cpld_gpio_dir_input;
	data->chip.direction_output = swc_cpld_gpio_dir_output;
	data->chip.ngpio = data->num_gpios;
	data->chip.names = data->names;

	data->cpld = get_device(pdata->cpld);

	platform_set_drvdata(pdev, data);

	err = gpiochip_add(&data->chip);
	if (err)
		goto fail;

	dev_err(&pdev->dev, "added gpio chip with %d pins\n", data->chip.ngpio);
	for (num = 0; num < data->num_gpios; num++) {
		dev_dbg(&pdev->dev, "%s pin %d offset %d\n", data->chip.names[num], num,
			gpio_to_offsets_bit(&data->chip, num));
	}

	return 0;

fail:
	put_device(data->cpld);
	return err;
}

static int swc_cpld_gpio_remove(struct platform_device *pdev)
{
	struct swc_cpld_gpio_data *data = platform_get_drvdata(pdev);

	put_device(data->cpld);
	gpiochip_remove(&data->chip);

	dev_info(&pdev->dev, "removed\n");

	return 0;
}

static struct platform_driver swc_cpld_gpio_driver = {
	.driver = {
		.name		= "swc-cpld-gpio",
		.owner		= THIS_MODULE,
	},
	.probe = swc_cpld_gpio_probe,
	.remove = swc_cpld_gpio_remove,
	.id_table = swc_cpld_gpio_ids,
};
module_platform_driver(swc_cpld_gpio_driver);
