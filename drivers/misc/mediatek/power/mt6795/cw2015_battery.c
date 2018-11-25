/*
 * Copyright (C) 2012 CellWise
 *
 * Authors: ChenGang <ben.chen@cellwise-semi.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.And this driver depends on
 * I2C and uses IIC bus for communication with the host.
 *
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/i2c.h>

#include <mach/charging.h>
#include <mach/mt_gpio.h>

#define BAT_CHANGE_ALGORITHM
#ifdef BAT_CHANGE_ALGORITHM
#define FILE_PATH "/data/lastsoc"
#define CPSOC  94
#define NORMAL_CYCLE 10
#endif

#define MODE_SLEEP              (0x3<<6)
#define MODE_NORMAL             (0x0<<6)
#define MODE_RESTART            (0xf<<0)

#define CONFIG_UPDATE_FLG       (0x1<<1)
#define ATHD                    (0x0<<3)        //ATHD = 0%

#define BATTERY_UP_MAX_CHANGE   420             // the max time allow battery change quantity
#define BATTERY_DOWN_CHANGE   60                // the max time allow battery change quantity
#define BATTERY_DOWN_MIN_CHANGE_RUN 30          // the min time allow battery change quantity when run
#define BATTERY_DOWN_MIN_CHANGE_SLEEP 1800      // the min time allow battery change quantity when run 30min

#define BATTERY_DOWN_MAX_CHANGE_RUN_AC_ONLINE 1800

#define USB_CHARGER_MODE        1
#define AC_CHARGER_MODE         2

#define FG_CW2015_DEBUG 1
#define FG_CW2015_TAG                  "[FG_CW2015]"
#ifdef FG_CW2015_DEBUG
#define FG_CW2015_FUN(f)               printk(KERN_ERR FG_CW2015_TAG"%s\n", __FUNCTION__)
#define FG_CW2015_ERR(fmt, args...)    printk(KERN_ERR FG_CW2015_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define FG_CW2015_LOG(fmt, args...)    printk(KERN_ERR FG_CW2015_TAG fmt, ##args)
#endif

#define CW2015_DEV_NAME "CW2015"
static const struct i2c_device_id cw2015_i2c_id[] = {{CW2015_DEV_NAME, 0}, {}};
static struct i2c_board_info __initdata cw2015_i2c = {I2C_BOARD_INFO(CW2015_DEV_NAME, 0x62)};

int cw2015_capacity = 0, cw2015_voltage = 0;
extern int cw2015_charging_type, cw2015_charging_status;

static u8 config_info_cos[64] = {
    0x17, 0xF3, 0x63, 0x6A, 0x6A, 0x68, 0x68, 0x65, 0x63, 0x60, 0x5B, 0x59, 0x65, 0x5B, 0x46, 0x41, 
    0x36, 0x31, 0x28, 0x27, 0x31, 0x35, 0x43, 0x51, 0x1C, 0x3B, 0x0B, 0x85, 0x22, 0x42, 0x5B, 0x82, 
    0x99, 0x92, 0x98, 0x96, 0x3D, 0x1A, 0x66, 0x45, 0x0B, 0x29, 0x52, 0x87, 0x8F, 0x91, 0x94, 0x52, 
    0x82, 0x8C, 0x92, 0x96, 0x54, 0xC2, 0xBA, 0xCB, 0x2F, 0x7D, 0x72, 0xA5, 0xB5, 0xC1, 0xA5, 0x49
};

static u8 config_info_des[64] = {
    0x17, 0xF9, 0x6D, 0x6D, 0x6B, 0x67, 0x65, 0x64, 0x58, 0x6D, 0x6D, 0x48, 0x57, 0x5D, 0x4A, 0x43,
    0x37, 0x31, 0x2B, 0x20, 0x24, 0x35, 0x44, 0x55, 0x20, 0x37, 0x0B, 0x85, 0x2A, 0x4A, 0x56, 0x68,
    0x74, 0x6B, 0x6D, 0x6E, 0x3C, 0x1A, 0x5C, 0x45, 0x0B, 0x30, 0x52, 0x87, 0x8F, 0x91, 0x94, 0x52,
    0x82, 0x8C, 0x92, 0x96, 0x64, 0xB4, 0xDB, 0xCB, 0x2F, 0x7D, 0x72, 0xA5, 0xB5, 0xC1, 0xA5, 0x42
};

struct cw2015_battery {
    struct i2c_client *client;
    struct workqueue_struct *battery_workqueue;
    struct delayed_work battery_delay_work;
    long sleep_time_capacity_change;
    long run_time_capacity_change;
    long sleep_time_charge_start;
    long run_time_charge_start;
    int usb_online;
    int charger_mode;
    int capacity;
    int voltage;
    int bat_change;
};

#ifdef BAT_CHANGE_ALGORITHM
struct cw_store {
    long bts; 
    int OldSOC;
    int DetSOC;
    int AlRunFlag;
};
#endif

#ifdef BAT_CHANGE_ALGORITHM
static int PowerResetFlag = -1, alg_run_flag = -1, count_num = 0, count_sp = 0;

static unsigned int cw_convertData(struct cw2015_battery *cw_bat, unsigned int ts)
{
    unsigned int i = ts%4096,n = ts/4096;
    unsigned int ret = 65536;

    if (i>=1700){i-=1700;ret=(ret*3)/4;}else{}
    if (i>=1700){i-=1700;ret=(ret*3)/4;}else{}
    if (i>=789){i-=789;ret=(ret*7)/8;}else{}
    if (i>=381){i-=381;ret=(ret*15)/16;}else{}  
    if (i>=188){i-=188;ret=(ret*31)/32;}else{}
    if (i>=188){i-=188;ret=(ret*31)/32;}else{}
    if (i>=93){i-=93;ret=(ret*61)/64;}else{}
    if (i>=46){i-=46;ret=(ret*127)/128;}else{}
    if (i>=23){i-=23;ret=(ret*255)/256;}else{}
    if (i>=11){i-=11;ret=(ret*511)/512;}else{}
    if (i>=6){i-=6;ret=(ret*1023)/1024;}else{}
    if (i>=3){i-=3;ret=(ret*2047)/2048;}else{} 
    if (i>=3){i-=3;ret=(ret*2047)/2048;}else{} 
    
    return ret>>n; 
}

static int AlgNeed(struct cw2015_battery *cw_bat, int dSOC, int SOCT0)
{
    int dSOC1 = dSOC * 100;
    int SOCT01 = SOCT0 * 100;
    if (SOCT01 < 2400) {
        if ((dSOC1 > -100) && (dSOC1 < ((2400 + SOCT01) / 3)))
            return 1;
    } else if (SOCT01 < 4800) {
        if ((dSOC1 > -100) && (dSOC1 < ((7200 - SOCT01) / 3)))
            return 1;
    } else {
        if ((dSOC1 > -100) && (dSOC1 < 800))
            return 1;
    }
    return -1;
}
static int cw_algorithm(struct cw2015_battery *cw_bat, int real_capacity)
{
    //struct timespec ts;
    struct file *file = NULL;
    struct cw_store st;
    struct inode *inode;
    mm_segment_t old_fs;
    int vmSOC; 
    unsigned int utemp,utemp1;
    long timeNow;
  
	count_num = count_num + 1;
    if (count_num<count_sp)
	{
	    return real_capacity;
	}
	else
	{
	    count_num = 0;
	}
    timeNow = get_seconds();
    vmSOC = real_capacity;
    if (file == NULL)
        file = filp_open(FILE_PATH,O_RDWR|O_CREAT,0644);
    if (IS_ERR(file))
    {
        FG_CW2015_ERR(" error occured while opening file %s,exiting...\n",FILE_PATH);
        return real_capacity;
    }
    old_fs = get_fs();
    set_fs(KERNEL_DS); 
    inode = file->f_dentry->d_inode;

    if ((long)(inode->i_size)<(long)sizeof(st))//(inode->i_size)<sizeof(st)
    {
        //st.bts = timeNow;
        //st.OldSOC = real_capacity;
        //st.DetSOC = 0;
        //st.AlRunFlag = -1; 
        FG_CW2015_ERR("cw2015_file_test  file size error!\n");
		set_fs(old_fs);
        filp_close(file,NULL);
        file = NULL;
		return real_capacity;
    }
    else
    {
        file->f_pos = 0;
        vfs_read(file,(char*)&st,sizeof(st),&file->f_pos);
    }

    if (((st.bts)<0)||(st.OldSOC>100)||(st.OldSOC<0)||(st.DetSOC>16)||(st.DetSOC<-1))
    {
        FG_CW2015_ERR("cw2015_file_test  reading file error!\n"); 
        FG_CW2015_ERR("cw2015_file_test  st.bts = %ld st.OldSOC = %d st.DetSOC = %d st.AlRunFlag = %d  vmSOC = %d  2015SOC=%d\n",st.bts,st.OldSOC,st.DetSOC,st.AlRunFlag,vmSOC,real_capacity); 
  
        set_fs(old_fs);
        filp_close(file,NULL);
        file = NULL;
        return real_capacity;
    }
   
    
    if (PowerResetFlag == 1)
    {
        PowerResetFlag = -1;
        st.DetSOC = st.OldSOC - real_capacity;

        if (AlgNeed(cw_bat ,st.DetSOC,st.OldSOC)==1)
            st.AlRunFlag = 1;  
        else
            st.AlRunFlag = -1;

        if ((real_capacity<100)&&(real_capacity>CPSOC))
        {
            st.AlRunFlag = 1;
            vmSOC = 100;
            st.DetSOC = 100 - real_capacity;
        }
        FG_CW2015_ERR("cw2015_file_test  PowerResetFlag == 1!\n");
    } else if ((st.AlRunFlag == 1) && (st.DetSOC != 0)) {
        utemp1 = 32768/(st.DetSOC);
        if ((st.bts)<timeNow)
            utemp = cw_convertData(cw_bat,(timeNow-st.bts));   
        else
            utemp = cw_convertData(cw_bat,1); 
        FG_CW2015_ERR("cw2015_file_test  convertdata = %d\n",utemp);
        if ((st.DetSOC)<0)
            vmSOC = real_capacity-(int)((((unsigned int)((st.DetSOC)*(-1))*utemp)+utemp1)/65536);
        else
            vmSOC = real_capacity+(int)((((unsigned int)(st.DetSOC)*utemp)+utemp1)/65536);
        if (vmSOC == real_capacity)
        { 
             st.AlRunFlag = -1;
             FG_CW2015_ERR("cw2015_file_test  algriothm end\n");
        }
    } else {
        count_sp = NORMAL_CYCLE;
        st.AlRunFlag = -1;
        st.bts = timeNow;
        FG_CW2015_ERR("cw2015_file_test  no algriothm\n");
    }
    alg_run_flag = st.AlRunFlag;
    if (vmSOC > 100)
        vmSOC = 100;
    else if (vmSOC < 0)
        vmSOC = 0;
    st.OldSOC = vmSOC;
    file->f_pos = 0;
    vfs_write(file,(char*)&st,sizeof(st),&file->f_pos);
    set_fs(old_fs);
    filp_close(file,NULL);
    file = NULL;
    return vmSOC;
}
#endif


static int cw_read(struct i2c_client *client, u8 reg, u8 buf[])
{
    int ret = i2c_smbus_read_byte_data(client,reg);
    printk("cw_read buf = %d",ret);
    if (ret >= 0) {
        buf[0] = ret;
        ret = 0;
    }
    return ret;
}

static int cw_write(struct i2c_client *client, u8 reg, u8 const buf[])
{
    int ret = i2c_smbus_write_byte_data(client,reg,buf[0]);
    return ret;
}

static int cw_read_word(struct i2c_client *client, u8 reg, u8 buf[])
{
    unsigned int data = i2c_smbus_read_word_data(client, reg);
    buf[0] = data & 0x00FF;
    buf[1] = (data & 0xFF00)>>8;
    return 0;
}

static int cw2015_battery_version;

static void cw2015_get_battery_version(void)
{
    int i = simple_strtol(strstr(saved_command_line, "batversion=")+11, 0, 10);
    cw2015_battery_version = i; //COS = 1, DES = 2
    printk("cw2015_battery_version = %d\n", cw2015_battery_version);
}

static int cw_update_config_info(struct cw2015_battery *cw_bat)
{
    int ret;
    u8 reg_val;
    int i;
    u8 reset_val;
#ifdef FG_CW2015_DEBUG
    FG_CW2015_LOG("func: %s-------\n", __func__);
#endif        
    FG_CW2015_LOG("test cw_bat_config_info = 0x%x",config_info_cos[0]);
    /* make sure no in sleep mode */
    ret = cw_read(cw_bat->client, 10, &reg_val);
    FG_CW2015_LOG("cw_update_config_info reg_val = 0x%x",reg_val);
    if (ret < 0)
        return ret;
    reset_val = reg_val;
    if ((reg_val & MODE_SLEEP) == MODE_SLEEP) {
#ifdef FG_CW2015_DEBUG
        FG_CW2015_ERR("Error, device in sleep mode, cannot update battery info\n");
#endif
        return -1;
    }

    /* update new battery info */
    for (i = 0; i < 64; i++) {
        if (cw2015_battery_version == 2)
            ret = cw_write(cw_bat->client, 16 + i, &config_info_des[i]);
        else
            ret = cw_write(cw_bat->client, 16 + i, &config_info_cos[i]);
        if (ret < 0)
            return ret;
    }

    /* readback & check */
    for (i = 0; i < 64; i++) {
        ret = cw_read(cw_bat->client, 16 + i, &reg_val);
        if (cw2015_battery_version == 2) {
            if (reg_val != config_info_des[i])
                return -1;
        } else if (reg_val != config_info_cos[i])
            return -1;
    }
        
    /* set cw2015/cw2013 to use new battery info */
    ret = cw_read(cw_bat->client, 8, &reg_val);
    if (ret < 0)
        return ret;

    reg_val |= CONFIG_UPDATE_FLG;   /* set UPDATE_FLAG */
    reg_val &= 0x07;                /* clear ATHD */
    reg_val |= ATHD;                /* set ATHD */
    ret = cw_write(cw_bat->client, 8, &reg_val);
    if (ret < 0)
        return ret;

    /* check 2015/cw2013 for ATHD & update_flag */ 
    ret = cw_read(cw_bat->client, 8, &reg_val);
    if (ret < 0)
        return ret;

    if (!(reg_val & CONFIG_UPDATE_FLG))
