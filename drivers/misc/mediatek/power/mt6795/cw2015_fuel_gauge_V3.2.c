#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/slab.h>

#include <cust_charging.h>
#include <mach/charging.h>
#include <mach/upmu_common.h>
#include <mach/battery_meter.h>
#include <mach/battery_common.h>
//#include <mach/mt_pmic.h>
#include <mach/mt_gpio.h>


#define CWFG_ENABLE_LOG 0 //CHANGE   Customer need to change this for enable/disable log
#define I2C_BUSNUM              4
/*
#define USB_CHARGING_FILE "/sys/class/power_supply/usb/online" // Chaman
#define DC_CHARGING_FILE "/sys/class/power_supply/ac/online"
*/
#define queue_delayed_work_time  8000
#define CW_PROPERTIES "cw-bat"

#define REG_VERSION             0x0
#define REG_VCELL               0x2
#define REG_SOC                 0x4
#define REG_RRT_ALERT           0x6
#define REG_CONFIG              0x8
#define REG_MODE                0xA
#define REG_VTEMPL              0xC
#define REG_VTEMPH              0xD
#define REG_BATINFO             0x10

#define MODE_SLEEP_MASK         (0x3<<6)
#define MODE_SLEEP              (0x3<<6)
#define MODE_NORMAL             (0x0<<6)
#define MODE_QUICK_START        (0x3<<4)
#define MODE_RESTART            (0xf<<0)

#define CONFIG_UPDATE_FLG       (0x1<<1)
#define ATHD                    (0x0<<3)        // ATHD = 0%

#define BATTERY_UP_MAX_CHANGE   420*1000            // The time for add capacity when charging
#define BATTERY_DOWN_MAX_CHANGE 120*1000
#define BATTERY_JUMP_TO_ZERO    30*1000
#define BATTERY_CAPACITY_ERROR  40*1000
#define BATTERY_CHARGING_ZERO   1800*1000

#define CHARGING_ON 1
#define NO_CHARGING 0


