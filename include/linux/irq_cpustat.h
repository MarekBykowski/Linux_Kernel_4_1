#ifndef __irq_cpustat_h
#define __irq_cpustat_h
#include <linux/timekeeper_internal.h>
#include <linux/ktime.h>
#include <linux/average.h>

/*
 * Contains default mappings for irq_cpustat_t, used by almost every
 * architecture.  Some arch (like s390) have per cpu hardware pages and
 * they define their own mappings for irq_stat.
 *
 * Keith Owens <kaos@ocs.com.au> July 2000.
 */


/*
 * Simple wrappers reducing source bloat.  Define all irq_stat fields
 * here, even ones that are arch dependent.  That way we get common
 * definitions instead of differing sets for each arch.
 */

#ifndef __ARCH_IRQ_STAT
extern irq_cpustat_t irq_stat[];		/* type in asm/hardirq.h with var in kernel/softirq.c */
#define __IRQ_STAT(cpu, member)	(irq_stat[cpu].member)
#define __SET_START_TIME(cpu, member, ipinr) (irq_stat[cpu].member[ipinr] = ktime_get())

#define __GET_DURATION(cpu, member, ipinr) ({ \
	irq_stat[cpu].end_time[ipinr] = ktime_get(); \
	irq_stat[cpu].member[ipinr] = ktime_sub(irq_stat[cpu].end_time[ipinr], irq_stat[cpu].start_time[ipinr]); \
})

#define __GET_AVG_DURATION(cpu, member, ipinr) ({ \
	sma_add(&irq_stat[cpu].member[ipinr], (unsigned long)irq_stat[cpu].duration[ipinr].tv64); \
})
#endif

#if 0
#define __GET_AVG_DURATION(cpu, member, ipinr) ({ \
	sma_add(&irq_stat[cpu].member[ipinr], (unsigned long)irq_stat[cpu].duration[ipinr].tv64); \
	sma_read(&irq_stat[cpu].member[ipinr]); \
})
#endif

  /* arch independent irq_stat fields */
#define local_softirq_pending() \
	__IRQ_STAT(smp_processor_id(), __softirq_pending)

  /* arch dependent irq_stat fields */
#define nmi_count(cpu)		__IRQ_STAT((cpu), __nmi_count)	/* i386 */

#endif	/* __irq_cpustat_h */
