#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include "base.h"

static struct class *meizu_class;

/*
 * Create link ysfs interface
 */
int meizu_sysfslink_register(struct device *dev)
{
	int ret = 0;

	if(dev == NULL)
		return -1;

	ret = sysfs_create_link(&meizu_class->p->subsys.kobj,
			&dev->kobj, dev->driver->name);
	if (ret < 0) {
		pr_err("%s()->%d:can not create sysfs link!\n",
			__func__, __LINE__);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(meizu_sysfslink_register);

int meizu_sysfslink_register_name(struct device *dev, char *name)
{
	int ret = 0;

	if(dev == NULL)
		return -1;

	if(name == NULL) {
		ret = sysfs_create_link(&meizu_class->p->subsys.kobj,
				&dev->kobj, dev->driver->name);
	} else {
		ret = sysfs_create_link(&meizu_class->p->subsys.kobj,
				&dev->kobj, name);
	}
	if (ret < 0) {
		pr_err("%s()->%d:can not create sysfs link!\n",
			__func__, __LINE__);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(meizu_sysfslink_register_name);

void meizu_sysfslink_unregister(struct device *dev)
{
	device_unregister(dev);
}
EXPORT_SYMBOL_GPL(meizu_sysfslink_unregister);

static int __init meizu_class_init(void)
{
        meizu_class = class_create(THIS_MODULE, "meizu");
        if (IS_ERR(meizu_class)) {
                pr_err("%s, create meizu_class failed.(err=%ld)\n",
                        __func__, IS_ERR(meizu_class));
                return PTR_ERR(meizu_class);
        }

	return 0;
}

static void __exit meizu_class_exit(void)
{
	class_destroy(meizu_class);
	meizu_class = NULL;
}

subsys_initcall(meizu_class_init);
module_exit(meizu_class_exit);

MODULE_DESCRIPTION("Meizu technology particular class");
MODULE_AUTHOR("Li Tao <litao@meizu.com>, tangxingyan<tangxingyan@meizu.com>");
MODULE_LICENSE("GPL");