#ifdef FG_CW2015_DEBUG
        FG_CW2015_LOG("update flag for new battery info have not set..\n");
#endif

    if ((reg_val & 0xf8) != ATHD)
#ifdef FG_CW2015_DEBUG
        FG_CW2015_LOG("the new ATHD have not set..\n");
#endif

    /* reset */
    reset_val &= ~(MODE_RESTART);
    reg_val = reset_val | MODE_RESTART;
    ret = cw_write(cw_bat->client, 10, &reg_val);
    if (ret < 0)
        return ret;

    msleep(10);
    ret = cw_write(cw_bat->client, 10, &reset_val);
    if (ret < 0)
        return ret;
#ifdef BAT_CHANGE_ALGORITHM
    PowerResetFlag = 1;
    FG_CW2015_ERR("cw2015_file_test  set PowerResetFlag/n ");
#endif
    msleep(10);
    return 0;
}

static int cw_init(struct cw2015_battery *cw_bat)
{
    int ret;
    int i;
    u8 reg_val = MODE_SLEEP;
    
    cw2015_get_battery_version();

    if ((reg_val & MODE_SLEEP) == MODE_SLEEP) {
        reg_val = MODE_NORMAL;
        ret = cw_write(cw_bat->client, 10, &reg_val);
        if (ret < 0) 
            return ret;
    }

    ret = cw_read(cw_bat->client, 8, &reg_val);
    if (ret < 0)
        return ret;
    FG_CW2015_LOG("the new ATHD have not set reg_val = 0x%x\n",reg_val);
    if ((reg_val & 0xf8) != ATHD) {
#ifdef FG_CW2015_DEBUG
        FG_CW2015_LOG("the new ATHD have not set\n");
#endif
        reg_val &= 0x07;    /* clear ATHD */
        reg_val |= ATHD;    /* set ATHD */
        ret = cw_write(cw_bat->client, 8, &reg_val);
        FG_CW2015_LOG("cw_init 1111\n");
        if (ret < 0)
            return ret;
    }

    ret = cw_read(cw_bat->client, 8, &reg_val);
    if (ret < 0) 
        return ret;
    FG_CW2015_LOG("cw_init 8 = %d\n",reg_val);

    if (!(reg_val & CONFIG_UPDATE_FLG)) {
#ifdef FG_CW2015_DEBUG
        FG_CW2015_LOG("update flag for new battery info have not set\n");
#endif
        ret = cw_update_config_info(cw_bat);
        if (ret < 0)
            return ret;
    } else {
        for(i = 0; i < 64; i++) { 
            ret = cw_read(cw_bat->client, (16 + i), &reg_val);
            if (ret < 0)
                return ret;
            if (cw2015_battery_version == 2) {
                if (config_info_des[i] != reg_val)
                    break;
            } else if (config_info_cos[i] != reg_val)
                break;
        }

        if (i != 64) {
#ifdef FG_CW2015_DEBUG
            FG_CW2015_LOG("update flag for new battery info have not set\n"); 
#endif
            ret = cw_update_config_info(cw_bat);
            if (ret < 0)
                return ret;
        }
    }

    for (i = 0; i < 30; i++) {
        ret = cw_read(cw_bat->client, 4, &reg_val);
        if (ret < 0)
            return ret;
        else if (reg_val <= 0x64) 
            break;

        msleep(100);
        if (i > 25)
#ifdef FG_CW2015_DEBUG
            FG_CW2015_ERR("cw2015/cw2013 input unvalid power error\n");
#endif
    }

    if (i >= 30) {
        reg_val = MODE_SLEEP;
        ret = cw_write(cw_bat->client, 10, &reg_val);
#ifdef FG_CW2015_DEBUG
        FG_CW2015_ERR("cw2015/cw2013 input unvalid power error_2\n");
#endif
        return -1;
    } 
    return 0;
}

