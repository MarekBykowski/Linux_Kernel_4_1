#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/reboot.h>

#include <linux/io.h>
#include <linux/proc_fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#define VERBOSE 0 /*VERBOSE is less robust*/
#if VERBOSE
#define pr_ccn(fmt, ...) pr_crit("ccn504: " fmt, ##__VA_ARGS__);
#else
#define pr_ccn(fmt, ...)
#endif

extern void __iomem *dickens;
static const u8 hnf[] = {
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27
};

static ssize_t
axxia_error_injection_trigger(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
       /*      	HNF0 0x20_0020_0000
      			HNF1 0x20_0021_0000
				etc up to 8
 				hnf_err_inj Register is at offset 0x0038
 				LPID 0b000 is logical processor id.
				SRCID 0b1 is RN-F id.
 		*/
#if 0
{
       unsigned i, value, srcid=1, lpid=1;
       #define HNF 0x200000
       for (i=0; i<=0x70000; i+=0x10000) {
               value=readl(DICKENS+HNF+i+0x0038);
               value |= (srcid <<16);
               value |= (lpid<<4); /*logical processor id*/
               value |= (1); /*enable*/
               printf("mb: 0x%x @ 0x%x\n", value, DICKENS+HNF+i+0x0038);
               writel(value, DICKENS+HNF+i+0x0038);
       }
}
#endif
	int i;
	u32 value;

#define     DKN_CLUSTER0_NODE       (1)
#define     DKN_CLUSTER1_NODE       (9)
#define     DKN_CLUSTER2_NODE       (11)
#define     DKN_CLUSTER3_NODE       (19)
	u32 srcid=DKN_CLUSTER1_NODE;

#define     CPUID0       (0)
#define     CPUID1       (1)
#define     CPUID2       (2)
#define     CPUID3       (3)
	u32 lpid=CPUID0;


	for (i = 0; i < 1/*ARRAY_SIZE(hnf)*/; ++i) {
		value = readl(dickens + (hnf[i] << 16) + 0x0038);
		value |= (srcid <<16);
		value |= (lpid<<4); /*logical processor id*/
		value |= (1); /*enable*/
		pr_ccn("write 0x%x @ %p\n", value,(void*) (dickens + (hnf[i] << 16) + 0x0038));
		trace_printk("mb: wrtie 0x%x @ %p\n", value,(void*) (dickens + (hnf[i] << 16) + 0x0038));
		writel(value, dickens + (hnf[i] << 16) + 0x0038);
	}

	return count;
}

static const struct file_operations axxia_error_injection_proc_ops = {
	.write      = axxia_error_injection_trigger,
	.llseek     = noop_llseek,
};

static unsigned mn = 0; /*misc node. Its offset is 0*/
static struct regmap *syscon;
extern void __iomem *base;
extern int ncr_reset_active;

#define CCN_MN_ERR_SIG_VAL_63_0 0x0300
#define CCN_MN_ERR_TYPE_VAL_63_0 0x0320
#define CCN_HNF_ERR_SYNDR_VAL_63_0(n) (0x0400 + ((n) * 8))
#define CCN_MN_ERRINT_STATUS__ALL_ERRORS__DISABLE 0x22
#define L3_DATA_DOUBLE_BIT_ECC_ERR 0x4
#define CCN_MN_ERRINT_STATUS 0x0008
#define CCN_HNF_ERROR_SYNDROME_CLEAR 0x0480
#define CCN_MN_ERRINT_STATUS_INTREQ_DESSERT 0x11

