#include <linux/device.h>
#include <linux/property.h>
#include <linux/module.h>
#include <linux/acpi.h>
#include "swc-fw-util.h"

MODULE_AUTHOR("Dustin Byford");
MODULE_DESCRIPTION("Network Switch Complex and Port Firmware Driver Utilities");
MODULE_LICENSE("GPL");

size_t swc_fw_util_sysfs_to_property(const char *src, char *dst, size_t len)
{
	size_t i;

	for (i = 0; i < min(strlen(src), len - 1); i++) {
		if (src[i] == '_')
			dst[i] = '-';
		else
			dst[i] = src[i];
	}
	dst[i] = '\0';

	return i;
}
EXPORT_SYMBOL(swc_fw_util_sysfs_to_property);

size_t swc_fw_util_property_to_sysfs(const char *src, char *dst, size_t len)
{
	size_t i;

	for (i = 0; i < min(strlen(src), len - 1); i++) {
		if (src[i] == '-')
			dst[i] = '_';
		else
			dst[i] = src[i];
	}
	dst[i] = '\0';

	return i;
}
EXPORT_SYMBOL(swc_fw_util_property_to_sysfs);

int swc_fw_util_acpi_get_adr(struct acpi_device *adev, u64 *adr)
{
	acpi_status adr_status;

	adr_status = acpi_evaluate_integer(adev->handle, "_ADR", NULL, adr);

	if (ACPI_FAILURE(adr_status)) {
		struct acpi_buffer path = { ACPI_ALLOCATE_BUFFER, NULL };
		acpi_status path_status;

		path_status = acpi_get_name(adev->handle, ACPI_FULL_PATHNAME,
		        	   	    &path);
		dev_err(&adev->dev, "failed to get ACPI address for %s, err=%d\n",
			ACPI_FAILURE(path_status) ? "(unknown)" : (char *)path.pointer,
			adr_status);
		kfree(path.pointer);
		return -ENODEV;
	}

	return 0;
}
EXPORT_SYMBOL(swc_fw_util_acpi_get_adr);

struct device *swc_fw_util_get_ref_physical(struct device *dev, const char *name)
{
	struct device *physical;
	struct acpi_reference_args ref;
	int err;

	err = acpi_node_get_property_reference(acpi_fwnode_handle(ACPI_COMPANION(dev)),
					       name, 0, &ref);
	if (err)
		return ERR_PTR(-EINVAL);

	physical = acpi_dev_get_physical(ref.adev);
	if (IS_ERR(physical))
		dev_warn(&ref.adev->dev, "no physical nodes\n");
		return physical;

	return physical;
}
EXPORT_SYMBOL(swc_fw_util_get_ref_physical);

static int devmatch(struct device *dev, const void *data)
{
	return dev->parent == data;
}

struct device *swc_fw_util_find_class_device(struct class *class, struct device *dev)
{
	return class_find_device(class, NULL, dev, devmatch);
}
EXPORT_SYMBOL(swc_fw_util_find_class_device);

#if 0
ssize_t swc_fw_util_device_property_str_show(struct device *dev,
					struct device_attribute *dattr,
					char *buf)
{
	const char *str;
	int err;

	swc_fw_util_sysfs_to_property(dattr->attr.name, buf, 128);

	err = device_property_read_string(dev->parent, buf, &str);
	if (err) {
		dev_err(dev, "failed to get device property: %s\n", buf);
		return -ENODEV;
	}

	return sprintf(buf, "%s\n", str);
}

static ssize_t swc_device_property_u64_show(struct device *dev,
		                            struct device_attribute *dattr,
					    char *buf)
{
	u64 val;
	int err;

	swc_fw_util_sysfs_to_property(dattr->attr.name, buf, 128);

	err = device_property_read_u64(dev->parent, buf, &val);
	if (err) {
		dev_err(dev, "failed to get device property: %s\n", buf);
		return -ENODEV;
	}

	return sprintf(buf, "0x%llx\n", val);
}


static int swc_class_link_adr(struct kobject *kobj, struct device *dev, char *prefix, int adr) {
	char name[16];
	int err;
		
	sprintf(name, "%s%u", prefix, adr);

	err = sysfs_create_link(kobj, &dev->kobj, name);
	if (err) {
		dev_err(dev, "failed to create swc class link for %s\n", name);
		return -ENODEV;
	}

	return 0;
}

static int swp_class_links(struct device *dev, struct device *cdev) {
	int port;
	int err;

	/*
	 * XXX - A shortcut.  There should be a port class with a struct
	 * port_device we fill with the appropriate function pointers and
	 * information.  The port class driver should be the one to create
	 * these links and sysfs nodes.  That way, we could mix firmware
	 * defined ports with native port drivers.  It's kind of a hack that
	 * I'm just linking together acpi devices under the swp class then
	 * peppering them with properties; that's no good for access by other
	 * kernel drivers.
	 */

	err = swc_device_property_link_ref(&cdev->kobj, dev, "system_eeprom");
	if (err == -ENODEV)
		return err;
	else if (!err)
		err = sysfs_create_file(&cdev->kobj, &dev_attr_system_eeprom_format.attr);
		// system-eeprom-format is optional

	port = 0;
	while (true) {
		struct acpi_device *port_adev;
		int lane;

		port_adev = acpi_find_child_device(ACPI_COMPANION(dev), port, false);
		if (!port_adev)
			break;

		swc_class_link_adr(&cdev->kobj, &port_adev->dev, "port", port);
		dev_dbg(dev, "found port %u\n", port);

		lane = 0;
		while (true) {
			struct acpi_device *lane_adev;

			lane_adev = acpi_find_child_device(port_adev, lane, false);
			if (!lane_adev)
				break;

			swc_class_link_adr(&port_adev->dev.kobj, &lane_adev->dev, "lane", lane);
			dev_dbg(dev, "found lane %u\n", lane);

			lane++;
		}

		port++;
	}

	return 0;
}
#endif