static void cw_update_time_member_charge_start(struct cw2015_battery *cw_bat)
{
    struct timespec ts;
    int new_run_time;
    int new_sleep_time;

    ktime_get_ts(&ts);
    new_run_time = ts.tv_sec;

    get_monotonic_boottime(&ts);
    new_sleep_time = ts.tv_sec - new_run_time;

    cw_bat->run_time_charge_start = new_run_time;
    cw_bat->sleep_time_charge_start = new_sleep_time; 
}

static void cw_update_time_member_capacity_change(struct cw2015_battery *cw_bat)
{
    struct timespec ts;
    int new_run_time;
    int new_sleep_time;

    ktime_get_ts(&ts);
    new_run_time = ts.tv_sec;

    get_monotonic_boottime(&ts);
    new_sleep_time = ts.tv_sec - new_run_time;

    cw_bat->run_time_capacity_change = new_run_time;
    cw_bat->sleep_time_capacity_change = new_sleep_time; 
}

static int cw_get_capacity(struct cw2015_battery *cw_bat)
{
    int cw_capacity;
    int ret;
    u8 reg_val[2];

    struct timespec ts;
    long new_run_time;
    long new_sleep_time;
    long capacity_or_aconline_time;
    int allow_change;
    int allow_capacity;
    static int if_quickstart = 0;
    static int jump_flag =0;
    static int reset_loop =0;
    int charge_time;
    u8 reset_val;

    ret = cw_read_word(cw_bat->client, 4, reg_val);
    if (ret < 0)
        return ret;
    FG_CW2015_LOG("cw_get_capacity cw_capacity_0 = %d,cw_capacity_1 = %d\n",reg_val[0],reg_val[1]);
    cw_capacity = reg_val[0];
    if ((cw_capacity < 0) || (cw_capacity > 100)) {
#ifdef FG_CW2015_DEBUG
        FG_CW2015_ERR("get cw_capacity error; cw_capacity = %d\n", cw_capacity);
#endif
        reset_loop++;
        if (reset_loop > 5) {
            reset_val = MODE_SLEEP;               
            ret = cw_write(cw_bat->client, 10, &reset_val);
            if (ret < 0)
                return ret;
            reset_val = MODE_NORMAL;
            msleep(10);
            ret = cw_write(cw_bat->client, 10, &reset_val);
            if (ret < 0)
                return ret;
            ret = cw_init(cw_bat);
            if (ret) 
                return ret;
                reset_loop =0;               
            }
            return cw_capacity;
        } else {
            reset_loop =0;
        }

        if (cw_capacity == 0) 
#ifdef FG_CW2015_DEBUG
                FG_CW2015_LOG("the cw201x capacity is 0 !!!!!!!, funciton: %s, line: %d\n", __func__, __LINE__);
#endif
        else 
#ifdef FG_CW2015_DEBUG
                FG_CW2015_LOG("the cw201x capacity is %d, funciton: %s\n", cw_capacity, __func__);
#endif

#ifdef BAT_CHANGE_ALGORITHM
        cw_capacity = cw_algorithm(cw_bat,cw_capacity);
#endif
        ktime_get_ts(&ts);
        new_run_time = ts.tv_sec;

        get_monotonic_boottime(&ts);
        new_sleep_time = ts.tv_sec - new_run_time;
        printk("cw_get_capacity cw_bat->charger_mode = %d\n",cw_bat->charger_mode);

        if (((cw_bat->charger_mode > 0) && (cw_capacity <= (cw_bat->capacity - 1)) && (cw_capacity > (cw_bat->capacity - 9)))
                        || ((cw_bat->charger_mode == 0) && (cw_capacity == (cw_bat->capacity + 1)))) {             // modify battery level swing

                if (!(cw_capacity == 0 && cw_bat->capacity <= 2)) {			
		                    cw_capacity = cw_bat->capacity;
		            }
				}        

        if ((cw_bat->charger_mode > 0) && (cw_capacity >= 95) && (cw_capacity <= cw_bat->capacity)) {     // avoid no charge full

                capacity_or_aconline_time = (cw_bat->sleep_time_capacity_change > cw_bat->sleep_time_charge_start) ? cw_bat->sleep_time_capacity_change : cw_bat->sleep_time_charge_start;
                capacity_or_aconline_time += (cw_bat->run_time_capacity_change > cw_bat->run_time_charge_start) ? cw_bat->run_time_capacity_change : cw_bat->run_time_charge_start;
                allow_change = (new_sleep_time + new_run_time - capacity_or_aconline_time) / BATTERY_UP_MAX_CHANGE;
                if (allow_change > 0) {
                        allow_capacity = cw_bat->capacity + allow_change; 
                        cw_capacity = (allow_capacity <= 100) ? allow_capacity : 100;
                        jump_flag =1;
                } else if (cw_capacity <= cw_bat->capacity) {
                        cw_capacity = cw_bat->capacity; 
                }

        }
       
        else if ((cw_bat->charger_mode == 0) && (cw_capacity <= cw_bat->capacity ) && (cw_capacity >= 90) && (jump_flag == 1)) {     // avoid battery level jump to CW_BAT
                capacity_or_aconline_time = (cw_bat->sleep_time_capacity_change > cw_bat->sleep_time_charge_start) ? cw_bat->sleep_time_capacity_change : cw_bat->sleep_time_charge_start;
                capacity_or_aconline_time += (cw_bat->run_time_capacity_change > cw_bat->run_time_charge_start) ? cw_bat->run_time_capacity_change : cw_bat->run_time_charge_start;
                allow_change = (new_sleep_time + new_run_time - capacity_or_aconline_time) / BATTERY_DOWN_CHANGE;
                if (allow_change > 0) {
                        allow_capacity = cw_bat->capacity - allow_change; 
                        if (cw_capacity >= allow_capacity){
                        	jump_flag =0;
                        }
                        else{
                                cw_capacity = (allow_capacity <= 100) ? allow_capacity : 100;
                        }
                } else if (cw_capacity <= cw_bat->capacity) {
                        cw_capacity = cw_bat->capacity;
                }
        }
				
				if ((cw_capacity == 0) && (cw_bat->capacity > 1)) {              // avoid battery level jump to 0% at a moment from more than 2%
                allow_change = ((new_run_time - cw_bat->run_time_capacity_change) / BATTERY_DOWN_MIN_CHANGE_RUN);
                allow_change += ((new_sleep_time - cw_bat->sleep_time_capacity_change) / BATTERY_DOWN_MIN_CHANGE_SLEEP);

                allow_capacity = cw_bat->capacity - allow_change;
                cw_capacity = (allow_capacity >= cw_capacity) ? allow_capacity: cw_capacity;
#ifdef FG_CW2015_DEBUG
                FG_CW2015_LOG("report GGIC POR happened\n");
#else
                //dev_info(&cw_bat->client->dev, "report GGIC POR happened");
#endif

                reset_val = MODE_SLEEP;               
                ret = cw_write(cw_bat->client, 10, &reset_val);
                if (ret < 0)
                    return ret;
                reset_val = MODE_NORMAL;
                msleep(10);
                ret = cw_write(cw_bat->client, 10, &reset_val);
                if (ret < 0)
                    return ret;
                                              
                ret = cw_init(cw_bat);
                   if (ret) 
                    return ret;
                                               
        }
 
#if 1	
	if ((cw_bat->charger_mode > 0) &&(cw_capacity == 0))
	{		  
                charge_time = new_sleep_time + new_run_time - cw_bat->sleep_time_charge_start - cw_bat->run_time_charge_start;
                if ((charge_time > BATTERY_DOWN_MAX_CHANGE_RUN_AC_ONLINE) && (if_quickstart == 0)) {
        		      reset_val = MODE_SLEEP;               
                ret = cw_write(cw_bat->client, 10, &reset_val);
                if (ret < 0)
                    return ret;
                reset_val = MODE_NORMAL;
                msleep(10);
                ret = cw_write(cw_bat->client, 10, &reset_val);
                if (ret < 0)
                    return ret;
                                              
                ret = cw_init(cw_bat);
                   if (ret) 
                    return ret;      // if the cw_capacity = 0 the cw2015 will qstrt
#ifdef FG_CW2015_DEBUG
        		      FG_CW2015_LOG("report battery capacity still 0 if in changing\n");
#else
        		      //dev_info(&cw_bat->client->dev, "report battery capacity still 0 if in changing");
#endif
                        if_quickstart = 1;
                }
	} else if ((if_quickstart == 1)&&(cw_bat->charger_mode == 0)) {
    		if_quickstart = 0;
        }

#endif

        return cw_capacity;
}

