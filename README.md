# UNOFFICIAL KERNEL SOURCES OF RN2
 - Check lcms tables(boe, auo)
 - Reverse focaltech and atmel touchscreen
 - cw2015 under
 - Add and check stk3x1x, yas537
 - Check ov5670* drivers(imgsensors)
 - consumerir under
 - Fix random crashes
 - Fix graphic glitches

## CONSUMERIR
#### DONE:
 - consumerir_init
 - consumerir_device
 - consumerir_driver
 - char_dev_fops
 - dev_char_ioctl
 - dev_char_read

#### TODO:
 - irtx_pwm_config
 - dev_char_open
 - dev_char_release
 - set_consumerir_pwm
 - dev_char_write
 - consumerir_probe

## CW2015
#### DONE:
 - cw2015_i2c_probe
 - cw_update_config_info
 - cw_init
 - cw_write
 - cw_read
 - cw_update_time_member_charge_start
 - cw2015_i2c_detect
 - cw2015_i2c_remove
 - cw2015_i2c_suspend
 - cw2015_i2c_resume
 - cw2015_i2c_driver
 - cw_bat_init
 - cw_bat_exit

#### TODO:
 - cw_bat_work
 - rk_bat_update_capacity
 - rk_bat_update_vol
 - cw_get_vol
 - cw_get_capacity
 - cw_convertData
 - cw_algorithm
 - cw_update_time_member_capacity_change
 - cw_read_word
 - rk_usb_update_online
 - get_usb_charge_state
