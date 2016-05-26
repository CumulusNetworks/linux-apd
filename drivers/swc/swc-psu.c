#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/property.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/swc/register.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include <linux/nls.h>
#include "swc-fw-util.h"


MODULE_DESCRIPTION("Firmware Defined PSU Device Driver");
MODULE_LICENSE("GPL v2");

extern struct class swc_psu_class;
struct swc_psu_data {
    struct device *dev;
    struct device *psu;
    struct device *pmbus;
    struct device *eeprom;
    struct device *present;
    struct device *power_good;
    struct device *ac_alert;
    struct device *sysfs_data;
    int    present_offset;
    int    power_good_offset;
    int    ac_alert_offset;
    int    num_attrs;
    struct attribute *swc_psu_attrs[16];
    struct attribute_group swc_psu_attr_group;
    struct attribute_group *swc_psu_groups[2];
};

DEFINE_IDA(swc_psu_ida);

static const struct platform_device_id swc_psu_ids[] = {
    { "swc-psu", 0 },
    { }
};
MODULE_DEVICE_TABLE(platform, swc_psu_ids);

static const struct of_device_id swc_psu_of_match[] = {
	{ .compatible = "swc-psu", },
	{ },
};
MODULE_DEVICE_TABLE(of, swc_psu_of_match);

static ssize_t show_present(struct device *dev,
			struct device_attribute *attr, char *buf)
{
    struct swc_psu_data *data = dev_get_drvdata(dev);
    int val;
    int err;
    int mask=0;
    int check, shift=0;

