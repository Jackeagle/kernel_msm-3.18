# SPDX-License-Identifier: GPL-2.0

obj-$(CONFIG_UNICODE) += unicode.o
obj-$(CONFIG_UNICODE_NORMALIZATION_SELFTEST) += utf8-selftest.o

unicode-y := utf8-norm.o utf8-core.o


# To regenerate utf8data.h, run the following in the top directory:
#   $ make REGENERATE_UTF8DATA=1 fs/unicode/
ifdef REGENERATE_UTF8DATA

$(obj)/utf8-norm.o: $(obj)/utf8data.h

quiet_cmd_utf8data = GEN     $@
      cmd_utf8data = $(obj)/mkutf8data \
		-a $(srctree)/$(src)/DerivedAge.txt \
		-c $(srctree)/$(src)/DerivedCombiningClass.txt \
		-p $(srctree)/$(src)/DerivedCoreProperties.txt \
		-d $(srctree)/$(src)/UnicodeData.txt \
		-f $(srctree)/$(src)/CaseFolding.txt \
		-n $(srctree)/$(src)/NormalizationCorrections.txt \
		-t $(srctree)/$(src)/NormalizationTest.txt \
		-o $@

$(obj)/utf8data.h: $(filter %.txt, $(cmd_utf8data)) $(obj)/mkutf8data FORCE
	$(call if_changed,utf8data)

always += utf8data.h
no-clean-files += utf8data.h

endif

hostprogs-y += mkutf8data