static int cw_get_vol(struct cw2015_battery *cw_bat)
{
    int ret;
    u8 reg_val[2];
    u16 value16, value16_1, value16_2, value16_3;
    int voltage;
    ret = cw_read_word(cw_bat->client, 2, reg_val);
    if (ret < 0) {
        return ret;
    }
    value16 = (reg_val[0] << 8) + reg_val[1];

    ret = cw_read_word(cw_bat->client, 2, reg_val);
    if (ret < 0) {
        return ret;
    }
    value16_1 = (reg_val[0] << 8) + reg_val[1];

    ret = cw_read_word(cw_bat->client, 2, reg_val);
    if (ret < 0) {
        return ret;
    }
    value16_2 = (reg_val[0] << 8) + reg_val[1];

    if (value16 > value16_1) {
        value16_3 = value16;
        value16 = value16_1;
        value16_1 = value16_3;
    }

    if (value16_1 > value16_2) {
        value16_3 = value16_1;
        value16_1 = value16_2;
        value16_2 = value16_3;
    }

    if (value16 > value16_1) {
        value16_3 = value16;
        value16 = value16_1;
        value16_1 = value16_3;
    }

    voltage = value16_1 * 312 / 1024;
    FG_CW2015_LOG("cw_get_vol voltage = %d\n",voltage);
    return voltage;
}

