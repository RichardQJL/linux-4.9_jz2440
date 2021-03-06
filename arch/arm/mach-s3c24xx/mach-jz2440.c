/* linux/arch/arm/mach-s3c2440/mach-jz2440.c
 *
 * Copyright (c) 2004-2005 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * http://www.fluff.org/ben/smdk2440/
 *
 * Thanks to Dimity Andric and TomTom for the loan of an SMDK2440.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/serial_s3c.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/irq.h>

#include <mach/hardware.h>
#include <asm/irq.h>
#include <asm/mach-types.h>

#include <mach/regs-gpio.h>
#include <mach/regs-lcd.h>

#include <mach/fb.h>
#include <linux/platform_data/i2c-s3c2410.h>

#include <plat/devs.h>
#include <plat/cpu.h>
#include <plat/samsung-time.h>

#include "common.h"
#include "common-jz2440.h"
#include <linux/dm9000.h>

static struct map_desc jz2440_iodesc[] __initdata = {
	/* ISA IO Space map (memory space selected by A24) */

	{
		.virtual	= (u32)S3C24XX_VA_ISA_WORD,
		.pfn		= __phys_to_pfn(S3C2410_CS2),
		.length		= 0x10000,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (u32)S3C24XX_VA_ISA_WORD + 0x10000,
		.pfn		= __phys_to_pfn(S3C2410_CS2 + (1<<24)),
		.length		= SZ_4M,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (u32)S3C24XX_VA_ISA_BYTE,
		.pfn		= __phys_to_pfn(S3C2410_CS2),
		.length		= 0x10000,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (u32)S3C24XX_VA_ISA_BYTE + 0x10000,
		.pfn		= __phys_to_pfn(S3C2410_CS2 + (1<<24)),
		.length		= SZ_4M,
		.type		= MT_DEVICE,
	}
};

#define UCON S3C2410_UCON_DEFAULT | S3C2410_UCON_UCLK
#define ULCON S3C2410_LCON_CS8 | S3C2410_LCON_PNONE | S3C2410_LCON_STOPB
#define UFCON S3C2410_UFCON_RXTRIG8 | S3C2410_UFCON_FIFOMODE

static struct s3c2410_uartcfg jz2440_uartcfgs[] __initdata = {
	[0] = {
		.hwport	     = 0,
		.flags	     = 0,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x03,
		.ufcon	     = 0x51,
	},
	[1] = {
		.hwport	     = 1,
		.flags	     = 0,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x03,
		.ufcon	     = 0x51,
	},
	/* IR port */
	[2] = {
		.hwport	     = 2,
		.flags	     = 0,
		.ucon	     = 0x3c5,
		.ulcon	     = 0x43,
		.ufcon	     = 0x51,
	}
};

/* LCD driver info */

static struct s3c2410fb_display jz2440_lcd_cfg __initdata = {

	.lcdcon5	= S3C2410_LCDCON5_FRM565 |
			  S3C2410_LCDCON5_INVVLINE |
			  S3C2410_LCDCON5_INVVFRAME |
			  S3C2410_LCDCON5_PWREN |
			  S3C2410_LCDCON5_HWSWP,

	.type		= S3C2410_LCDCON1_TFT,

	.width		= 240,
	.height		= 320,

	.pixclock	= 9000000000, /* HCLK 60 MHz, divisor 10 */
	.xres		= 480,
	.yres		= 272,
	.bpp		= 16,
	.left_margin	= 2,
	.right_margin	= 2,
	.hsync_len	= 41,
	.upper_margin	= 2,
	.lower_margin	= 2,
	.vsync_len	= 10,
};

static struct s3c2410fb_mach_info jz2440_fb_info __initdata = {
	.displays	= &jz2440_lcd_cfg,
	.num_displays	= 1,
	.default_display = 0,

#if 0
	/* currently setup by downloader */
	.gpccon		= 0xaa940659,
	.gpccon_mask	= 0xffffffff,
	.gpcup		= 0x0000ffff,
	.gpcup_mask	= 0xffffffff,
	.gpdcon		= 0xaa84aaa0,
	.gpdcon_mask	= 0xffffffff,
	.gpdup		= 0x0000faff,
	.gpdup_mask	= 0xffffffff,
#endif

	//.lpcsel		= ((0xCE6) & ~7) | 1<<4,
};

/* DM9000AEP 10/100 ethernet controller */
#define JZ2440_DM9K_BASE	(S3C2410_CS4 + 0x300)

static struct resource jz2440_dm9k_resource[] = {
	[0] = DEFINE_RES_MEM(JZ2440_DM9K_BASE, 4),
	[1] = DEFINE_RES_MEM(JZ2440_DM9K_BASE + 4, 4),
	[2] = DEFINE_RES_NAMED(IRQ_EINT7, 1, NULL, IORESOURCE_IRQ \
						| IORESOURCE_IRQ_HIGHEDGE),
};

/*
 * The DM9000 has no eeprom, and it's MAC address is set by
 * the bootloader before starting the kernel.
 */
static struct dm9000_plat_data jz2440_dm9k_pdata = {
	.flags		= (DM9000_PLATF_16BITONLY | DM9000_PLATF_NO_EEPROM),
};

static struct platform_device jz2440_device_eth = {
	.name		= "dm9000",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(jz2440_dm9k_resource),
	.resource	= jz2440_dm9k_resource,
	.dev		= {
		.platform_data	= &jz2440_dm9k_pdata,
	},
};

static struct platform_device *jz2440_devices[] __initdata = {
	&s3c_device_ohci,
	&s3c_device_lcd,
	&s3c_device_wdt,
	&s3c_device_i2c0,
	&s3c_device_iis,
	&jz2440_device_eth,
};

static void __init jz2440_map_io(void)
{
	s3c24xx_init_io(jz2440_iodesc, ARRAY_SIZE(jz2440_iodesc));
	s3c24xx_init_uarts(jz2440_uartcfgs, ARRAY_SIZE(jz2440_uartcfgs));
	samsung_set_timer_source(SAMSUNG_PWM3, SAMSUNG_PWM4);
}

static void __init jz2440_init_time(void)
{
	s3c2440_init_clocks(12000000);
	samsung_timer_init();
}

static void __init jz2440_machine_init(void)
{
	s3c24xx_fb_set_platdata(&jz2440_fb_info);
	s3c_i2c0_set_platdata(NULL);

	platform_add_devices(jz2440_devices, ARRAY_SIZE(jz2440_devices));
	jz_machine_init();
	//backlight on
	writel((readl(S3C2410_GPBCON)&~(3))|1, S3C2410_GPBCON);
	writel((readl(S3C2410_GPBDAT)|1),S3C2410_GPBDAT);
}

MACHINE_START(JZ2440, "JZ2440")
	/* Maintainer: Ben Dooks <ben-linux@fluff.org> */
	.atag_offset	= 0x100,

	.init_irq	= s3c2440_init_irq,
	.map_io		= jz2440_map_io,
	.init_machine	= jz2440_machine_init,
	.init_time	= jz2440_init_time,
MACHINE_END
