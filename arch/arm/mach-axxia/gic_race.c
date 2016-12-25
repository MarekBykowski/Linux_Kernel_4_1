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


#if 0
static ssize_t
trigger_tty(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	int irq = 88; /*irq of ttyAMA0*/
	unsigned int cpu = 0;
	unsigned int val;
	int s;
	preempt_disable();
	u32 pcpu = cpu_logical_map(smp_processor_id());
	atomic_t value = ATOMIC_INIT(0x2);
	char to_tty[3] = { 0 };
	struct irq_desc *desc = irq_to_desc(irq);	
	printk(KERN_INFO "mb: %s value 0x%x\n", __func__, atomic_read_set_bit_write(3, &value));
	printk(KERN_INFO "mb: %s desc->action->name %s\n", __func__, desc->action->name);	

	if (copy_from_user(to_tty, buf, count))
	    return -EFAULT;                              

	if (kstrtou32(to_tty, 10, &cpu))
	    return -EFAULT;               

	if (irq_set_affinity(irq, cpumask_of(cpu)) && irq > 32)             
		pr_warning("unable to set irq affinity (irq=%d, cpu=%u)\n", irq, cpu);

	while (on_same_cluster(pcpu, cpu_logical_map(cpu))) {
		cpu++;
	}
	s=3;
	printk(KERN_INFO "mb: %s value 0x%x\n", __func__, s);
	val = mb_atomic_read_set_bit_write(s, &(&per_cpu(marek_mux_msg_val, cpu_logical_map(cpu)))->msg);
	pr_info("mb: %s val 0x%x", __func__, val);
	val = mb_atomic_read_set_bit_write(s, &(&per_cpu(marek_mux_msg_val, cpu_logical_map(cpu)))->msg);
	pr_info("mb: %s val 0x%x", __func__, val);
	/*marek_ipi_message_pass(cpumask_of(cpu), 3);
	marek_ipi_message_pass(cpumask_of(cpu), 7);*/
	preempt_enable();
	return count;
}
#endif

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

		if (0 != request_irq(40, trng_isr, 0, "trng", cookie))
			BUG_ON(1);
		trace_printk("requested irq 40\n");

		while (on_same_cluster(pcpu, cpu_logical_map(lcpu))) 
				lcpu++;                     

		if (lcpu <= 15) {          
			trace_printk("I'm on cpu(%u) needing to execute irq_set_affinity() on cpu(%u)\n", pcpu, lcpu);              
			if (irq_set_affinity(40, cpumask_of(lcpu)))             
				pr_warning("unable to set irq affinity (irq=%d, cpu=%u)\n", 40, lcpu);
			lcpu++;
		} else {
			lcpu = 0;
		}
	} else if (2 == haha) {
		haha = 1;
		free_irq(40, cookie);
		flush_workqueue(foo->wq);
		destroy_workqueue(foo->wq);
		kfree(foo);
	}

	return count;
}

static void run_me_on_all_cpus(void *args)
{
	/*trace_printk("mb: %s() -> smp_call_function\n", __func__);*/
	return;
}


static ssize_t
trigger_ipi_func(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
    smp_call_function(run_me_on_all_cpus, NULL, true);
	return count;
}

static ssize_t
trigger_ipi_irq_work(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	smp_call_irq_work(run_me_on_all_cpus, NULL, true);
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

void
axxia_race_gic_init(void)
{
	/* Create /proc entry. */
	proc_create("ipi-func-single", S_IWUSR, NULL, &ipi_func_single_ops);
	proc_create("ipi-func", S_IWUSR, NULL, &ipi_func_ops);
	proc_create("ipi-irq-work", S_IWUSR, NULL, &ipi_irq_work_ops);
	return;
}