static void rk_bat_update_capacity(struct cw2015_battery *cw_bat)
{
    int cw_capacity = cw_get_capacity(cw_bat);
#ifdef BAT_CHANGE_ALGORITHM
    FG_CW2015_ERR("cw2015_file_test userdata, %ld, %d, %d\n",get_seconds(), cw_capacity, cw_bat->voltage);
#endif
    if ((cw_capacity >= 0) && (cw_capacity <= 100) && (cw_bat->capacity != cw_capacity)) {
        cw_bat->capacity = cw_capacity;
        cw_bat->bat_change = 1;
        cw_update_time_member_capacity_change(cw_bat);

        if (cw_bat->capacity == 0)
#ifdef FG_CW2015_DEBUG
            FG_CW2015_LOG("report battery capacity 0 and will shutdown if no changing\n");
#endif
    }
    FG_CW2015_LOG("rk_bat_update_capacity cw_capacity = %d\n",cw_bat->capacity);
}

static void rk_bat_update_vol(struct cw2015_battery *cw_bat)
{
    int ret = cw_get_vol(cw_bat);
    if ((ret >= 0) && (cw_bat->voltage != ret)) {
        cw_bat->voltage = ret;
        cw_bat->bat_change = 1;
    }
}

static int get_usb_charge_state(struct cw2015_battery *cw_bat)
{
    int usb_status = 0;
    FG_CW2015_LOG("get_usb_charge_state cw2015_charging_type = %d\n",cw2015_charging_type);
    if (cw2015_charging_status == 0) {
        usb_status = 0;
        cw_bat->charger_mode = 0;
    } else {
        if (cw2015_charging_type == STANDARD_HOST) {
            usb_status = 1;
            cw_bat->charger_mode = USB_CHARGER_MODE;
        } else if (cw2015_charging_type == STANDARD_CHARGER) {
            usb_status = 2;
            cw_bat->charger_mode = AC_CHARGER_MODE;
        }
    }
    FG_CW2015_LOG("get_usb_charge_state usb_status = %d,cw2015_charging_status = %d\n",usb_status,cw2015_charging_status);
    return usb_status;
}

