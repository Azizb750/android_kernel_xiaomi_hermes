/*
 * Copyright (C) 2011-2014 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/string.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include "lcm_drv.h"
#include "tps65132_i2c.h"

#include <mach/mt_gpio.h>
#include <cust_i2c.h>

static struct i2c_board_info __initdata tps65132_board_info = {I2C_BOARD_INFO("tps65132", 0x3E)};
static struct i2c_client *tps65132_i2c_client = NULL;
static int tps65132_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int tps65132_remove(struct i2c_client *client);

struct tps65132_dev {
    struct i2c_client *client;
};

static const struct i2c_device_id tps65132_id[] = {
    {"tps65132", 0}
};

static struct i2c_driver tps65132_iic_driver = {
    .id_table = tps65132_id,
    .probe = tps65132_probe,
    .remove = tps65132_remove,
    .driver = {
        .owner = THIS_MODULE,
        .name = "tps65132"
    }
};

static int tps65132_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    tps65132_i2c_client = client;
    return 0;
}

static int tps65132_remove(struct i2c_client *client)
{
    tps65132_i2c_client = NULL;
    i2c_unregister_device(client);
    return 0;
}

int tps65132_write_bytes(unsigned char addr, unsigned char value)
{
    int ret;
    struct i2c_client *client = tps65132_i2c_client;
    char write_data[2];
    write_data[0] = addr;
    write_data[1] = value;
    ret = i2c_master_send(client, write_data, 2);
    if (ret < 0)
        printk("tps65132 write fail\n");
    return ret;
}

static int __init tps65132_iic_init(void)
{
    i2c_register_board_info(0, &tps65132_board_info, 1);
    i2c_add_driver(&tps65132_iic_driver);
    return 0;
}

static void __exit tps65132_iic_exit(void)
{
    i2c_del_driver(&tps65132_iic_driver);
}

module_init(tps65132_iic_init);
module_exit(tps65132_iic_exit);

MODULE_AUTHOR("Xiaokuan Shi");
MODULE_DESCRIPTION("MTK TPS65132 I2C Driver");
MODULE_LICENSE("GPL");
