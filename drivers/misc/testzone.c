/*
 *  Copyright (c) LSI Corporation, 2013
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License 2 as published by the
 *  Free Software Foundation.
 *
 */


#include <linux/random.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/of_irq.h>
#include <linux/dmaengine.h>
#include <linux/crc32.h>
#include <linux/delay.h>
#include <linux/highmem.h>


/*KMALLOC_MAX_CACHE_SIZE 8192*/

#define NR_ALLOCS 10

void static *buf[192] = {NULL};
size_t static size[192] = { [0 ... NR_ALLOCS] = 6580 };

static int alloc_init(void)
{
	gfp_t gfpflags = GFP_DMA32 /*GFP_KERNEL*//*|GFP_DMA|GFP_USER*/;
	int i;
	
	for (i=0; i<ARRAY_SIZE(size); i++) {
		/* Allocate */
		if (size[i] == 0) continue;
		buf[i] = kmalloc(size[i],gfpflags);
			
		/* Test */
		if(!buf[i]) {
			/* handle error */
			pr_err("memory allocation failed\n");
			return -ENOMEM;
		} else {
			pr_info("[%d] Address 0x%p (v:0x%p) (size %lu) %pGg(0x%x)\n", 
				i, (void*)virt_to_phys(buf[i]), buf[i], size[i], &gfpflags, gfpflags);
		}

		/* Release 
		kfree(buf[i]);
		pr_info("Memory freed\n");
		*/
	}
	
	return 0;
}

static void alloc_exit(void)
{
	int i;
	for (i=0; i<ARRAY_SIZE(buf); i++) {
		if (size[i] == 0) continue;
		kfree(buf[i]);
		pr_info("Memory freed\n");
	}
	pr_info("module %s removed\n", __FILE__);
}

module_init(alloc_init);
module_exit(alloc_exit);

MODULE_AUTHOR("mb:");
MODULE_DESCRIPTION("Test dma32 allocation");
MODULE_LICENSE("GPL v2");
