#ifndef _LINUX_AVERAGE_H
#define _LINUX_AVERAGE_H

#include <linux/kernel.h>
/* Exponentially weighted moving average (EWMA) */

/* For more documentation see lib/average.c */

struct ewma {
	unsigned long internal;
	unsigned long factor;
	unsigned long weight;
};

extern void ewma_init(struct ewma *avg, unsigned long factor,
		      unsigned long weight);

extern struct ewma *ewma_add(struct ewma *avg, unsigned long val);

/**
 * ewma_read() - Get average value
 * @avg: Average structure
 *
 * Returns the average value held in @avg.
 */
static inline unsigned long ewma_read(const struct ewma *avg)
{
	return avg->internal >> avg->factor;
}

typedef struct sma_helper{
	unsigned int count;
} sma_helper_t;

struct sma {
	unsigned long internal;
	unsigned long n;
	sma_helper_t helper;
}; 

extern void sma_init(struct sma *avg, unsigned long n);
extern unsigned long sma_add(struct sma *avg, unsigned long val);

static inline unsigned long sma_read(const struct sma *avg)
{
	/* until n return sum to date / n, beyond avg calculated */
	return avg->helper.count < avg->n ? 
			avg->internal / avg->helper.count : avg->internal;
}
#endif /* _LINUX_AVERAGE_H */
