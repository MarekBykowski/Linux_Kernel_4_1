#ifndef __ASM_HARDIRQ_H
#define __ASM_HARDIRQ_H

#include <linux/cache.h>
#include <linux/threads.h>
#include <linux/ktime.h>
#include <asm/irq.h>
#include <linux/average.h>

#define NR_IPI 8

typedef struct {
	unsigned int __softirq_pending;
#ifdef CONFIG_SMP
	unsigned int ipi_irqs[NR_IPI];
	ktime_t start_time[NR_IPI];
	ktime_t end_time[NR_IPI];
	ktime_t duration[NR_IPI];
	struct sma sma_avg[NR_IPI];
#endif
} ____cacheline_aligned irq_cpustat_t;

#include <linux/irq_cpustat.h>	/* Standard mappings for irq_cpustat_t above */

#define __inc_irq_stat(cpu, member)	__IRQ_STAT(cpu, member)++
#define __get_irq_stat(cpu, member)	__IRQ_STAT(cpu, member)
#define __set_start_time(cpu, member, ipinr) __SET_START_TIME(cpu, member, ipinr)
#define __get_duration(cpu, member, ipinr)	__GET_DURATION(cpu, member, ipinr)
#define __get_avg_duration(cpu, member, ipinr)	__GET_AVG_DURATION(cpu, member, ipinr)

#ifdef CONFIG_SMP
u64 smp_irq_stat_cpu(unsigned int cpu);
#else
#define smp_irq_stat_cpu(cpu)	0
#endif

#define arch_irq_stat_cpu	smp_irq_stat_cpu

#define __ARCH_IRQ_EXIT_IRQS_DISABLED	1

#endif /* __ASM_HARDIRQ_H */
