/*
 * include/linux/meizu-sys.h
 *
 * meizu technology particular classes
 *
 */

#ifndef __H_MEIZU_PARTICULAR_CLASS_H__
#define __H_MEIZU_PARTICULAR_CLASS_H__

#include <linux/types.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/sysfs.h>

int meizu_sysfslink_register(struct device *dev);
int meizu_sysfslink_register_name(struct device *dev, char *name);
void meizu_sysfslink_unregister(struct device *dev);

#endif /*__H_MEIZU_PARTICULAR_CLASS_H__*/
