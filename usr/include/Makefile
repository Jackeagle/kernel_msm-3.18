# SPDX-License-Identifier: GPL-2.0-only

# Unlike the kernel space, uapi headers are written in standard C.
#  - Forbid C++ style comments
#  - Use '__inline__', '__asm__' instead of 'inline', 'asm'
#
# -std=c90 (equivalent to -ansi) catches the violation of those.
# We cannot go as far as adding -Wpedantic since it emits too many warnings.
#
# REVISIT: re-consider the proper set of compiler flags for uapi compile-test.

UAPI_CFLAGS := -std=c90 -Wall -Werror=implicit-function-declaration

override c_flags = $(UAPI_CFLAGS) -Wp,-MD,$(depfile) -I$(objtree)/usr/include

# We want to compile as many headers as possible. We collect all headers
# by using the wildcard, then filter-out some later.
all-uapi-headers = $(shell cd $(obj) && find * -name '*.h')

# asm-generic/*.h is used by asm/*.h, and should not be included directly
no-header-test += asm-generic/%.h

# The following are excluded for now just because they fail to build.
# The cause of errors are mostly missing include directives.
# Check one by one, and send a patch to each subsystem.
#
# Do not add a new header to the list without legitimate reason.
# Please consider to fix the header first.
no-header-test += asm/ipcbuf.h
no-header-test += asm/msgbuf.h
no-header-test += asm/sembuf.h
no-header-test += asm/shmbuf.h
no-header-test += asm/signal.h
no-header-test += asm/ucontext.h
no-header-test += drm/vmwgfx_drm.h
no-header-test += linux/am437x-vpfe.h
no-header-test += linux/android/binderfs.h
no-header-test += linux/android/binder.h
no-header-test += linux/coda.h
no-header-test += linux/coda_psdev.h
no-header-test += linux/dvb/audio.h
no-header-test += linux/dvb/osd.h
no-header-test += linux/elfcore.h
no-header-test += linux/errqueue.h
no-header-test += linux/fsmap.h
no-header-test += linux/hdlc/ioctl.h
no-header-test += linux/jffs2.h
no-header-test += linux/kexec.h
no-header-test += linux/matroxfb.h
no-header-test += linux/netfilter_bridge/ebtables.h
no-header-test += linux/netfilter_ipv4/ipt_LOG.h
no-header-test += linux/netfilter_ipv6/ip6t_LOG.h
no-header-test += linux/nfc.h
no-header-test += linux/nilfs2_ondisk.h
no-header-test += linux/omap3isp.h
no-header-test += linux/omapfb.h
no-header-test += linux/patchkey.h
no-header-test += linux/phonet.h
no-header-test += linux/reiserfs_xattr.h
no-header-test += linux/scc.h
no-header-test += linux/sctp.h
no-header-test += linux/signal.h
no-header-test += linux/sysctl.h
no-header-test += linux/usb/audio.h
no-header-test += linux/ivtv.h
no-header-test += linux/v4l2-mediabus.h
no-header-test += linux/v4l2-subdev.h
no-header-test += linux/videodev2.h
no-header-test += linux/vm_sockets.h
no-header-test += misc/ocxl.h
no-header-test += scsi/scsi_bsg_fc.h
no-header-test += scsi/scsi_netlink_fc.h
no-header-test += scsi/scsi_netlink.h
no-header-test += sound/asequencer.h
no-header-test += sound/asound.h
no-header-test += sound/asoc.h
no-header-test += sound/compress_offload.h
no-header-test += sound/emu10k1.h
no-header-test += sound/sfnt_info.h
no-header-test += sound/sof/eq.h
no-header-test += sound/sof/fw.h
no-header-test += sound/sof/header.h
no-header-test += sound/sof/manifest.h
no-header-test += sound/sof/trace.h
no-header-test += xen/evtchn.h
no-header-test += xen/gntdev.h
no-header-test += xen/privcmd.h

# more headers are broken in some architectures

ifeq ($(SRCARCH),arc)
no-header-test += linux/bpf_perf_event.h
endif

ifeq ($(SRCARCH),ia64)
no-header-test += asm/setup.h
no-header-test += asm/sigcontext.h
no-header-test += asm/perfmon.h
no-header-test += asm/perfmon_default_smpl.h
no-header-test += linux/if_bonding.h
endif

ifeq ($(SRCARCH),mips)
no-header-test += asm/stat.h
endif

ifeq ($(SRCARCH),powerpc)
no-header-test += asm/stat.h
no-header-test += linux/bpf_perf_event.h
endif

ifeq ($(SRCARCH),riscv)
no-header-test += linux/bpf_perf_event.h
endif

ifeq ($(SRCARCH),s390)
no-header-test += asm/runtime_instr.h
no-header-test += asm/zcrypt.h
endif

ifeq ($(SRCARCH),sparc)
no-header-test += asm/stat.h
no-header-test += asm/uctx.h
no-header-test += asm/fbio.h
no-header-test += asm/openpromio.h
endif

# Use '=' instead of ':=' to avoid $(shell ...) evaluation when cleaning
header-test-y = $(filter-out $(no-header-test), $(all-uapi-headers))

# Use '=' instead of ':=' to avoid $(shell ...) evaluation when building
clean-dirs = $(shell cd $(obj) 2>/dev/null && find * -maxdepth 0 -type d)
