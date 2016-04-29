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


MODULE_DESCRIPTION("Firmware Defined Led Device Driver");
MODULE_LICENSE("GPL v2");

static struct class swc_led_class = {
    .name = "swc-led",
    .owner = THIS_MODULE,
};

struct swc_led_data {
    struct device *dev;
    struct device *leds;
    struct device *led_diag_green;
    struct device *led_diag_amber;
    struct device *led_loc_blue;
    struct device *sysfs_data;
    int led_diag_green_offset;	
    int led_diag_amber_offset;	
    int led_loc_blue_offset;	
    int num_attrs;	
    struct attribute *swc_led_attrs[16];
    struct attribute_group swc_led_attr_group;	
    struct attribute_group *swc_led_groups[2];
};


static const struct platform_device_id swc_led_ids[] = {
    { "swc-led", 0 },
    { }
};
MODULE_DEVICE_TABLE(platform, swc_led_ids);

static const struct of_device_id swc_led_of_match[] = {
	{ .compatible = "swc-led", },
	{ },
};
MODULE_DEVICE_TABLE(of, swc_led_of_match);


static ssize_t set_led_diag_green(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
    struct swc_led_data *data = dev_get_drvdata(dev);    
    unsigned long value;
    int err, mask, shift, check=0;

    if (kstrtoul(buf, 0, &value))
        return -EINVAL;
        
    if(swc_cpld_register_mask_get(data->led_diag_green, data->led_diag_green_offset, &mask))
    {
        dev_err(dev, "failed to get led_diag_green mask\n");
        return -EFAULT;
    }    
    for(shift=0;shift<8;shift++)
    {
        check=mask&(1<<shift);
        if(check)
            break;
    }
    value=(~(value<<shift)) & mask;
    err = swc_cpld_register_set(data->led_diag_green, data->led_diag_green_offset, value);
    if (err)
    {
        dev_err(dev, "failed to set led diag-green\n");
        return -EFAULT;
    }
   
    return count;
}

static ssize_t show_led_diag_green(struct device *dev,
			struct device_attribute *attr, char *buf)
{
    struct swc_led_data *data = dev_get_drvdata(dev);  
    int val;
    int err;
    int mask=0;
    int check, shift=0;

    err = swc_cpld_register_get(data->led_diag_green, data->led_diag_green_offset, &val);
    if (err)
    {
        dev_err(dev, "failed to get fan present\n");
        return -EFAULT;
    }
	
    err = swc_cpld_register_mask_get(data->led_diag_green, data->led_diag_green_offset, &mask);
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

DEVICE_ATTR(diag_green, S_IRUGO | S_IWUSR, show_led_diag_green, set_led_diag_green);

static ssize_t set_led_diag_amber(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
    struct swc_led_data *data = dev_get_drvdata(dev);    
    unsigned long value;
    int err;
    int mask, check, shift=0;

    if (kstrtoul(buf, 0, &value))
        return -EINVAL;
    
    if(swc_cpld_register_mask_get(data->led_diag_amber, data->led_diag_amber_offset, &mask))
    {
        dev_err(dev, "failed to get led_diag_amber mask\n");
        return -EFAULT;
    }    
    for(shift=0;shift<8;shift++)
    {
        check=mask&(1<<shift);
        if(check)
            break;
    }
    value=(~(value<<shift)) & mask;
    err = swc_cpld_register_set(data->led_diag_amber, data->led_diag_amber_offset, value);
    if (err)
    {
        dev_err(dev, "failed to set led_diag_amber\n");
        return -EFAULT;
    }
   
    return count;
}

static ssize_t show_led_diag_amber(struct device *dev,
			struct device_attribute *attr, char *buf)
{
    struct swc_led_data *data = dev_get_drvdata(dev);  
    int val;
    int err;
    int mask=0;
    int check, shift=0;

    err = swc_cpld_register_get(data->led_diag_amber, data->led_diag_amber_offset, &val);
    if (err)
    {
        dev_err(dev, "failed to get fan present\n");
        return -EFAULT;
    }
	
