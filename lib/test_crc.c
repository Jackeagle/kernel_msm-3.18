// SPDX-License-Identifier: GPL-2.0
/*
 * CRC test driver
 *
 * Copyright (C) 2018 Coly Li <colyli@suse.de>
 *
 * This module provides an simple framework to check the consistency of
 * Linux kernel CRC calculation routines in lib/crc*.c. This driver
 * requires CONFIG_CRC* items to be enabled if the associated routines are
 * tested here. The test results will be printed to kernel message
 * when this test driver is loaded.
 *
 * Current test routines are,
 * - crc64_be()
 */

#include <linux/module.h>
#include <linux/crc64.h>

struct crc_test_record {
	char	*name;
	u64	data[4];
	u64	initval;
	u64	expval;
	void	(*handler)(struct crc_test_record *rec);
};

int failed_tests;
int total_tests;

static void chk_and_msg(const char *name, u64 crc, u64 expval)
{
	total_tests++;
	if (crc == expval)
		return;

	pr_err("test_crc: %s: FAILED:(0x%016llx, expected 0x%016llx)\n",
		name, crc, expval);
	failed_tests++;
}

/* Add your crc test cases here */
static void test_crc64_be(struct crc_test_record *rec)
{
	u64 crc;

	crc = crc64_be(rec->initval, rec->data, sizeof(rec->data));
	chk_and_msg(rec->name, crc, rec->expval);
}

/*
 * Set up your crc test initial data here.
 * Do not change the existing items, they are hard coded with
 * pre-calculated values.
 */
static struct crc_test_record test_data[] = {
	{ .name		= "crc64_be",
	  .data		= { 0x42F0E1EBA9EA3693, 0x85E1C3D753D46D26,
			    0xC711223CFA3E5BB5, 0x493366450E42ECDF },
	  .initval	= 0x61C8864680B583EB,
	  .expval	= 0xb2c863673f4292bf,
	  .handler	= test_crc64_be,
	},
	{}
};

static int __init test_crc_init(void)
{
	int i;

	failed_tests = 0;
	total_tests = 0;

	pr_info("Kernel CRC consistency testing:\n");
	for (i = 0; test_data[i].name; i++)
		test_data[i].handler(&test_data[i]);

	if (failed_tests == 0)
		pr_info("test_crc: all %d tests passed\n", i);
	else
		pr_err("test_crc: %d cases tested, %d passed, %d failed\n",
		       total_tests, total_tests - failed_tests, failed_tests);

	return (failed_tests == 0) ? 0 : -EINVAL;
}
late_initcall(test_crc_init);

static void __exit test_crc_exit(void) { }
module_exit(test_crc_exit);

MODULE_DESCRIPTION("CRC consistency testing driver");
MODULE_AUTHOR("Coly Li <colyli@suse.de>");
MODULE_LICENSE("GPL v2");
