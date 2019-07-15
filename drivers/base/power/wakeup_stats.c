// SPDX-License-Identifier: GPL-2.0
/*
 * Wakeup statistics in sysfs
 *
 * Copyright (c) 2019 Linux Foundation
 * Copyright (c) 2019 Greg Kroah-Hartman <gregkh@linuxfoundation.org>
 * Copyright (c) 2019 Google Inc.
 */

#include <linux/slab.h>
#include <linux/kdev_t.h>

#include "power.h"

static struct class *wakeup_class;

#define wakeup_attr(_name)						\
static ssize_t _name##_show(struct device *dev,				\
			    struct device_attribute *attr, char *buf)	\
{									\
	struct wakeup_source *ws = dev_get_drvdata(dev);		\
									\
	return sprintf(buf, "%lu\n", ws->_name);			\
}									\
static DEVICE_ATTR_RO(_name)

wakeup_attr(active_count);
wakeup_attr(event_count);
wakeup_attr(wakeup_count);
wakeup_attr(expire_count);

static ssize_t active_time_ms_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct wakeup_source *ws = dev_get_drvdata(dev);
	ktime_t active_time =
		ws->active ? ktime_sub(ktime_get(), ws->last_time) : 0;

	return sprintf(buf, "%lld\n", ktime_to_ms(active_time));
}
static DEVICE_ATTR_RO(active_time_ms);

static ssize_t total_time_ms_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct wakeup_source *ws = dev_get_drvdata(dev);
	ktime_t active_time;
	ktime_t total_time = ws->total_time;

	if (ws->active) {
		active_time = ktime_sub(ktime_get(), ws->last_time);
		total_time = ktime_add(total_time, active_time);
	}
	return sprintf(buf, "%lld\n", ktime_to_ms(total_time));
}
static DEVICE_ATTR_RO(total_time_ms);

static ssize_t max_time_ms_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct wakeup_source *ws = dev_get_drvdata(dev);
	ktime_t active_time;
	ktime_t max_time = ws->max_time;

	if (ws->active) {
		active_time = ktime_sub(ktime_get(), ws->last_time);
		if (active_time > max_time)
			max_time = active_time;
	}
	return sprintf(buf, "%lld\n", ktime_to_ms(max_time));
}
static DEVICE_ATTR_RO(max_time_ms);

static ssize_t last_change_ms_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct wakeup_source *ws = dev_get_drvdata(dev);

	return sprintf(buf, "%lld\n", ktime_to_ms(ws->last_time));
}
static DEVICE_ATTR_RO(last_change_ms);

static ssize_t prevent_suspend_time_ms_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct wakeup_source *ws = dev_get_drvdata(dev);
	ktime_t prevent_sleep_time = ws->prevent_sleep_time;

	if (ws->active && ws->autosleep_enabled) {
		prevent_sleep_time = ktime_add(prevent_sleep_time,
			ktime_sub(ktime_get(), ws->start_prevent_time));
	}
	return sprintf(buf, "%lld\n", ktime_to_ms(prevent_sleep_time));
}
static DEVICE_ATTR_RO(prevent_suspend_time_ms);

static struct attribute *wakeup_source_attrs[] = {
	&dev_attr_active_count.attr,
	&dev_attr_event_count.attr,
	&dev_attr_wakeup_count.attr,
	&dev_attr_expire_count.attr,
	&dev_attr_active_time_ms.attr,
	&dev_attr_total_time_ms.attr,
	&dev_attr_max_time_ms.attr,
	&dev_attr_last_change_ms.attr,
	&dev_attr_prevent_suspend_time_ms.attr,
	NULL,
};
ATTRIBUTE_GROUPS(wakeup_source);

/**
 * wakeup_source_sysfs_add - Add wakeup_source attributes to sysfs.
 * @ws: Wakeup source to be added in sysfs.
 */
int wakeup_source_sysfs_add(struct wakeup_source *ws)
{
	struct device *dev;

	dev = device_create_with_groups(wakeup_class, NULL, MKDEV(0, 0), ws,
					wakeup_source_groups, "%s", ws->name);
	if (IS_ERR(dev))
		return PTR_ERR(dev);

	ws->dev = dev;
	return 0;
}
EXPORT_SYMBOL_GPL(wakeup_source_sysfs_add);

/**
 * wakeup_source_sysfs_remove - Remove wakeup_source attributes from sysfs.
 * @ws: Wakeup source to be removed from sysfs.
 */
void wakeup_source_sysfs_remove(struct wakeup_source *ws)
{
	device_unregister(ws->dev);
}
EXPORT_SYMBOL_GPL(wakeup_source_sysfs_remove);

static int __init wakeup_sources_sysfs_init(void)
{
	wakeup_class = class_create(THIS_MODULE, "wakeup");
	return PTR_ERR_OR_ZERO(wakeup_class);
}

postcore_initcall(wakeup_sources_sysfs_init);
