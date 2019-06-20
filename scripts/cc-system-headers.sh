#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only

cat << "END" | $@ -E -x c - -o /dev/null >/dev/null 2>&1
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/time.h>
END