    err = swc_cpld_register_get(data->present, data->present_offset, &val);
    if (err)
    {
        dev_err(dev, "failed to get psu present\n");
        return -EFAULT;
    }
    err = swc_cpld_register_mask_get(data->present, data->present_offset, &mask);
    if (err)
    {
        dev_err(dev, "failed to get psu present\n");
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
DEVICE_ATTR(present, S_IRUGO, show_present, NULL);

static ssize_t show_power_good(struct device *dev,
			struct device_attribute *attr, char *buf)
{
    struct swc_psu_data *data = dev_get_drvdata(dev);
    int val;
    int err;
    int mask=0;
    int check, shift=0;

    err = swc_cpld_register_get(data->power_good, data->power_good_offset, &val);
    if (err)
    {
        dev_err(dev, "failed to get psu present\n");
        return -EFAULT;
    }
    err = swc_cpld_register_mask_get(data->power_good, data->power_good_offset, &mask);
    if (err)
    {
        dev_err(dev, "failed to get psu present mask\n");
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
DEVICE_ATTR(power_good, S_IRUGO, show_power_good, NULL);

static ssize_t show_ac_alert(struct device *dev,
			struct device_attribute *attr, char *buf)
{
    struct swc_psu_data *data = dev_get_drvdata(dev);
    int val;
    int err;
    int mask=0;
    int check, shift=0;

    err = swc_cpld_register_get(data->ac_alert, data->ac_alert_offset, &val);
    if (err)
    {
        dev_err(dev, "failed to get psu ac_alert\n");
        return -EFAULT;
    }
    err = swc_cpld_register_mask_get(data->ac_alert, data->ac_alert_offset, &mask);
    if (err)
    {
        dev_err(dev, "failed to get psu ac_alert mask\n");
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
DEVICE_ATTR(ac_alert, S_IRUGO, show_ac_alert, NULL);
static void swc_psu_add_attr(struct swc_psu_data *data,
			     struct attribute *attr)
{
    if (data->num_attrs >= ARRAY_SIZE(data->swc_psu_attrs))
    {
        pr_err("attr array too small\n");
        return;
    }
    data->swc_psu_attrs[data->num_attrs++] = attr;
}

static void swc_psu_device_unregister(struct swc_psu_data *swc)
{
    int id;

    dev_info(swc->dev, "unregistering\n");

    if (likely(sscanf(dev_name(swc->dev), "psu%d", &id) == 1))
    {
        device_unregister(swc->dev);
        ida_simple_remove(&swc_psu_ida, id);
    }
    else
    {
        dev_err(swc->dev, "failed to unregister swc_psu device\n");
    }
}

static int swc_psu_probe(struct platform_device *pdev)
{
    struct swc_psu_data *data;
    int err, id;

    dev_info(&pdev->dev, "swc_psu_probe()\n");
    data = devm_kzalloc(&pdev->dev, sizeof *data, GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->pmbus = swc_fw_util_get_ref_physical(&pdev->dev, "pmbus");
    if (PTR_ERR(data->pmbus) == -ENODEV)
    {
        goto fail;
    }
    else if (IS_ERR(data->pmbus))
        data->pmbus = NULL;

    data->eeprom = swc_fw_util_get_ref_physical(&pdev->dev, "eeprom");
    if (PTR_ERR(data->eeprom) == -ENODEV)
    {
        goto fail;
    }
    else if (IS_ERR(data->eeprom))
        data->eeprom = NULL;

    data->present = swc_fw_util_get_ref_physical(&pdev->dev, "psu-present");
    if (PTR_ERR(data->present) == -ENODEV)
    {
        return -EPROBE_DEFER;
    }
    else if (IS_ERR(data->present))
    {
        return -ENODEV;
    }
    else
    {
        struct acpi_reference_args ref;

        err = acpi_node_get_property_reference(
        acpi_fwnode_handle(ACPI_COMPANION(&pdev->dev)),
					   "psu-present", 0, &ref);
        if (err)
        {
            dev_err(&pdev->dev, "failed to get psu-present device\n");
            return -EINVAL;
        }
        if (ref.nargs > 1)
        {
            dev_err(&pdev->dev, "too many args to 'psu-present'\n");
            return -EINVAL;
        }
        if (ref.nargs == 0)
            data->present_offset = 0;
        else
            data->present_offset = ref.args[0];
    }
    data->power_good = swc_fw_util_get_ref_physical(&pdev->dev, "psu-power-good");
    if (PTR_ERR(data->power_good) == -ENODEV)
    {
        return -EPROBE_DEFER;
    }
    else if (IS_ERR(data->power_good))
    {
        return -ENODEV;
    }
    else
    {
        struct acpi_reference_args ref;

        err = acpi_node_get_property_reference(
        acpi_fwnode_handle(ACPI_COMPANION(&pdev->dev)),
					   "psu-power-good", 0, &ref);
        if (err)
        {
            dev_err(&pdev->dev, "failed to get power-good device\n");
            return -EINVAL;
        }
        if (ref.nargs > 1)
        {
            dev_err(&pdev->dev, "too many args to 'power-good'\n");
            return -EINVAL;
        }
        if (ref.nargs == 0)
            data->power_good_offset = 0;
        else
            data->power_good_offset = ref.args[0];
    }
    data->ac_alert = swc_fw_util_get_ref_physical(&pdev->dev, "psu-ac-alert");
    if (PTR_ERR(data->ac_alert) == -ENODEV)
    {
        return -EPROBE_DEFER;
    }
    else if (IS_ERR(data->ac_alert))
    {
        return -ENODEV;
    }
    else
    {
        struct acpi_reference_args ref;

        err = acpi_node_get_property_reference(
        acpi_fwnode_handle(ACPI_COMPANION(&pdev->dev)),
					   "psu-ac-alert", 0, &ref);
        if (err)
        {
            dev_err(&pdev->dev, "failed to get psu-ac-alert device\n");
            return -EINVAL;
        }
        if (ref.nargs > 1)
        {
            dev_err(&pdev->dev, "too many args to 'psu-ac-alert'\n");
            return -EINVAL;
        }
        if (ref.nargs == 0)
            data->ac_alert_offset = 0;
        else
            data->ac_alert_offset = ref.args[0];
    }
    data->present = get_device(data->present);
    data->power_good = get_device(data->power_good);
    data->ac_alert = get_device(data->ac_alert);
    platform_set_drvdata(pdev, data);

    if(data->present)
        swc_psu_add_attr(data, &dev_attr_present.attr);
    if(data->power_good)
        swc_psu_add_attr(data, &dev_attr_power_good.attr);
    if(data->ac_alert)
        swc_psu_add_attr(data, &dev_attr_ac_alert.attr);

    if (data->num_attrs)
    {
        id = ida_simple_get(&swc_psu_ida, 0, 0, GFP_KERNEL);
        if (id < 0)
           return id;
        data->dev = device_create(&swc_psu_class, &pdev->dev, MKDEV(0, 0),
                                 data, "psu%d", id);
        if (IS_ERR(data->dev))
        {
            err = -ENODEV;
            goto fail;
        }
        if(data->pmbus)
        {
            err = sysfs_create_link(&data->dev->kobj, &data->pmbus->kobj,
            "pmbus");
        }
        if(data->eeprom)
        {
            err = sysfs_create_link(&data->dev->kobj, &data->eeprom->kobj,
            "psu_eeprom");
        }
        data->swc_psu_attr_group.attrs = data->swc_psu_attrs;
        err = sysfs_create_group(&data->dev->kobj, &data->swc_psu_attr_group);
        if (err)
            goto fail;
    }
    dev_info(&pdev->dev, "added swc-psu with %d attrs\n", data->num_attrs);

    return 0;

fail:
    put_device(data->present);
    put_device(data->power_good);
    put_device(data->ac_alert);
    return err;
}

static int swc_psu_remove(struct platform_device *pdev)
{
    struct swc_psu_data *data = platform_get_drvdata(pdev);

    if (data->num_attrs)
    {
        sysfs_remove_group(&data->dev->kobj, &data->swc_psu_attr_group);
        swc_psu_device_unregister(data);
    }
    put_device(data->psu);
    dev_info(&pdev->dev, "removed\n");

    return 0;
}

static struct platform_driver swc_psu_driver = {
	.driver = {
		.name		= "swc-psu",
		.owner		= THIS_MODULE,
		.of_match_table = swc_psu_of_match,
	},
	.probe = swc_psu_probe,
	.remove = swc_psu_remove,
	.id_table = swc_psu_ids,
};
module_platform_driver(swc_psu_driver);