static int rk_usb_update_online(struct cw2015_battery *cw_bat)
{
    int ret = 0;
    int usb_status = 0;

    FG_CW2015_LOG("rk_usb_update_online cw2015_charging_status = %d\n",cw2015_charging_status);

    usb_status = get_usb_charge_state(cw_bat);        
    if (usb_status == 2) {
        if (cw_bat->charger_mode != AC_CHARGER_MODE) {
            cw_bat->charger_mode = AC_CHARGER_MODE;
            ret = 1;
        }
        if (cw_bat->usb_online != 1) {
            cw_bat->usb_online = 1;
            cw_update_time_member_charge_start(cw_bat);
        }
    } else if (usb_status == 1) {
        if (cw_bat->charger_mode != USB_CHARGER_MODE) {
            cw_bat->charger_mode = USB_CHARGER_MODE;
            ret = 1;
        }
        if (cw_bat->usb_online != 1){
            cw_bat->usb_online = 1;
            cw_update_time_member_charge_start(cw_bat);
        }
    } else if (usb_status == 0 && cw_bat->usb_online != 0) {
        cw_bat->charger_mode = 0;
        cw_update_time_member_charge_start(cw_bat);
        cw_bat->usb_online = 0;
        ret = 1;
    }

    return ret;
}

static void cw_bat_work(struct work_struct *work)
{
    struct delayed_work *delay_work;
    struct cw2015_battery *cw_bat;
    int ret;

    FG_CW2015_FUN(); 
    printk("cw_bat_work\n");

    delay_work = container_of(work, struct delayed_work, work);
    cw_bat = container_of(delay_work, struct cw2015_battery, battery_delay_work);

    if (cw_bat->usb_online == 1)
        ret = rk_usb_update_online(cw_bat);

    rk_bat_update_capacity(cw_bat);
    rk_bat_update_vol(cw_bat);
    cw2015_capacity = cw_bat->capacity;
    cw2015_voltage = cw_bat->voltage;
    printk("cw_bat_work 777 vol = %d,cap = %d\n",cw_bat->voltage,cw_bat->capacity);
    if (cw_bat->bat_change)
        cw_bat->bat_change = 0;
    queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(1000));
}

