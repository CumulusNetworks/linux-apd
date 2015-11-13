#ifndef SWC_CPLD_H_
#define SWC_CPLD_H_

#include <linux/device.h>
#include <linux/regmap.h>

#define SWC_CPLD_READABLE BIT(0)
#define SWC_CPLD_WRITEABLE BIT(1)
#define SWC_CPLD_VOLATILE BIT(2)

extern int swc_cpld_set_regprops(struct device *dev, unsigned int reg, u8 props);
extern int swc_cpld_get_property(struct device *dev, const char *property, u8 *arr, size_t arr_size);
extern int swc_cpld_get_property_n(struct device *dev, const char *property, u8 *arr, size_t arr_size);
extern struct regmap *swc_cpld_get_regmap(struct device *dev);
extern void swc_cpld_put_regmap(struct device *dev, struct regmap *regmap);

#endif