#define cw_printk(fmt, arg...)        \
	({                                    \
		if(CWFG_ENABLE_LOG){              \
			printk("FG_CW2015 : %s : " fmt, __FUNCTION__ ,##arg);  \
		}else{}                           \
	})     //need check by Chaman


#define CW2015_DEV_NAME         "CW2015"
#define SIZE_BATINFO    64

static const struct i2c_device_id CW2015_i2c_id[] = {{CW2015_DEV_NAME,0},{}};
static struct i2c_board_info __initdata i2c_CW2015 = { I2C_BOARD_INFO("CW2015", 0x62)};

static u8 config_info[SIZE_BATINFO] = {
    0x17, 0xF3, 0x63, 0x6A, 0x6A, 0x68, 0x68, 0x65, 0x63, 0x60, 
    0x5B, 0x59, 0x65, 0x5B, 0x46, 0x41, 0x36, 0x31, 0x28, 0x27, 
    0x31, 0x35, 0x43, 0x51, 0x1C, 0x3B, 0x0B, 0x85, 0x22, 0x42, 
    0x5B, 0x82, 0x99, 0x92, 0x98, 0x96, 0x3D, 0x1A, 0x66, 0x45, 
    0x0B, 0x29, 0x52, 0x87, 0x8F, 0x91, 0x94, 0x52, 0x82, 0x8C, 
    0x92, 0x96, 0x54, 0xC2, 0xBA, 0xCB, 0x2F, 0x7D, 0x72, 0xA5, 
    0xB5, 0xC1, 0xA5, 0x49
};
static u8 config_info_des[SIZE_BATINFO] = {
    0x17, 0xF9, 0x6D, 0x6D, 0x6B, 0x67, 0x65, 0x64, 0x58, 0x6D, 
    0x6D, 0x48, 0x57, 0x5D, 0x4A, 0x43, 0x37, 0x31, 0x2B, 0x20, 
    0x24, 0x35, 0x44, 0x55, 0x20, 0x37, 0x0B, 0x85, 0x2A, 0x4A, 
    0x56, 0x68, 0x74, 0x6B, 0x6D, 0x6E, 0x3C, 0x1A, 0x5C, 0x45, 
    0x0B, 0x30, 0x52, 0x87, 0x8F, 0x91, 0x94, 0x52, 0x82, 0x8C, 
    0x92, 0x96, 0x64, 0xB4, 0xDB, 0xCB, 0x2F, 0x7D, 0x72, 0xA5, 
    0xB5, 0xC1, 0xA5, 0x42
};

static struct power_supply *chrg_usb_psy;
static struct power_supply *chrg_ac_psy;
static struct i2c_client *cw_client;

#ifdef CONFIG_PM
static struct timespec suspend_time_before;
static struct timespec after;
static int suspend_resume_mark = 0;
#endif

struct cw_bat_platform_data {
    int (*io_init)(void);

    int is_usb_charge;
    int is_dc_charge;
    u8 *cw_bat_config_info;
    u32 irq_flags;
    u32 dc_det_pin;
    u32 dc_det_level;

    u32 bat_low_pin;
    u32 bat_low_level;
    u32 chg_ok_pin;
    u32 chg_ok_level;
    u32 chg_mode_sel_pin;
    u32 chg_mode_sel_level;
};

static struct cw_bat_platform_data cw_bat_platdata = {    
    .bat_low_pin    = 0,
    .bat_low_level  = 0,   
    .chg_ok_pin   = 0,
    .chg_ok_level = 0,
    
    .is_usb_charge = 0,    
    .cw_bat_config_info = config_info,
};

struct cw_battery {
    struct i2c_client *client;
    struct workqueue_struct *cwfg_workqueue;
    struct delayed_work battery_delay_work;
    struct power_supply cw_bat;
    struct cw_bat_platform_data *plat_data;

    int charger_mode;
    int capacity;
    int voltage;
    int status;
    int time_to_empty;
	int change;
    //int alt;
};

int g_cw2015_capacity = 0;
int g_cw2015_vol = 0;

#define CONFIG_PM
static int liuchao_test_hmi_battery_version;
extern char* saved_command_line;

static void hmi_get_battery_version(void)
{
    int i; 
    printk("12345 %s\n", saved_command_line); 
    printk("12345 %d\n", strstr(saved_command_line, "batversion=")); 
    i = simple_strtol(strstr(saved_command_line, "batversion=")+11, 0, 10); 
    liuchao_test_hmi_battery_version = i; //COS = 1, DES = 2 
    printk("liuchao_test_hmi_battery_version %d\n", liuchao_test_hmi_battery_version); 
}

/*Define CW2015 iic read function*/
int cw_read(struct i2c_client *client, unsigned char reg, unsigned char buf[])
{
	int ret = 0;
	msleep(10);	
	ret = i2c_smbus_read_i2c_block_data( client, reg, 1, buf);
	cw_printk("%2x = %2x\n", reg, buf[0]);
	return ret;
}
/*Define CW2015 iic write function*/		
int cw_write(struct i2c_client *client, unsigned char reg, unsigned char const buf[])
{
	int ret = 0;
	msleep(10);	
	ret = i2c_smbus_write_i2c_block_data( client, reg, 1, &buf[0] );
	cw_printk("%2x = %2x\n", reg, buf[0]);
	return ret;
}
/*Define CW2015 iic read word function*/	
int cw_read_word(struct i2c_client *client, unsigned char reg, unsigned char buf[])
{
	int ret = 0;
	msleep(10);	
	ret = i2c_smbus_read_i2c_block_data( client, reg, 2, buf );
	cw_printk("%2x = %2x %2x\n", reg, buf[0], buf[1]);
	return ret;
}

int cw2015_read_version(void)
{
	int i=0,ret;
         u8 reg_val;
	while(i++ <4)
	{
		ret=cw_read(cw_client, 0, &reg_val);
		if(ret >0)break;//return reg_val;
	}
	//printk("ppppppppp--%d\n",reg_val);
	return reg_val;
}
int cw2015_read_temp(void)
{
	int i=0,ret;
         u8 reg_templ,reg_temph;
	while(i++ <4)
	{
		ret=cw_read(cw_client, REG_VTEMPL, &reg_templ);
		ret=cw_read(cw_client, REG_VTEMPH, &reg_temph);
		if(ret >0)break;//return reg_val;
	}
	 ret=(25-((reg_temph*256+reg_templ)-3010)/8);
	//printk("ppppppppp==%d\n",ret);
	return ret;
}

/*CW2015 update profile function, Often called during initialization*/
int cw_update_config_info(struct cw_battery *cw_bat)
{
    int ret;
    unsigned char reg_val;
    int i;
    unsigned char reset_val;

    cw_printk("\n");
    cw_printk("[FGADC] test config_info = 0x%x\n",config_info[0]);

    
    // make sure no in sleep mode
    ret = cw_read(cw_bat->client, REG_MODE, &reg_val);
    printk("cw_update_config_info reg_val = 0x%x",reg_val);
    if(ret < 0) {
        return ret;
    }

    reset_val = reg_val;
    if((reg_val & MODE_SLEEP_MASK) == MODE_SLEEP) {
        return -1;
    }

    /* update new battery info */
    for (i = 0; i < SIZE_BATINFO; i++) 
    {
        printk("[CW2015] cw_bat->plat_data->cw_bat_config_info[%d] = 0x%x\n", i, \
                cw_bat->plat_data->cw_bat_config_info[i]);
        ret = cw_write(cw_bat->client, REG_BATINFO + i, &cw_bat->plat_data->cw_bat_config_info[i]);

        if (ret < 0)
            return ret;
    }

    /* readback & check */
    for (i = 0; i < SIZE_BATINFO; i++) 
    {
        ret = cw_read(cw_bat->client, REG_BATINFO + i, &reg_val);
        if (reg_val != cw_bat->plat_data->cw_bat_config_info[i])
            return -EPERM;
    }

    reg_val |= CONFIG_UPDATE_FLG;   // set UPDATE_FLAG
    reg_val &= 0x07;                // clear ATHD
    reg_val |= ATHD;                // set ATHD
    ret = cw_write(cw_bat->client, REG_CONFIG, &reg_val);
    if(ret < 0) 
		return ret;
    // read back and check
    ret = cw_read(cw_bat->client, REG_CONFIG, &reg_val);
    if(ret < 0) {
        return ret;
    }

    if (!(reg_val & CONFIG_UPDATE_FLG)) {
		printk("Error: The new config set fail\n");
		//return -1;
    }

    if ((reg_val & 0xf8) != ATHD) {
		printk("Error: The new ATHD set fail\n");
		//return -1;
    }

    // reset
    reset_val &= ~(MODE_RESTART);
    reg_val = reset_val | MODE_RESTART;
    ret = cw_write(cw_bat->client, REG_MODE, &reg_val);
    if(ret < 0) return ret;

    msleep(10);
    
    ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
    if(ret < 0) return ret;
	
	cw_printk("cw2015 update config success!\n");
	
    return 0;
}
/*CW2015 init function, Often called during initialization*/
static int cw_init(struct cw_battery *cw_bat)
{
    int ret;
    int i;
    unsigned char reg_val = MODE_SLEEP;
    hmi_get_battery_version();
	
    if ((reg_val & MODE_SLEEP_MASK) == MODE_SLEEP) {
        reg_val = MODE_NORMAL;
        ret = cw_write(cw_bat->client, REG_MODE, &reg_val);
        if (ret < 0) 
            return ret;
    }

    ret = cw_read(cw_bat->client, REG_CONFIG, &reg_val);
    if (ret < 0)
    	return ret;
    printk("the new ATHD have not set reg_val = 0x%x\n",reg_val);

    if ((reg_val & 0xf8) != ATHD) {
        reg_val &= 0x07;    /* clear ATHD */
        reg_val |= ATHD;    /* set ATHD */
        ret = cw_write(cw_bat->client, REG_CONFIG, &reg_val);
        if (ret < 0)
            return ret;
    }

    ret = cw_read(cw_bat->client, REG_CONFIG, &reg_val);
    if (ret < 0) 
        return ret;

    if (!(reg_val & CONFIG_UPDATE_FLG)) {
		cw_printk("update config flg is true, need update config\n");
        ret = cw_update_config_info(cw_bat);
        if (ret < 0) {
			printk("%s : update config fail\n", __func__);
            return ret;
        }
    } else {
    for(i = 0; i < SIZE_BATINFO; i++) { 
        ret = cw_read(cw_bat->client, (REG_BATINFO + i), &reg_val);
        if (ret < 0)
            return ret;
    }        
    
    if (i != SIZE_BATINFO) {
        #ifdef FG_CW2015_DEBUG
        FG_CW2015_LOG("update flag for new battery info have not set\n"); 
        #endif
        ret = cw_update_config_info(cw_bat);
        if (ret < 0)
            return ret;
    }
}

    for (i = 0; i < 30; i++) {
        ret = cw_read(cw_bat->client, REG_SOC, &reg_val);
        if (ret < 0)
            return ret;
        else if (reg_val <= 0x64) 
            break;
        msleep(120);
    }
	
    if (i >= 30 ){
    	 reg_val = MODE_SLEEP;
         ret = cw_write(cw_bat->client, REG_MODE, &reg_val);
         cw_printk("cw2015 input unvalid power error, cw2015 join sleep mode\n");
         return -1;
    } 

	cw_printk("cw2015 init success!\n");	
    return 0;
}

/*Functions:< check_chrg_usb_psy check_chrg_ac_psy get_chrg_psy get_charge_state > for Get Charger Status from outside*/
static int check_chrg_usb_psy(struct device *dev, void *data)
{
        struct power_supply *psy = dev_get_drvdata(dev);

        if (psy->type == POWER_SUPPLY_TYPE_USB) {
                chrg_usb_psy = psy;
                return 1;
        }
        return 0;
}

static int check_chrg_ac_psy(struct device *dev, void *data)
{
        struct power_supply *psy = dev_get_drvdata(dev);

        if (psy->type == POWER_SUPPLY_TYPE_MAINS) {
                chrg_ac_psy = psy;
                return 1;
        }
        return 0;
}

static void get_chrg_psy(void)
{
	if(!chrg_usb_psy)
		class_for_each_device(power_supply_class, NULL, NULL, check_chrg_usb_psy);
	if(!chrg_ac_psy)
		class_for_each_device(power_supply_class, NULL, NULL, check_chrg_ac_psy);
}

static int get_charge_state(void)
{
        union power_supply_propval val;
        int ret = -ENODEV;
		int usb_online = 0;
		int ac_online = 0;

        if (!chrg_usb_psy || !chrg_ac_psy)
                get_chrg_psy();
			
        if(chrg_usb_psy) {
            ret = chrg_usb_psy->get_property(chrg_usb_psy, POWER_SUPPLY_PROP_ONLINE, &val);
            if (!ret)
                usb_online = val.intval;
        }
		if(chrg_ac_psy) {
            ret = chrg_ac_psy->get_property(chrg_ac_psy, POWER_SUPPLY_PROP_ONLINE, &val);
            if (!ret)
                ac_online = val.intval;			
		}
		if(!chrg_usb_psy){
			cw_printk("Usb online didn't find\n");
		}
		if(!chrg_ac_psy){
			cw_printk("Ac online didn't find\n");
		}
		cw_printk("ac_online = %d    usb_online = %d\n", ac_online, usb_online);
		if(ac_online || usb_online){
			return 1;
		}
        return 0;
}


static int cw_por(struct cw_battery *cw_bat)
{
	int ret;
	unsigned char reset_val;
	
	reset_val = MODE_SLEEP; 			  
	ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
	if (ret < 0)
		return ret;
	reset_val = MODE_NORMAL;
	msleep(10);
	ret = cw_write(cw_bat->client, REG_MODE, &reset_val);
	if (ret < 0)
		return ret;
	ret = cw_init(cw_bat);
	if (ret) 
		return ret;
	return 0;
}

static int cw_get_capacity(struct cw_battery *cw_bat)
{
	int cw_capacity;
	int ret;
	unsigned char reg_val[2];

	static int reset_loop = 0;
	static int charging_loop = 0;
	static int discharging_loop = 0;
	static int jump_flag = 0;
	static int charging_5_loop = 0;
	int sleep_cap = 0;
	
	ret = cw_read_word(cw_bat->client, REG_SOC, reg_val);
	if (ret < 0)
		return ret;

	cw_capacity = reg_val[0];

	if ((cw_capacity < 0) || (cw_capacity > 100)) {
		cw_printk("Error:  cw_capacity = %d\n", cw_capacity);
		reset_loop++;			
		if (reset_loop > (BATTERY_CAPACITY_ERROR / queue_delayed_work_time)){ 
			cw_por(cw_bat);
			reset_loop =0;							 
		}
								 
		return cw_bat->capacity; //cw_capacity Chaman change because I think customer didn't want to get error capacity.
	}else {
		reset_loop =0;
	}

	/* case 1 : aviod swing */
	if (((cw_bat->charger_mode > 0) && (cw_capacity <= (cw_bat->capacity - 1)) && (cw_capacity > (cw_bat->capacity - 9)))
					|| ((cw_bat->charger_mode == 0) && (cw_capacity == (cw_bat->capacity + 1)))) {
		if (!(cw_capacity == 0 && cw_bat->capacity <= 2)) { 		
			cw_capacity = cw_bat->capacity;
		}
	}
	
	/* case 2 : aviod no charge full */
	if ((cw_bat->charger_mode > 0) && (cw_capacity >= 95) && (cw_capacity <= cw_bat->capacity)) {
		cw_printk("Chaman join no charge full\n");
		charging_loop++;	
		if (charging_loop > (BATTERY_UP_MAX_CHANGE / queue_delayed_work_time) ){
			cw_capacity = (cw_bat->capacity + 1) <= 100 ? (cw_bat->capacity + 1) : 100; 
			charging_loop = 0;
			jump_flag = 1;
		}else{
			cw_capacity = cw_bat->capacity; 
		}
	}

	/*case 3 : avoid battery level jump to CW_BAT */
	if ((cw_bat->charger_mode == 0) && (cw_capacity <= cw_bat->capacity ) && (cw_capacity >= 90) && (jump_flag == 1)) {
		cw_printk("Chaman join no charge full discharging\n");
		#ifdef CONFIG_PM
		if(suspend_resume_mark == 1){
			suspend_resume_mark = 0;
			sleep_cap = (after.tv_sec + discharging_loop * (queue_delayed_work_time / 1000))/ (BATTERY_DOWN_MAX_CHANGE/1000) ;
			cw_printk("sleep_cap = %d\n", sleep_cap);
			
			if(cw_capacity >= cw_bat->capacity - sleep_cap) {
				return cw_capacity;
			}else{
				if(!sleep_cap)
					discharging_loop = discharging_loop + 1 + after.tv_sec / (queue_delayed_work_time/1000);
				else
					discharging_loop = 0;
				cw_printk("discharging_loop = %d\n", discharging_loop);
				return cw_bat->capacity - sleep_cap;
			}
		}
		#endif
		discharging_loop++;
		if (discharging_loop > (BATTERY_DOWN_MAX_CHANGE / queue_delayed_work_time) ){
			if (cw_capacity >= cw_bat->capacity - 1){
				jump_flag = 0;
			} else {
				cw_capacity = cw_bat->capacity - 1;
			}
			discharging_loop = 0;
		}else{
			cw_capacity = cw_bat->capacity;
		}
	}
	
	/*case 4 : avoid battery level is 0% when long time charging*/
	if((cw_bat->charger_mode > 0) &&(cw_capacity == 0))
	{
		charging_5_loop++;
		if (charging_5_loop > BATTERY_CHARGING_ZERO / queue_delayed_work_time) {
			cw_por(cw_bat);
			charging_5_loop = 0;
		}
	}else if(charging_5_loop != 0){
		charging_5_loop = 0;
	}
	#ifdef CONFIG_PM
	if(suspend_resume_mark == 1)
		suspend_resume_mark = 0;
	#endif
	return cw_capacity;
}

/*This function called when get voltage from cw2015*/
static int cw_get_voltage(struct cw_battery *cw_bat)
{    
    int ret;
    unsigned char reg_val[2];
    u16 value16, value16_1, value16_2, value16_3;
    int voltage;
    
    ret = cw_read_word(cw_bat->client, REG_VCELL, reg_val);
    if(ret < 0) {
        return ret;
    }
    value16 = (reg_val[0] << 8) + reg_val[1];

    ret = cw_read_word(cw_bat->client, REG_VCELL, reg_val);
    if(ret < 0) {
          return ret;
    }
    value16_1 = (reg_val[0] << 8) + reg_val[1];

    ret = cw_read_word(cw_bat->client, REG_VCELL, reg_val);
    if(ret < 0) {
        return ret;
    }
    value16_2 = (reg_val[0] << 8) + reg_val[1];

    if(value16 > value16_1) {     
        value16_3 = value16;
        value16 = value16_1;
        value16_1 = value16_3;
    }

    if(value16_1 > value16_2) {
    value16_3 =value16_1;
    value16_1 =value16_2;
    value16_2 =value16_3;
    }

    if(value16 >value16_1) {     
    value16_3 =value16;
    value16 =value16_1;
    value16_1 =value16_3;
    }            

    voltage = value16_1 * 312 / 1024;

    return voltage;
}

/*This function called when get RRT from cw2015*/
static int cw_get_time_to_empty(struct cw_battery *cw_bat)
{
    int ret;
    unsigned char reg_val;
    u16 value16;

    ret = cw_read(cw_bat->client, REG_RRT_ALERT, &reg_val);
    if (ret < 0)
            return ret;

    value16 = reg_val;

    ret = cw_read(cw_bat->client, REG_RRT_ALERT + 1, &reg_val);
    if (ret < 0)
            return ret;

    value16 = ((value16 << 8) + reg_val) & 0x1fff;
    return value16;
}
/*
int check_charging_state(const char *filename)
{
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;
	int read_size = 8;
	int state = 0;
	char buf[read_size];
	int ret;

	cw_printk("\n");
	fp = filp_open(filename, O_RDONLY, 0644);
	if (IS_ERR(fp))
		return -1;
	fs = get_fs();
	set_fs(KERNEL_DS);	
	pos = 0;
	ret = vfs_read(fp, buf, read_size, &pos);
	if(ret < 0)
		return -1;
	
	filp_close(fp,NULL);
	set_fs(fs);
	
	state = buf[0] - '0';
	cw_printk(" filename = %s  state = %d \n", filename, state);
	return state;
}
*/ //Old function of get charger status

static void cw_update_charge_status(struct cw_battery *cw_bat)
{
/*
	int if_charging = 0;
	if(check_charging_state(USB_CHARGING_FILE) == 1 
		|| check_charging_state(DC_CHARGING_FILE) == 1)
	{
		if_charging = CHARGING_ON;
	}else{
		if_charging = NO_CHARGING;
	}
	if(if_charging != cw_bat->charger_mode){
		cw_bat->charger_mode = if_charging;
	}
*/ //Old function of get charger status
	int cw_charger_mode;
	cw_charger_mode = get_charge_state();
	if(cw_bat->charger_mode != cw_charger_mode){
        cw_bat->charger_mode = cw_charger_mode;
		cw_bat->change = 1;		
	}
}


static void cw_update_capacity(struct cw_battery *cw_bat)
{
    int cw_capacity;
    cw_capacity = cw_get_capacity(cw_bat);

    if ((cw_capacity >= 0) && (cw_capacity <= 100) && (cw_bat->capacity != cw_capacity)) {				
        cw_bat->capacity = cw_capacity;
		cw_bat->change = 1;
    }
}



static void cw_update_vol(struct cw_battery *cw_bat)
{
    int ret;
    ret = cw_get_voltage(cw_bat);
    if ((ret >= 0) && (cw_bat->voltage != ret)) {
        cw_bat->voltage = ret;
		cw_bat->change = 1;
    }
}

static void cw_update_status(struct cw_battery *cw_bat)
{
    int status;

    if (cw_bat->charger_mode > 0) {
        if (cw_bat->capacity >= 100) 
            status = POWER_SUPPLY_STATUS_FULL;
        else
            status = POWER_SUPPLY_STATUS_CHARGING;
    } else {
        status = POWER_SUPPLY_STATUS_DISCHARGING;
    }

    if (cw_bat->status != status) {
        cw_bat->status = status;
		cw_bat->change = 1;
    } 
}

static void cw_update_time_to_empty(struct cw_battery *cw_bat)
{
    int ret;
    ret = cw_get_time_to_empty(cw_bat);
    if ((ret >= 0) && (cw_bat->time_to_empty != ret)) {
        cw_bat->time_to_empty = ret;
		cw_bat->change = 1;
    }
}


static void cw_bat_work(struct work_struct *work)
{
    struct delayed_work *delay_work;
    struct cw_battery *cw_bat;

    delay_work = container_of(work, struct delayed_work, work);
    cw_bat = container_of(delay_work, struct cw_battery, battery_delay_work);

	cw_update_capacity(cw_bat);
	cw_update_vol(cw_bat);
	cw_update_charge_status(cw_bat);
	cw_update_status(cw_bat);
	cw_update_time_to_empty(cw_bat);
	cw_printk("charger_mod = %d\n", cw_bat->charger_mode);
	cw_printk("status = %d\n", cw_bat->status);
	cw_printk("capacity = %d\n", cw_bat->capacity);
	cw_printk("voltage = %d\n", cw_bat->voltage);

	#ifdef CONFIG_PM
	if(suspend_resume_mark == 1)
		suspend_resume_mark = 0;
	#endif
	
	#ifdef CW_PROPERTIES
	if (cw_bat->change == 1){
		power_supply_changed(&cw_bat->cw_bat); 
		cw_bat->change = 0;
	}
	#endif
	g_cw2015_capacity = cw_bat->capacity;
        g_cw2015_vol = cw_bat->voltage;
	
	queue_delayed_work(cw_bat->cwfg_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(queue_delayed_work_time));
}

static int cw2015_i2c_remove(struct i2c_client *client)
{
    struct cw_battery *data = i2c_get_clientdata(client); 
    printk("[CW2015] cw2015_i2c_remove\n");  
    cancel_delayed_work(&data->battery_delay_work);
    i2c_unregister_device(client);
    kfree(data);
    return 0;
}

#ifdef CW_PROPERTIES
static int cw_battery_get_property(struct power_supply *psy,
                enum power_supply_property psp,
                union power_supply_propval *val)
{
    int ret = 0;

    struct cw_battery *cw_bat;
    cw_bat = container_of(psy, struct cw_battery, cw_bat); 

    switch (psp) {
    case POWER_SUPPLY_PROP_CAPACITY:
            val->intval = cw_bat->capacity;
	   //---------by lifei----------		
	   if(val->intval ==0 && BMT_status.charger_exist==KAL_TRUE) val->intval =1;
            break;
	/*
    case POWER_SUPPLY_PROP_STATUS:   //Chaman charger ic will give a real value
            val->intval = cw_bat->status; 
            break;                 
    */        
    case POWER_SUPPLY_PROP_HEALTH:   //Chaman charger ic will give a real value
            val->intval= POWER_SUPPLY_HEALTH_GOOD;
            break;
    case POWER_SUPPLY_PROP_PRESENT:
            val->intval = cw_bat->voltage <= 0 ? 0 : 1;
            break;
            
    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
            val->intval = cw_bat->voltage;
            break;
            
    case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
            val->intval = cw_bat->time_to_empty;			
            break;
        
    case POWER_SUPPLY_PROP_TECHNOLOGY:  //Chaman this value no need
            val->intval = POWER_SUPPLY_TECHNOLOGY_LION;	
            break;

    default:
            break;
    }
    return ret;
}

static enum power_supply_property cw_battery_properties[] = {
    POWER_SUPPLY_PROP_CAPACITY,
    //POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_HEALTH,
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
    POWER_SUPPLY_PROP_TECHNOLOGY,
};
#endif 

static int cw2015_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ret;
    int loop = 0;
	struct cw_battery *cw_bat;
    //struct device *dev;
	cw_printk("\n");

    mt_set_gpio_mode(GPIO_I2C4_SDA_PIN, GPIO_I2C4_SDA_PIN_M_SDA);
    mt_set_gpio_mode(GPIO_I2C4_SCA_PIN, GPIO_I2C4_SCA_PIN_M_SCL);

    cw_bat = devm_kzalloc(&client->dev, sizeof(*cw_bat), GFP_KERNEL);
    if (!cw_bat) {
		cw_printk("cw_bat create fail!\n");
        return -ENOMEM;
    }

    hmi_get_battery_version();        
    switch (liuchao_test_hmi_battery_version)
    {
        case 1:
            cw_bat_platdata.cw_bat_config_info = config_info;
            break;
        case 2:
            cw_bat_platdata.cw_bat_config_info = config_info_des;
            break;
        default:
        printk("[CW2015] Battery type ID not match\n");
    }

    i2c_set_clientdata(client, cw_bat);
	cw_client=client;
    cw_bat->client = client;
    cw_bat->plat_data = &cw_bat_platdata;
    cw_bat->capacity = 1;
    cw_bat->voltage = 0;
    cw_bat->status = 0;
	cw_bat->charger_mode = NO_CHARGING;
	cw_bat->change = 0;
	
    ret = cw_init(cw_bat);
    while ((loop++ < 200) && (ret != 0)) {
		msleep(9);
        ret = cw_init(cw_bat);
	printk("%s : cw2015 while(1)!\n", __func__);
    }
    if (ret) {
		printk("%s : cw2015 init fail!\n", __func__);
        return ret;	
    }

	#ifdef CW_PROPERTIES
	cw_bat->cw_bat.name = CW_PROPERTIES;
	cw_bat->cw_bat.type = POWER_SUPPLY_TYPE_BATTERY;
	cw_bat->cw_bat.properties = cw_battery_properties;
	cw_bat->cw_bat.num_properties = ARRAY_SIZE(cw_battery_properties);
	cw_bat->cw_bat.get_property = cw_battery_get_property;
	ret = power_supply_register(&client->dev, &cw_bat->cw_bat);
	if(ret < 0) {
	    power_supply_unregister(&cw_bat->cw_bat);
	    return ret;
	}
	#endif

	cw_bat->cwfg_workqueue = create_singlethread_workqueue("cwfg_gauge");
	INIT_DELAYED_WORK(&cw_bat->battery_delay_work, cw_bat_work);
	queue_delayed_work(cw_bat->cwfg_workqueue, &cw_bat->battery_delay_work , msecs_to_jiffies(50));
	
	cw_printk("cw2015 driver probe success!\n");
    return 0;
}


