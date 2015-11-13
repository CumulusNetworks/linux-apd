#ifndef __SWX_FW
#define __SWX_FW

#include <linux/device.h>
#include <linux/property.h>
#include <linux/acpi.h>
#include <linux/swc.h>
#include <linux/swp.h>
#include <linux/sff.h>

struct swc_fw_util_platform_data {
	struct swc_device *swc;
	struct device *cpld;
};

extern size_t swc_fw_util_sysfs_to_property(const char *src, char *dst, size_t len);
extern size_t swc_fw_util_property_to_sysfs(const char *src, char *dst, size_t len);
extern struct device *swc_fw_util_get_ref_physical(struct device *dev, const char *name);
extern int swc_fw_util_acpi_get_adr(struct acpi_device *adev, u64 *adr);

#endif