static irqreturn_t
axxia_edac_error_injection_handler(int irq_no, void *arg)
{
	int i;
	u32 err_sig_val[6];
	u32 err_type_val[8];
	u32 err_syndr_val[4];
	enum err_class_val_ { res0 = 0, res1, res2, fatal = 0x3 } err_class_val;

	/* Lets do it by the book "CCN-504, chapter 2.9.3 : Error handling requirements"
 	   Points 1,2,3,4ab, 5*/

	/* 1. Read the three Error Signal Valid registers.
          Error Signal valid registers are atomically cleared on a read. */
	for (i = 0; i < ARRAY_SIZE(err_sig_val); i++) {
		err_sig_val[i] = readl(dickens + mn +
				CCN_MN_ERR_SIG_VAL_63_0 + i * 4);
	}

	/* 2. Read the six Error Type registers. I couldn't find six only four...*/
	for (i = 0; i < ARRAY_SIZE(err_type_val); i++) {
		err_type_val[i] = readl(dickens + mn +
				CCN_MN_ERR_TYPE_VAL_63_0 + i * 4);
		err_type_val[i] = err_type_val[i];
	}

	/* 3. For each device x that has its err_sig_val_x bits set,
 	      read the applicable Error Syndrome 0 (and 1 if present) register/s.*/

	/* we will currently do it only for HNFs as we have injected errors to
 	   HNF/s (L3 cache) and we know the errors will be there.*/

	for (i = 0; i < ARRAY_SIZE(err_sig_val); i++) {
		if (i != 1)
			continue;
		/* err_sig_val[1] holds err signals asserted to hnf/s */
		if (err_sig_val[1] &= 0xff) {
			for (i = 0; i < ARRAY_SIZE(hnf); ++i) {
				/* read Err Syndr Regs 0 and 1*/
				err_syndr_val[0] = readl(dickens + (hnf[i] << 16) +
									CCN_HNF_ERR_SYNDR_VAL_63_0(0));
				err_syndr_val[1] = readl(dickens + (hnf[i] << 16) +
									CCN_HNF_ERR_SYNDR_VAL_63_0(0) + 4);
#define err_exntd 30
				/* I know HNF is Err Syndr Reg 1 too but this is to satisfy the "by the book" */
				if (err_syndr_val[1] >> err_exntd) {
					err_syndr_val[2] = readl(dickens + (hnf[i] << 16) +
									CCN_HNF_ERR_SYNDR_VAL_63_0(1));
					err_syndr_val[3] = readl(dickens + (hnf[i] << 16) +
									CCN_HNF_ERR_SYNDR_VAL_63_0(1) + 4);

					pr_ccn("faulty addr %08x\n", err_syndr_val[2]);
				}
#define err_class 27
				err_class_val = (err_syndr_val[1] >> err_class) & 0x3;
				pr_ccn("err_class %s\n", err_class_val == fatal ? "fatal" : "reserved" );
#define mult_err 26
				pr_ccn("mult_err %s\n", ((err_syndr_val[1] >> mult_err) && 0x1) ?
									"yes" : "no");

	/* 4ab. Write 1 to bits[62,59] of the Error Syndrome Clear register.
	   This clears the first_err_vld and mult_err bits */
				writel(0x48000000, dickens + (hnf[i] << 16) +
						   CCN_HNF_ERROR_SYNDROME_CLEAR + 4);

				/* if err_id == L3_DATA_DOUBLE_BIT_ECC_ERR */
				if ((err_syndr_val[0] & 0x7)
						== L3_DATA_DOUBLE_BIT_ECC_ERR) {
					/* log reset reason to pscratch reg */
					regmap_update_bits(syscon,
						   0xdc,
						   L3_DATA_DOUBLE_BIT_ECC_ERR,
						   L3_DATA_DOUBLE_BIT_ECC_ERR);

#if 1
					/* chip reset the board */
					writel(0x000000ab, base + 0x31000); /* Access Key */
					writel(0x00080802, base + 0x31008); /* Chip Reset */
					asm volatile("dsb sy\nisb\nwfi\n");
#endif
				}
			}
		}
	}

	/* 5. Write ones to the Error Interrupt Status register to deassert the interrupt */
	/* Honestly we should never get here as following the fatal errors and before
 	   here we reset the board */
	writel(CCN_MN_ERRINT_STATUS_INTREQ_DESSERT,
			dickens + mn + CCN_MN_ERRINT_STATUS);

	return IRQ_HANDLED;
}

void
axxia_l3_error_injection(void)
{
	static unsigned cookie, irq;
	int val;
	proc_create("axxia_edac_error_injection", S_IWUSR, NULL, &axxia_error_injection_proc_ops);

#define CCN_MN_ERRINT_STATUS__PMU_EVENTS__ENABLE	0x08
#define CCN_MN_ERRINT_STATUS__PMU_EVENTS__DISABLED	0x80
#define CCN_MN_ERRINT_STATUS__PMU_EVENTS__DISABLE	0x88

	/* Check if we can use the interrupt */
	writel(CCN_MN_ERRINT_STATUS__PMU_EVENTS__DISABLE,
			dickens + 0x0008);
	if (readl(dickens + 0x0008) &
			CCN_MN_ERRINT_STATUS__PMU_EVENTS__DISABLED) {
		/* Can set 'disable' bits, so can acknowledge interrupts */
		writel(CCN_MN_ERRINT_STATUS__PMU_EVENTS__ENABLE,
				dickens + 0x0008);

		/* fetch irq number off ccn504 node
		   TODO: check why irq_no is DICKENS_INTREQ (from 5500 manual) + 32 */
		irq = irq_of_parse_and_map(
			of_find_compatible_node(NULL, NULL,
            "arm,ccn-504"),
			0);

#if 0
		if (0 != request_threaded_irq(irq /* SharedInterruptNo: DICKENS_INTREQ+32*/,
						NULL,
						axxia_edac_error_injection_handler,
						IRQF_ONESHOT,
						"l3_ecc_error_inject",
						&cookie))
#else
		if (0 != request_irq(irq /* SharedInterruptNo: DICKENS_INTREQ+32*/,
						axxia_edac_error_injection_handler,
						0,
						"l3_ecc_error_inject",
						&cookie))
#endif
    		BUG_ON(1);

		pr_ccn("mb: can use the interrupts for ccn504\n");
		trace_printk("mb: can use the interrupts for ccn504\n");
	} else {
		pr_ccn("mb: cannot use the interrupts for ccn504\n");
		trace_printk("mb: cannot use the interrupts for ccn504\n");
	}

	syscon = syscon_regmap_lookup_by_compatible("syscon");
	regmap_read(syscon, 0xdc, &val);
	trace_printk("mb: syscon+0xdc (pscratch) 0x%x\n"
				"	-> if 0x4 then \"L3 data double-bit ECC error\"\n",
				val);

	return;
}