static int cw2015_detect(struct i2c_client *client, struct i2c_board_info *info) 
{	 
	cw_printk("\n");
	strcpy(info->type, CW2015_DEV_NAME);
	return 0;
}


#ifdef CONFIG_PM
static int cw_bat_suspend(struct device *dev)
{
        struct i2c_client *client = to_i2c_client(dev);
        struct cw_battery *cw_bat = i2c_get_clientdata(client);
		read_persistent_clock(&suspend_time_before);
        cancel_delayed_work(&cw_bat->battery_delay_work);
        return 0;
}

static int cw_bat_resume(struct device *dev)
{	
        struct i2c_client *client = to_i2c_client(dev);
        struct cw_battery *cw_bat = i2c_get_clientdata(client);
		suspend_resume_mark = 1;
		read_persistent_clock(&after);
		after = timespec_sub(after, suspend_time_before);
        queue_delayed_work(cw_bat->cwfg_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(2));
        return 0;
}

static const struct dev_pm_ops cw_bat_pm_ops = {
        .suspend  = cw_bat_suspend,
        .resume   = cw_bat_resume,
};
#endif

static const struct of_device_id cw2015_of_match[] = {
	{.compatible = "mediatek,cw2015_config"},
	
};

static struct i2c_driver cw2015_i2c_driver = {
	.probe		  = cw2015_probe,
	.remove 	  = cw2015_i2c_remove,
	.detect 	  = cw2015_detect,
        .id_table   = CW2015_i2c_id,
	.driver 	  = 	
	 {
		 #ifdef CONFIG_PM
      		  .pm     = &cw_bat_pm_ops,
		#endif
	        //.of_match_table = cw2015_of_match,
	        .name   = CW2015_DEV_NAME
	        //.owner  = THIS_MODULE,
 	   },
};
//
static int __init cw_bat_init(void)
{
    i2c_register_board_info(I2C_BUSNUM, &i2c_CW2015, 1);   
    if(i2c_add_driver(&cw2015_i2c_driver))
    {
        printk("[CW2015] add driver error\n");
        return -1;
    }
    printk("[CW2015] cw_bat_init done\n");
    return 0;
}

static void __exit cw_bat_exit(void)
{
    printk("[CW2015] cw_bat_exit\n");
    i2c_del_driver(&cw2015_i2c_driver);
}

module_init(cw_bat_init);
module_exit(cw_bat_exit);

MODULE_AUTHOR("Chaman Qi");
MODULE_DESCRIPTION("CW2015 FGADC Device Driver V3.0");
MODULE_LICENSE("GPL");
