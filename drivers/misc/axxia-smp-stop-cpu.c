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

static ssize_t
write_ops(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	pr_info("start\n");
	preempt_disable();
	/* send stop message to other CPUs */
	local_irq_disable();
	pr_info("local_irq_disable\n");
	local_fiq_disable();
	asm volatile ("dsb sy");
	asm volatile ("isb");
	pr_info("dsb isb\n");
	smp_send_stop();
	pr_info("after smp_send_stop\n");
	udelay(1000);

	return count;
}

static const struct file_operations stop_cpus_ops = {
	.write      = write_ops,
};

static int
axxia_stop_cpus(void)
{
	proc_create("stop-cpus", S_IWUSR, NULL, &stop_cpus_ops);
	return 0;
}

early_initcall(axxia_stop_cpus);
