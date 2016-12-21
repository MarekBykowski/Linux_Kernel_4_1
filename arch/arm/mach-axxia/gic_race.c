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
#include "axxia.h"

#include "../../../kernel/irq/internals.h"

#define CORES_PER_CLUSTER (4)

struct marek_mux_msg {
	atomic_t msg;
};

static DEFINE_PER_CPU_SHARED_ALIGNED(struct marek_mux_msg, marek_mux_msg_val);

static bool on_same_cluster(u32 pcpu1, u32 pcpu2);

int mb_atomic_read_set_bit_write(int i, atomic_t *v) 
{                                   
    unsigned long tmp, tmp2;             
    int result;                   
                                 
	pr_info("mb: %s val %d", __func__, i);
    smp_mb();                   
    prefetchw(&v->counter);    
                              
    __asm__ __volatile__("@ atomic_set_bit_return\n"    
"1: ldrex   %0, [%4]\n"                   
"   mov %2, #1\n"
"   orr %0, %0, %2, LSL %5\n"            
"   strex   %1, %0, [%4]\n"             
"   teq %1, #0\n"                      
"   bne 1b"                       
    : "=&r" (result), "=&r" (tmp), "=&r" (tmp2), "+Qo" (v->counter) 
	: "r" (&v->counter), "Ir" (i)
    : "cc");                          
                                  
    smp_mb();                    
                                
    return result;             
}

static void marek_ipi_message_pass(const struct cpumask *mask,
				   int i)
{
	int lcpu = smp_processor_id();
	unsigned int val;
	struct marek_mux_msg* info;
	info = &per_cpu(marek_mux_msg_val, cpu_logical_map(lcpu));

	for_each_cpu(lcpu, mask) {
		val = mb_atomic_read_set_bit_write(i, &info->msg);
		pr_info("mb: %s val 0x%x", __func__, val);
	}

	__asm __volatile__("kot: b kot\n");
}

static bool on_same_cluster(u32 pcpu1, u32 pcpu2)
{
	return pcpu1 / CORES_PER_CLUSTER == pcpu2 / CORES_PER_CLUSTER;
}

struct foo {
	int marek;
	struct workqueue_struct *wq;
	struct work_struct offload;
};

static irqreturn_t 
trng_isr(int irq_no, void *arg)
{
	struct foo *foo = arg;
	queue_work(foo->wq, &foo->offload);
	return IRQ_HANDLED;
}

static void
work_func(struct work_struct *work)
{	
	struct foo *foo = container_of(work, struct foo, offload);
	trace_printk("mb: %s() foo->marek %d\n", __func__, foo->marek);
	return;
}

static int haha = 1;
void *cookie = &haha;

static ssize_t
trigger_ipi_func_single(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	static unsigned int lcpu = 0;
	u32 pcpu = cpu_logical_map(smp_processor_id());
	char to_tty[3] = { 0 };
	unsigned int var;
	static struct foo *foo; 

	if (copy_from_user(to_tty, buf, count))
	    return -EFAULT;                              

	if (kstrtou32(to_tty, 10, &var))
	    return -EFAULT;               

	/*if (desc->depth == 1) enable_irq(40);
	else disable_irq(40);*/

	if (1 == haha) {
		haha = 2;
		foo= kzalloc(sizeof(*foo), GFP_KERNEL);
		/* Create kernel thread/work queue*/
		foo->wq = create_singlethread_workqueue("marek_workqueue");
		/* Fill out work_struct */
		INIT_WORK(&foo->offload, work_func);

		trace_printk("cpu(%u) requesting irq 40\n", pcpu);
		if (0 != request_irq(40, trng_isr, 0, "trng", cookie))
			BUG_ON(1);

		while (on_same_cluster(pcpu, cpu_logical_map(lcpu))) 
				lcpu++;                     

		if (lcpu <= 15) {          
			trace_printk("cpu(%u) executing irq_set_affinity() on cpu(%u)\n", pcpu, lcpu);              
			if (irq_set_affinity(40, cpumask_of(lcpu)))             
				pr_warning("unable to set irq affinity (irq=%d, cpu=%u)\n", 40, lcpu);
			lcpu++;
		} else {
			lcpu = 0;
		}
	} else if (2 == haha) {
		haha = 1;
		trace_printk("cpu(%u) freeing irq 40\n", pcpu);
		free_irq(40, cookie);
		flush_workqueue(foo->wq);
		destroy_workqueue(foo->wq);
		kfree(foo);
	}

	return count;
}

static void run_me_on_remote_cpu(void *args)
{
	/* just anything */
	int i = 100;
	i += 50;
}

static ssize_t
trigger_ipi_func(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	unsigned int cpu = 0;
	for (cpu=0; cpu<16; cpu++)
    	smp_call_function_specific(cpumask_of(cpu), run_me_on_remote_cpu, NULL, true);

	return count;
}

static ssize_t
trigger_ipi_irq_work(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	/*smp_call_irq_work(cpu_online_mask, run_me_on_all_cpus, NULL, true);*/
	unsigned int cpu = 0;
	for (cpu=0; cpu<16; cpu++)
		smp_call_irq_work(cpumask_of(cpu), run_me_on_remote_cpu, NULL, true);
	
	return count;
}

static ssize_t
sma_ops(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	int iterate = 100;
	struct sma avg1, avg2;
	int i, j;
	unsigned long k=0, sum = 0;
	sma_init(&avg1, 10);
	for (i=1; i<iterate; i++) {
		sum += i;
		sma_add(&avg1, i);
	}
	trace_printk("avg1 sma %lu\n", sma_read(&avg1));	
	trace_printk("avg1 %lu\n", sum/iterate);	

	sma_init(&avg2, 10);
	for (i=1; i<iterate; i++) {
		if (i % 2) j=-20;
		else j=20;
		k = 5000 + j;
		sum += k;
		sma_add(&avg2, k);
	}
	trace_printk("avg2 sma %lu\n", sma_read(&avg2));	
	trace_printk("avg2 %lu\n", sum/iterate);	
	return count;
}

static const struct file_operations ipi_func_single_ops = {
	.write      = trigger_ipi_func_single,
};
static const struct file_operations ipi_func_ops = {
	.write      = trigger_ipi_func,
};
static const struct file_operations ipi_irq_work_ops = {
	.write      = trigger_ipi_irq_work,
};
static const struct file_operations test_sma_ops = {
	.write      = sma_ops,
};

void
axxia_race_gic_init(void)
{
	proc_create("ipi-func-single", S_IWUSR, NULL, &ipi_func_single_ops);
	proc_create("ipi-func", S_IWUSR, NULL, &ipi_func_ops);
	proc_create("ipi-irq-work", S_IWUSR, NULL, &ipi_irq_work_ops);
	proc_create("sma", S_IWUSR, NULL, &test_sma_ops);
}