static int cw2015_i2c_detect(struct i2c_client *client, struct i2c_board_info *info) 
{    
    FG_CW2015_FUN();
    strcpy(info->type, CW2015_DEV_NAME);
    return 0;
}

static ssize_t file_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d", 1);
}

static DEVICE_ATTR(file_state, S_IRUGO, file_state_show, NULL);

static int cw2015_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct cw2015_battery *cw_bat;
    int ret;

    FG_CW2015_FUN(); 
    mt_set_gpio_mode(GPIO_I2C4_SDA_PIN, GPIO_I2C4_SDA_PIN_M_SDA);
    mt_set_gpio_mode(GPIO_I2C4_SCA_PIN, GPIO_I2C4_SCA_PIN_M_SCL);
    cw_bat = kzalloc(sizeof(struct cw2015_battery), GFP_KERNEL);
    if (!cw_bat) {
#ifdef FG_CW2015_DEBUG
        FG_CW2015_ERR("fail to allocate memory\n");
#endif
        return -ENOMEM;
    }

    i2c_set_clientdata(client, cw_bat);
    cw_bat->client = client;

    ret = cw_init(cw_bat);
    if (ret) 
        return ret;

    cw_bat->usb_online = 0;
    cw_bat->charger_mode = 0;
    cw_bat->capacity = 1;
    cw_bat->voltage = 0;
    cw_bat->bat_change = 0;

    cw_update_time_member_capacity_change(cw_bat);
    cw_update_time_member_charge_start(cw_bat);
    device_create_file(cw_bat->client, &dev_attr_file_state);

    cw_bat->battery_workqueue = create_singlethread_workqueue("rk_battery");
    INIT_DELAYED_WORK(&cw_bat->battery_delay_work, cw_bat_work);
    queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(10));

