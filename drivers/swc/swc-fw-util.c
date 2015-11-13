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

	err = acpi_node_get_property_reference(dev->fwnode, name, 0, &ref);
	if (err)
		return ERR_PTR(-EINVAL);

	physical = acpi_find_physical_dev(ref.adev);
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
