/*#TODO: reduce a number of includes*/

#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/reboot.h>
#include <linux/syscore_ops.h>
#include <linux/proc_fs.h>
#include <linux/prefetch.h>
#include <linux/delay.h>

#include <linux/bitmap.h>          
#include <linux/interrupt.h>       
#include <linux/irq.h>             
#include <linux/kernel.h>          
#include <linux/export.h>          
#include <linux/of.h>              
#include <linux/perf_event.h>      
#include <linux/platform_device.h> 
#include <linux/slab.h>            
#include <linux/spinlock.h>        
#include <linux/uaccess.h>         
#include <linux/average.h>
                                   
#include <asm/cputype.h>           
#include <asm/irq.h>               
#include <asm/irq_regs.h>          
#include <asm/pmu.h>               
#include <asm/stacktrace.h>        
#include <asm/atomic.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/lsi-ncr.h>
#include <linux/phy.h>

/* MDIO bus driver private data */                     
struct axxia_mdio_priv {                               
    void __iomem *base;                                
    struct mii_bus *bus;                               
};                                                     
                                                       
static inline void __iomem *                           
bus_to_regs(struct mii_bus *bus)                       
{                                                      
    return ((struct axxia_mdio_priv *)bus->priv)->base;
}                                                      

static int
scan(struct mii_bus *bus, int mdio_no)
{
	int i;
	pr_info("Scanning bus MDIO#%d (bus@virt %p)\n", mdio_no, (void*)bus_to_regs(bus));
	for (i = 0; i < PHY_MAX_ADDR; i++) {
		struct phy_device *phydev = get_phy_device(bus, i, false);
		if (IS_ERR(phydev) || phydev == NULL) {
			;
		} else {
			pr_info("phydev->%08X", phydev->addr);
		}
	}
	return 0;
}

static int callback(struct device *dev, void *data)
{
	int ret = 0;
	struct mii_bus *bus = dev_get_drvdata(dev);

	if (0xffffff800007a000 /*should be virt addr of MDIO#0*/ == 
			(unsigned long) bus_to_regs(bus)) {
		if (scan(bus, 0)) {
			ret = -1;
			pr_info("Error scanning MDIO#%d (bus@virt %p)\n", 0, (void*) bus_to_regs(bus));
		}
	} else /*and here MDIO#1*/ {
		if (scan(bus, 1)) {
			ret = -1;
			pr_info("Error scanning MDIO#%d (bus@virt %p)\n", 1, (void*) bus_to_regs(bus));
		}
	}
	
	return ret;
}

static ssize_t
write_ops(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	struct device_driver *drv;
	if ((drv = driver_find("axxia-mdio", &platform_bus_type))) 
		if (!driver_for_each_device(drv, NULL, NULL, callback)) 
			pr_info("Scanning MDIO busses completed with success\n");
	return count;
}

static const struct file_operations mdio_scan_ops = {
	.write      = write_ops,
};

static int
axxia_race_gic_init(void)
{
	proc_create("mdio-scan", S_IWUSR, NULL, &mdio_scan_ops);
	return 0;
}

early_initcall(axxia_race_gic_init);

