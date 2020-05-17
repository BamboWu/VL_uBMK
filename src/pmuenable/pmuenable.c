/*
 * Kernel module to enable user space access for arm PMU v3
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <asm-generic/int-ll64.h>

#define MODULE_VER "1.0"
#define MODULE_NM "pmuenable"

static void enable_cpu_counters(void* data)
{
  __asm__ volatile (
      "mov x8,             0xd \n\r"
      "msr PMUSERENR_EL0,   x8 \n\r"
      :
      :
      : "x8"
      );
}

static void disable_cpu_counters(void* data)
{
  __asm__ volatile (
      "mov x8,             0x0 \n\r"
      "msr PMUSERENR_EL0,   x8 \n\r"
      :
      :
      : "x8"
      );
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * function: init_pmuenable
 */

static int __init init_pmuenable(void)
{
  printk("[KM %s] Start initializing \n", MODULE_NM);

  on_each_cpu(enable_cpu_counters, NULL, 1);

  printk(KERN_INFO "[KM %s] written 0xd to PMUSERENR_EL0\n", MODULE_NM);

  return 0;
}

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * function: cleanup_pmuenable
 */

static void __exit cleanup_pmuenable(void)
{
  on_each_cpu(disable_cpu_counters, NULL, 1);

  printk(KERN_INFO "[KM %s] written 0x0 to PMUSERENR_EL0\n", MODULE_NM);
}

module_init(init_pmuenable);
module_exit(cleanup_pmuenable);

MODULE_AUTHOR("Qinzhe Wu");
MODULE_DESCRIPTION("enable PMU for user space access");
MODULE_LICENSE("GPL");
