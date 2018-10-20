#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include "yusu_android_speaker.h"

#define PRINTK(format, args...)

#define AMP_CLASS_AB

#define SPK_WARM_UP_TIME 55
#define SPK_AMP_GAIN 4
#define RCV_AMP_GAIN 1
#define SPK_R_ENABLE 1
#define SPK_L_ENABLE 1

static int Speaker_Volume;
static bool gsk_on;
static bool gsk_resume;
static bool gsk_forceon;

extern void Yusu_Sound_AMP_Switch(BOOL enable);

bool Speaker_Init(void)
{
    return true;
}

bool Speaker_Register(void)
{
    return false;
}

int ExternalAmp(void)
{
    return 0;
}

bool Speaker_DeInit(void)
{
    return false;
}

void Sound_SpeakerL_SetVolLevel(int level)
{
    PRINTK("Sound_SpeakerL_SetVolLevel level=%d\n",level);
}

void Sound_SpeakerR_SetVolLevel(int level)
{
    PRINTK("Sound_SpeakerR_SetVolLevel level=%d\n",level);
}

void Sound_Speaker_Turnon(int channel)
{
    PRINTK("Sound_Speaker_Turnon channel = %d\n",channel);
    if (gsk_on)
        return;
    gsk_on = true;
}

void Sound_Speaker_Turnoff(int channel)
{
    PRINTK("Sound_Speaker_Turnoff channel = %d\n",channel);
    if (!gsk_on)
        return;
    gsk_on = false;
}

void Sound_Speaker_SetVolLevel(int level)
{
    Speaker_Volume =level;
}

void Sound_Headset_Turnon(void) {}

void Sound_Headset_Turnoff(void) {}

void Sound_Earpiece_Turnon(void) {}

void Sound_Earpiece_Turnoff(void) {}

void AudioAMPDevice_Suspend(void)
{
    PRINTK("AudioDevice_Suspend\n");
    if (gsk_on) {
        Sound_Speaker_Turnoff(Channel_Stereo);
        gsk_resume = true;
    }
}
void AudioAMPDevice_Resume(void)
{
    PRINTK("AudioDevice_Resume\n");
    if (gsk_resume)
        Sound_Speaker_Turnon(Channel_Stereo);
    gsk_resume = false;
}
void AudioAMPDevice_SpeakerLouderOpen(void)
{
    PRINTK("AudioDevice_SpeakerLouderOpen\n");
    gsk_forceon = false;
    if (gsk_on)
        return;
    Sound_Speaker_Turnon(Channel_Stereo);
    gsk_forceon = true;
    return;
}
void AudioAMPDevice_SpeakerLouderClose(void)
{
    PRINTK("AudioDevice_SpeakerLouderClose\n");
    if (gsk_forceon)
        Sound_Speaker_Turnoff(Channel_Stereo);
    gsk_forceon = false;

}
void AudioAMPDevice_mute(void)
{
    PRINTK("AudioDevice_mute\n");
    if (gsk_on)
        Sound_Speaker_Turnoff(Channel_Stereo);
}

int Audio_eamp_command(unsigned int type, unsigned long args, unsigned int count)
{
    return 0;
}
static char *ExtFunArray[] =
{
    "InfoMATVAudioStart",
    "InfoMATVAudioStop",
    "End"
};

kal_int32 Sound_ExtFunction(const char* name, void* param, int param_size)
{
    int i;
    int funNum = -1;

    while(strcmp("End",ExtFunArray[i]) != 0 ) {
        if (strcmp(name,ExtFunArray[i]) == 0 ) {
            funNum = i;
            break;
        }
        i++;
    }

    switch (funNum) {
        case 0:
            printk("RunExtFunction InfoMATVAudioStart\n");
            break;
        case 1:
            printk("RunExtFunction InfoMATVAudioStop\n");
            break;
        default:
            break;
    }

    return 1;
}
