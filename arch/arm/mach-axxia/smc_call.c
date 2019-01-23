#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/reboot.h>
#include <linux/syscore_ops.h>
#include <linux/proc_fs.h>

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/platform_device.h>
#include <linux/arm-smccc.h>

#include <linux/of.h>
#include <linux/io.h>
#include <linux/lsi-ncr.h>
#include "axxia.h"

static ssize_t
smc_call_op(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	/*
	void arm_smccc_smc(unsigned long a0, unsigned long a1, unsigned long a2,
       unsigned long a3, unsigned long a4, unsigned long a5,
       unsigned long a6, unsigned long a7, struct arm_smccc_res *res,
       struct arm_smccc_quirk *quirk)
	*/
	struct arm_smccc_res res;
	int cpu;
	for_each_possible_cpu(cpu)
		arm_smccc_smc(0x84000000, 0, 0, 0, 0, 0, 0, 0, &res);

	return count;
}

static const struct file_operations test_smc_call = {
	.write      = smc_call_op,
};

void
axxia_proc_create(void)
{
	proc_create("smc_call", S_IWUSR, NULL, &test_smc_call);
}