    err = swc_cpld_register_mask_get(data->led_diag_amber, data->led_diag_amber_offset, &mask);
    if (err)
    {
        dev_err(dev, "failed to get led_diag_amber\n");
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

DEVICE_ATTR(diag_amber, S_IRUGO | S_IWUSR, show_led_diag_amber, set_led_diag_amber);


static ssize_t set_led_loc_blue(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
    struct swc_led_data *data = dev_get_drvdata(dev);    
    unsigned long value;
    int err, mask, shift, check=0;

    if (kstrtoul(buf, 0, &value))
        return -EINVAL;
   
    if(swc_cpld_register_mask_get(data->led_loc_blue, data->led_loc_blue_offset, &mask))
    {
        dev_err(dev, "failed to get led_loc_blue mask\n");
        return -EFAULT;
    }    
    for(shift=0;shift<8;shift++)
    {
        check=mask&(1<<shift);
        if(check)
            break;
    }   
    value=(~(value<<shift)) & mask;
    err = swc_cpld_register_set(data->led_loc_blue, data->led_loc_blue_offset, value);
    if (err)
    {
        dev_err(dev, "failed to set led_loc_blue\n");
        return -EFAULT;
    }
   
    return count;
}

static ssize_t show_led_loc_blue(struct device *dev,
			struct device_attribute *attr, char *buf)
{
    struct swc_led_data *data = dev_get_drvdata(dev);  
    int val;
    int err;
    int mask=0;
    int check, shift=0;

    err = swc_cpld_register_get(data->led_loc_blue, data->led_loc_blue_offset, &val);
    if (err)
    {
        dev_err(dev, "failed to get led_loc\n");
        return -EFAULT;
    }
	
    err = swc_cpld_register_mask_get(data->led_loc_blue, data->led_loc_blue_offset, &mask);
    if (err)
    {
        dev_err(dev, "failed to get led_loc_blue\n");
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

DEVICE_ATTR(loc_blue, S_IRUGO | S_IWUSR, show_led_loc_blue, set_led_loc_blue);




static void swc_led_add_attr(struct swc_led_data *data,
			     struct attribute *attr)
{
    if (data->num_attrs >= ARRAY_SIZE(data->swc_led_attrs))
    {
        pr_err("attr array too small\n");
        return;
    }
    data->swc_led_attrs[data->num_attrs++] = attr;
}

static int swc_led_probe(struct platform_device *pdev)
{
    struct swc_led_data *data;
    int err;
    
    dev_info(&pdev->dev, "swc_led_probe()\n");
    data = devm_kzalloc(&pdev->dev, sizeof *data, GFP_KERNEL);
    if (!data)
        return -ENOMEM;
    
    data->leds = swc_fw_util_get_ref_physical(&pdev->dev, "leds");
    if (PTR_ERR(data->leds) == -ENODEV)
    {
        return -EPROBE_DEFER;
    }
    else if (IS_ERR(data->leds))
    {
        return -ENODEV;
    }

    if (!get_device(data->leds))
        return -ENODEV;
    
    err = sysfs_create_link(&pdev->dev.kobj, &data->leds->kobj,
				"system-led");
    if (err)
        goto fail;
    
    data->led_diag_green = swc_fw_util_get_ref_physical(&pdev->dev, "diag-green");
    if (PTR_ERR(data->led_diag_green) == -ENODEV)
    {
        return -EPROBE_DEFER;
    }
    else if (IS_ERR(data->led_diag_green))
    {
        return -ENODEV;
    }
    else
    {
        struct acpi_reference_args ref;
        
        err = acpi_node_get_property_reference(
        acpi_fwnode_handle(ACPI_COMPANION(&pdev->dev)),
					   "diag-green", 0, &ref);
        if (err)
        {
            dev_err(&pdev->dev, "failed to get diag-green device\n");
            return -EINVAL;
        }
        if (ref.nargs > 1)
        {
            dev_err(&pdev->dev, "too many args to 'diag-green'\n");
            return -EINVAL;
        }
        if (ref.nargs == 0)
            data->led_diag_green_offset = 0;
        else
            data->led_diag_green_offset = ref.args[0];
    }
    
    data->led_diag_amber = swc_fw_util_get_ref_physical(&pdev->dev, "diag-amber");
    if (PTR_ERR(data->led_diag_amber) == -ENODEV)
    {
        return -EPROBE_DEFER;
    }
    else if (IS_ERR(data->led_diag_amber))
    {
        return -ENODEV;
    }
    else
    {
        struct acpi_reference_args ref;
        
        err = acpi_node_get_property_reference(
        acpi_fwnode_handle(ACPI_COMPANION(&pdev->dev)),
					   "diag-amber", 0, &ref);
        if (err)
        {
            dev_err(&pdev->dev, "failed to get diag-amber device\n");
            return -EINVAL;
        }
        if (ref.nargs > 1)
        {
            dev_err(&pdev->dev, "too many args to 'diag-amber'\n");
            return -EINVAL;
        }
        if (ref.nargs == 0)
            data->led_diag_amber_offset = 0;
        else
            data->led_diag_amber_offset = ref.args[0];
    }
     data->led_loc_blue = swc_fw_util_get_ref_physical(&pdev->dev, "loc-blue");
    if (PTR_ERR(data->led_loc_blue) == -ENODEV)
    {
        return -EPROBE_DEFER;
    }
    else if (IS_ERR(data->led_loc_blue))
    {
        return -ENODEV;
    }
    else
    {
        struct acpi_reference_args ref;
        
        err = acpi_node_get_property_reference(
        acpi_fwnode_handle(ACPI_COMPANION(&pdev->dev)),
					   "loc-blue", 0, &ref);
        if (err)
        {
            dev_err(&pdev->dev, "failed to get loc-blue device\n");
            return -EINVAL;
        }
        if (ref.nargs > 1)
        {
            dev_err(&pdev->dev, "too many args to 'loc-blue'\n");
            return -EINVAL;
        }
        if (ref.nargs == 0)
            data->led_loc_blue_offset = 0;
        else
            data->led_loc_blue_offset = ref.args[0];
    }
    
    data->dev = &pdev->dev;   
    data->led_diag_green = get_device(data->led_diag_green);
    data->led_diag_amber = get_device(data->led_diag_amber);
    data->led_loc_blue = get_device(data->led_loc_blue);
	  platform_set_drvdata(pdev, data);
    
    if(data->led_diag_green)
        swc_led_add_attr(data, &dev_attr_diag_green.attr);
    if(data->led_diag_amber)
        swc_led_add_attr(data, &dev_attr_diag_amber.attr);    
    if(data->led_loc_blue)
        swc_led_add_attr(data, &dev_attr_loc_blue.attr);            
    
    if (data->num_attrs)
    {
        data->swc_led_attr_group.attrs = data->swc_led_attrs;

        err = sysfs_create_group(&pdev->dev.kobj, &data->swc_led_attr_group);
        if (err)
        goto fail;
    }
    dev_info(&pdev->dev, "added sff with %d attrs\n", data->num_attrs);
        
    return 0; 
    
fail:
    put_device(data->leds);
    put_device(data->led_diag_green);
    put_device(data->led_diag_amber);
    put_device(data->led_loc_blue);
    return err;
}

static int swc_led_remove(struct platform_device *pdev)
{	
    struct swc_led_data *data = platform_get_drvdata(pdev);
        
    sysfs_remove_link(&pdev->dev.kobj, "system-led");

    if (data->num_attrs)
        sysfs_remove_group(&pdev->dev.kobj, &data->swc_led_attr_group);
        
    put_device(data->leds);
    dev_info(&pdev->dev, "removed\n");
    return 0;
}

static struct platform_driver swc_led_driver = {
	.driver = {
		.name		= "swc-led",
		.owner		= THIS_MODULE,
		.of_match_table = swc_led_of_match,
	},
	.probe = swc_led_probe,
	.remove = swc_led_remove,
	.id_table = swc_led_ids,
};
module_platform_driver(swc_led_driver);