#ifdef FG_CW2015_DEBUG
    FG_CW2015_LOG("cw2015/cw2013 driver v1.2 probe sucess\n");
#endif
    return 0;
}

static int cw2015_i2c_remove(struct i2c_client *client)
{
    struct cw2015_battery *data = i2c_get_clientdata(client);
    FG_CW2015_FUN();
    cancel_delayed_work(&data->battery_delay_work);
    client = NULL;
    i2c_unregister_device(client);
    kfree(data);
    return 0;
}

static int cw2015_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
    struct cw2015_battery *cw_bat = i2c_get_clientdata(client);
    FG_CW2015_FUN(); 
    cancel_delayed_work(&cw_bat->battery_delay_work);
    return 0;
}

static int cw2015_i2c_resume(struct i2c_client *client)
{
    struct cw2015_battery *cw_bat = i2c_get_clientdata(client);
    FG_CW2015_FUN(); 
    queue_delayed_work(cw_bat->battery_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(100));
    return 0;
}

static struct i2c_driver cw2015_i2c_driver = {
    .probe = cw2015_i2c_probe,
    .remove = cw2015_i2c_remove,
    .detect = cw2015_i2c_detect,
    .suspend = cw2015_i2c_suspend,
    .resume = cw2015_i2c_resume,
    .id_table = cw2015_i2c_id,
    .driver = {
        .name = CW2015_DEV_NAME
    }
};

static int __init cw_bat_init(void)
{    
    i2c_register_board_info(4, &cw2015_i2c, 1);
    printk("cw_bat_init\n");
    if (i2c_add_driver(&cw2015_i2c_driver)) {
        printk("cw_bat_init add driver error\n");
        return -1;
    }
    return 0;
}

static void __exit cw_bat_exit(void)
{
    i2c_del_driver(&cw2015_i2c_driver);
}

module_init(cw_bat_init);
module_exit(cw_bat_exit);

MODULE_AUTHOR("xhc<xhc@rock-chips.com>");
MODULE_DESCRIPTION("cw2015/cw2013 battery driver");
MODULE_LICENSE("GPL");

