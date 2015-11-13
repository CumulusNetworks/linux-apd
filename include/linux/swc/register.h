#ifndef SWC_CPLD_REGISTER_H_
#define SWC_CPLD_REGISTER_H_

#include <linux/device.h>

extern int swc_cpld_register_set(struct device *dev, int reg, int val);
extern int swc_cpld_register_get(struct device *dev, int reg, int *out);

#endif
