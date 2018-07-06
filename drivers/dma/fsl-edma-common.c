// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2013-2014 Freescale Semiconductor, Inc
// Copyright (c) 2017 Sysam, Angelo Dureghello  <angelo@sysam.it>

#include <linux/dmapool.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "fsl-edma-common.h"

#define EDMA_CR			0x00
#define EDMA_ES			0x04
#define EDMA_ERQ		0x0C
#define EDMA_EEI		0x14
#define EDMA_CEEI		0x18
#define EDMA_SEEI		0x19
#define EDMA_CERQ		0x1A
#define EDMA_SERQ		0x1B
#define EDMA_CINT		0x1F
#define EDMA_CERR		0x1E
#define EDMA_SSRT		0x1D
#define EDMA_CDNE		0x1C
#define EDMA_INTR		0x24
#define EDMA_ERR		0x2C

#define EDMA64_ERQH		0x08
#define EDMA64_EEIH		0x10
#define EDMA64_SERQ		0x18
#define EDMA64_CERQ		0x19
#define EDMA64_SEEI		0x1a
#define EDMA64_CEEI		0x1b
#define EDMA64_CINT		0x1c
#define EDMA64_CERR		0x1d
#define EDMA64_SSRT		0x1e
#define EDMA64_CDNE		0x1f
#define EDMA64_INTH		0x20
#define EDMA64_INTL		0x24
#define EDMA64_ERRH		0x28
#define EDMA64_ERRL		0x2c

#define EDMA_TCD		0x1000
#define EDMA_TCD_SIZE		32
#define EDMA_TCD_MEM_ALIGN	EDMA_TCD_SIZE

#define EDMA_TCD_ATTR_SSIZE_8BIT	0
#define EDMA_TCD_ATTR_SSIZE_16BIT	BIT(0)
#define EDMA_TCD_ATTR_SSIZE_32BIT	BIT(1)
#define EDMA_TCD_ATTR_SSIZE_64BIT	(BIT(1) | BIT(0))
#define EDMA_TCD_ATTR_SSIZE_16BYTE	BIT(2)
#define EDMA_TCD_ATTR_SSIZE_32BYTE	(BIT(2) | BIT(0))
#define EDMA_TCD_ATTR_DSIZE_8BIT	(EDMA_TCD_ATTR_SSIZE_8BIT << 8)
#define EDMA_TCD_ATTR_DSIZE_16BIT	(EDMA_TCD_ATTR_SSIZE_16BIT << 8)
#define EDMA_TCD_ATTR_DSIZE_32BIT	(EDMA_TCD_ATTR_SSIZE_32BIT << 8)
#define EDMA_TCD_ATTR_DSIZE_64BIT	(EDMA_TCD_ATTR_SSIZE_64BIT << 8)
#define EDMA_TCD_ATTR_DSIZE_16BYTE	(EDMA_TCD_ATTR_SSIZE_16BYTE << 8)
#define EDMA_TCD_ATTR_DSIZE_32BYTE	(EDMA_TCD_ATTR_SSIZE_32BYTE << 8)

#define EDMA_TCD_CITER_CITER(x)		((x) & GENMASK(14, 0))
#define EDMA_TCD_BITER_BITER(x)		((x) & GENMASK(14, 0))

#define EDMA_TCD_CSR_START		BIT(0)
#define EDMA_TCD_CSR_INT_MAJOR		BIT(1)
#define EDMA_TCD_CSR_INT_HALF		BIT(2)
#define EDMA_TCD_CSR_D_REQ		BIT(3)
#define EDMA_TCD_CSR_E_SG		BIT(4)
#define EDMA_TCD_CSR_E_LINK		BIT(5)
#define EDMA_TCD_CSR_ACTIVE		BIT(6)
#define EDMA_TCD_CSR_DONE		BIT(7)

struct fsl_edma_chan *to_fsl_edma_chan(struct dma_chan *chan)
{
	return container_of(chan, struct fsl_edma_chan, vchan.chan);
}
EXPORT_SYMBOL_GPL(to_fsl_edma_chan);

struct fsl_edma_desc *to_fsl_edma_desc(struct virt_dma_desc *vd)
{
	return container_of(vd, struct fsl_edma_desc, vdesc);
}
EXPORT_SYMBOL_GPL(to_fsl_edma_desc);

/*
 * R/W functions for big- or little-endian registers:
 * The eDMA controller's endian is independent of the CPU core's endian.
 * For the big-endian IP module, the offset for 8-bit or 16-bit registers
 * should also be swapped opposite to that in little-endian IP.
 */
u32 edma_readl(struct fsl_edma_engine *edma,
			void __iomem *addr)
{
	if (edma->big_endian)
		return ioread32be(addr);
	else
		return ioread32(addr);
}
EXPORT_SYMBOL_GPL(edma_readl);

void edma_writeb(struct fsl_edma_engine *edma, u8 val,
			void __iomem *addr)
{
	/* swap the reg offset for these in big-endian mode */
	if (edma->big_endian)
		iowrite8(val, (void __iomem *)((unsigned long)addr ^ 0x3));
	else
		iowrite8(val, addr);
}
EXPORT_SYMBOL_GPL(edma_writeb);

void edma_writew(struct fsl_edma_engine *edma, u16 val,
			void __iomem *addr)
{
	/* swap the reg offset for these in big-endian mode */
	if (edma->big_endian)
		iowrite16be(val, (void __iomem *)((unsigned long)addr ^ 0x2));
	else
		iowrite16(val, addr);
}
EXPORT_SYMBOL_GPL(edma_writew);

void edma_writel(struct fsl_edma_engine *edma, u32 val,
			void __iomem *addr)
{
	if (edma->big_endian)
		iowrite32be(val, addr);
	else
		iowrite32(val, addr);
}
EXPORT_SYMBOL_GPL(edma_writel);

static void fsl_edma_fill_tcd(struct fsl_edma_hw_tcd *tcd,
			u32 src, u32 dst, u16 attr, u16 soff,
			u32 nbytes, u32 slast, u16 citer, u16 biter,
			u16 doff, u32 dlast_sga, bool major_int,
			bool disable_req, bool enable_sg)
{
	u16 csr = 0;

	/*
	 * eDMA hardware SGs require the TCDs to be stored in little
	 * endian format irrespective of the register endian model.
	 * So we put the value in little endian in memory, waiting
	 * for fsl_edma_set_tcd_regs doing the swap.
	 */
	tcd->saddr = cpu_to_le32(src);
	tcd->daddr = cpu_to_le32(dst);
	tcd->attr = cpu_to_le16(attr);
	tcd->soff = cpu_to_le16(soff);
	tcd->nbytes = cpu_to_le32(nbytes);
	tcd->slast = cpu_to_le32(slast);
	tcd->citer = cpu_to_le16(EDMA_TCD_CITER_CITER(citer));
	tcd->doff = cpu_to_le16(doff);
	tcd->dlast_sga = cpu_to_le32(dlast_sga);
	tcd->biter = cpu_to_le16(EDMA_TCD_BITER_BITER(biter));

	if (major_int)
		csr |= EDMA_TCD_CSR_INT_MAJOR;
	if (disable_req)
		csr |= EDMA_TCD_CSR_D_REQ;
	if (enable_sg)
		csr |= EDMA_TCD_CSR_E_SG;

	tcd->csr = cpu_to_le16(csr);
}

static void fsl_edma_enable_request(struct fsl_edma_chan *fsl_chan)
{
	struct edma_regs *regs = &fsl_chan->edma->regs;
	u32 ch = fsl_chan->vchan.chan.chan_id;

	if (fsl_chan->edma->version == v1) {
		edma_writeb(fsl_chan->edma, EDMA_SEEI_SEEI(ch), regs->seei);
		edma_writeb(fsl_chan->edma, ch, regs->serq);
	} else {
		/* ColdFire is big endian, and accesses natively
		 * big endian I/O peripherals
		 */
		iowrite8(EDMA_SEEI_SEEI(ch), regs->seei);
		iowrite8(ch, regs->serq);
	}
}

static unsigned int fsl_edma_get_tcd_attr(enum dma_slave_buswidth addr_width)
{
	switch (addr_width) {
	case 1:
		return EDMA_TCD_ATTR_SSIZE_8BIT | EDMA_TCD_ATTR_DSIZE_8BIT;
	case 2:
		return EDMA_TCD_ATTR_SSIZE_16BIT | EDMA_TCD_ATTR_DSIZE_16BIT;
	case 4:
		return EDMA_TCD_ATTR_SSIZE_32BIT | EDMA_TCD_ATTR_DSIZE_32BIT;
	case 8:
		return EDMA_TCD_ATTR_SSIZE_64BIT | EDMA_TCD_ATTR_DSIZE_64BIT;
	default:
		return EDMA_TCD_ATTR_SSIZE_32BIT | EDMA_TCD_ATTR_DSIZE_32BIT;
	}
}

static struct fsl_edma_desc *fsl_edma_alloc_desc(struct fsl_edma_chan *fsl_chan,
		int sg_len)
{
	struct fsl_edma_desc *fsl_desc;
	int i;

	fsl_desc = kzalloc(sizeof(*fsl_desc) +
			sizeof(struct fsl_edma_sw_tcd) * sg_len, GFP_NOWAIT);
	if (!fsl_desc)
		return NULL;

	fsl_desc->echan = fsl_chan;
	fsl_desc->n_tcds = sg_len;
	for (i = 0; i < sg_len; i++) {
		fsl_desc->tcd[i].vtcd = dma_pool_alloc(fsl_chan->tcd_pool,
					GFP_NOWAIT, &fsl_desc->tcd[i].ptcd);
		if (!fsl_desc->tcd[i].vtcd)
			goto err;
	}
	return fsl_desc;

err:
	while (--i >= 0)
		dma_pool_free(fsl_chan->tcd_pool, fsl_desc->tcd[i].vtcd,
				fsl_desc->tcd[i].ptcd);
	kfree(fsl_desc);
	return NULL;
}

static void fsl_edma_set_tcd_regs(struct fsl_edma_chan *fsl_chan,
			   struct fsl_edma_hw_tcd *tcd)
{
	struct fsl_edma_engine *edma = fsl_chan->edma;
	struct fsl_edma_hw_tcd *mtcd = edma->regs.tcd
					+ fsl_chan->vchan.chan.chan_id;

	/*
	 * TCD parameters are stored in struct fsl_edma_hw_tcd in little
	 * endian format. However, we need to load the TCD registers in
	 * big- or little-endian obeying the eDMA engine model endian.
	 */
	edma_writel(edma, le32_to_cpu(tcd->saddr), &mtcd->saddr);
	edma_writel(edma, le32_to_cpu(tcd->daddr), &mtcd->daddr);
	edma_writew(edma, le16_to_cpu(tcd->attr), &mtcd->attr);
	edma_writew(edma, le16_to_cpu(tcd->soff), &mtcd->soff);
	edma_writel(edma, le32_to_cpu(tcd->nbytes), &mtcd->nbytes);
	edma_writel(edma, le32_to_cpu(tcd->slast), &mtcd->slast);
	edma_writew(edma, le16_to_cpu(tcd->citer), &mtcd->citer);
	edma_writew(edma, le16_to_cpu(tcd->biter), &mtcd->biter);
	edma_writew(edma, le16_to_cpu(tcd->doff), &mtcd->doff);
	edma_writel(edma, le32_to_cpu(tcd->dlast_sga), &mtcd->dlast_sga);
	edma_writew(edma, le16_to_cpu(tcd->csr), &mtcd->csr);
}

static size_t fsl_edma_desc_residue(struct fsl_edma_chan *fsl_chan,
		struct virt_dma_desc *vdesc, bool in_progress)
{
	struct fsl_edma_desc *edesc = fsl_chan->edesc;
	struct edma_regs *regs = &fsl_chan->edma->regs;
	u32 ch = fsl_chan->vchan.chan.chan_id;
	enum dma_transfer_direction dir = fsl_chan->dir;
	dma_addr_t cur_addr, dma_addr;
	size_t len, size;
	int i;

	/* calculate the total size in this desc */
	for (len = i = 0; i < fsl_chan->edesc->n_tcds; i++)
		len += le32_to_cpu(edesc->tcd[i].vtcd->nbytes)
			* le16_to_cpu(edesc->tcd[i].vtcd->biter);

	if (!in_progress)
		return len;

	if (dir == DMA_MEM_TO_DEV)
		cur_addr = edma_readl(fsl_chan->edma, &regs->tcd[ch].saddr);
	else
		cur_addr = edma_readl(fsl_chan->edma, &regs->tcd[ch].daddr);

	/* figure out the finished and calculate the residue */
	for (i = 0; i < fsl_chan->edesc->n_tcds; i++) {
		size = le32_to_cpu(edesc->tcd[i].vtcd->nbytes)
			* le16_to_cpu(edesc->tcd[i].vtcd->biter);
		if (dir == DMA_MEM_TO_DEV)
			dma_addr = le32_to_cpu(edesc->tcd[i].vtcd->saddr);
		else
			dma_addr = le32_to_cpu(edesc->tcd[i].vtcd->daddr);

		len -= size;
		if (cur_addr >= dma_addr && cur_addr < dma_addr + size) {
			len += dma_addr + size - cur_addr;
			break;
		}
	}

	return len;
}

void fsl_edma_disable_request(struct fsl_edma_chan *fsl_chan)
{
	struct edma_regs *regs = &fsl_chan->edma->regs;
	u32 ch = fsl_chan->vchan.chan.chan_id;

	if (fsl_chan->edma->version == v1) {
		edma_writeb(fsl_chan->edma, ch, regs->cerq);
		edma_writeb(fsl_chan->edma, EDMA_CEEI_CEEI(ch), regs->ceei);
	} else {
		/* ColdFire is big endian, and accesses natively
		 * big endian I/O peripherals
		 */
		iowrite8(ch, regs->cerq);
		iowrite8(EDMA_CEEI_CEEI(ch), regs->ceei);
	}
}
EXPORT_SYMBOL_GPL(fsl_edma_disable_request);

void fsl_edma_cleanup_vchan(struct dma_device *dmadev)
{
	struct fsl_edma_chan *chan, *_chan;

	list_for_each_entry_safe(chan, _chan,
				&dmadev->channels, vchan.chan.device_node) {
		list_del(&chan->vchan.chan.device_node);
		tasklet_kill(&chan->vchan.task);
	}
}
EXPORT_SYMBOL_GPL(fsl_edma_cleanup_vchan);

void fsl_edma_free_desc(struct virt_dma_desc *vdesc)
{
	struct fsl_edma_desc *mcf_desc;
	int i;

	mcf_desc = to_fsl_edma_desc(vdesc);

	for (i = 0; i < mcf_desc->n_tcds; i++)
		dma_pool_free(mcf_desc->echan->tcd_pool, mcf_desc->tcd[i].vtcd,
				mcf_desc->tcd[i].ptcd);
	kfree(mcf_desc);
}
EXPORT_SYMBOL_GPL(fsl_edma_free_desc);

int fsl_edma_alloc_chan_resources(struct dma_chan *chan)
{
	struct fsl_edma_chan *mcf_chan = to_fsl_edma_chan(chan);

	mcf_chan->tcd_pool = dma_pool_create("tcd_pool", chan->device->dev,
			sizeof(struct fsl_edma_hw_tcd), EDMA_TCD_MEM_ALIGN, 0);

	return 0;
}
EXPORT_SYMBOL_GPL(fsl_edma_alloc_chan_resources);

void fsl_edma_free_chan_resources(struct dma_chan *chan)
{
	struct fsl_edma_chan *fsl_chan = to_fsl_edma_chan(chan);
	unsigned long flags;
	LIST_HEAD(head);

	spin_lock_irqsave(&fsl_chan->vchan.lock, flags);
	fsl_edma_disable_request(fsl_chan);
	fsl_chan->edesc = NULL;
	vchan_get_all_descriptors(&fsl_chan->vchan, &head);
	spin_unlock_irqrestore(&fsl_chan->vchan.lock, flags);

	vchan_dma_desc_free_list(&fsl_chan->vchan, &head);
	dma_pool_destroy(fsl_chan->tcd_pool);
	fsl_chan->tcd_pool = NULL;
}
EXPORT_SYMBOL_GPL(fsl_edma_free_chan_resources);

int fsl_edma_slave_config(struct dma_chan *chan,
			  struct dma_slave_config *config)
{
	struct fsl_edma_chan *fsl_chan = to_fsl_edma_chan(chan);

	memcpy(&fsl_chan->cfg, config, sizeof(*config));

	return 0;
}
EXPORT_SYMBOL_GPL(fsl_edma_slave_config);

struct dma_async_tx_descriptor *fsl_edma_prep_dma_cyclic(
		struct dma_chan *chan, dma_addr_t dma_addr, size_t buf_len,
		size_t period_len, enum dma_transfer_direction direction,
		unsigned long flags)
{
	struct fsl_edma_chan *fsl_chan = to_fsl_edma_chan(chan);
	struct fsl_edma_desc *fsl_desc;
	dma_addr_t dma_buf_next;
	int sg_len, i;
	u32 src_addr, dst_addr, last_sg, nbytes, attr;
	u16 soff, doff, iter;

	if (!is_slave_direction(direction))
		return NULL;

	sg_len = buf_len / period_len;
	fsl_desc = fsl_edma_alloc_desc(fsl_chan, sg_len);
	if (!fsl_desc)
		return NULL;

	fsl_desc->iscyclic = true;
	fsl_chan->dir = direction;

	dma_buf_next = dma_addr;

	for (i = 0; i < sg_len; i++) {
		if (dma_buf_next >= dma_addr + buf_len)
			dma_buf_next = dma_addr;

		/* get next sg's physical address */
		last_sg = fsl_desc->tcd[(i + 1) % sg_len].ptcd;

		if (direction == DMA_MEM_TO_DEV) {
			nbytes = fsl_chan->cfg.dst_addr_width *
				 fsl_chan->cfg.dst_maxburst;
			src_addr = dma_buf_next;
			dst_addr = fsl_chan->cfg.dst_addr;
			soff = fsl_chan->cfg.dst_addr_width;
			doff = 0;
			attr = fsl_edma_get_tcd_attr(
				fsl_chan->cfg.dst_addr_width);
		} else {
			nbytes = fsl_chan->cfg.src_addr_width *
				 fsl_chan->cfg.src_maxburst;
			src_addr =  fsl_chan->cfg.src_addr;
			dst_addr = dma_buf_next;
			soff = 0;
			doff = fsl_chan->cfg.src_addr_width;
			attr = fsl_edma_get_tcd_attr(
				fsl_chan->cfg.src_addr_width);
		}

		iter = period_len / nbytes;

		fsl_edma_fill_tcd(fsl_desc->tcd[i].vtcd,
				  src_addr, dst_addr,
				  attr, soff, nbytes, 0, iter,
				  iter, doff, last_sg, true, false, true);
		dma_buf_next += period_len;
	}

	return vchan_tx_prep(&fsl_chan->vchan, &fsl_desc->vdesc, flags);
}
EXPORT_SYMBOL_GPL(fsl_edma_prep_dma_cyclic);

struct dma_async_tx_descriptor *fsl_edma_prep_slave_sg(
		struct dma_chan *chan, struct scatterlist *sgl,
		unsigned int sg_len, enum dma_transfer_direction direction,
		unsigned long flags, void *context)
{
	struct fsl_edma_chan *fsl_chan = to_fsl_edma_chan(chan);
	struct fsl_edma_desc *fsl_desc;
	struct scatterlist *sg;
	u32 src_addr, dst_addr, last_sg, nbytes, attr;
	u16 soff, doff, iter;
	int i;

	if (!is_slave_direction(direction))
		return NULL;

	fsl_desc = fsl_edma_alloc_desc(fsl_chan, sg_len);
	if (!fsl_desc)
		return NULL;

	fsl_desc->iscyclic = false;
	fsl_chan->dir = direction;

	for_each_sg(sgl, sg, sg_len, i) {
		/* get next sg's physical address */
		last_sg = fsl_desc->tcd[(i + 1) % sg_len].ptcd;

		if (direction == DMA_MEM_TO_DEV) {
			nbytes = fsl_chan->cfg.dst_addr_width *
				 fsl_chan->cfg.dst_maxburst;
			src_addr = sg_dma_address(sg);
			dst_addr = fsl_chan->cfg.dst_addr;
			soff = fsl_chan->cfg.dst_addr_width;
			doff = 0;
			attr = fsl_edma_get_tcd_attr(
					fsl_chan->cfg.dst_addr_width);

		} else {
			nbytes = fsl_chan->cfg.src_addr_width *
				 fsl_chan->cfg.src_maxburst;
			src_addr = fsl_chan->cfg.src_addr;
			dst_addr = sg_dma_address(sg);
			soff = 0;
			doff = fsl_chan->cfg.src_addr_width;
			attr = fsl_edma_get_tcd_attr(
					fsl_chan->cfg.src_addr_width);
		}

		iter = sg_dma_len(sg) / nbytes;

		if (i < sg_len - 1) {
			last_sg = fsl_desc->tcd[(i + 1)].ptcd;
			fsl_edma_fill_tcd(fsl_desc->tcd[i].vtcd,
					  src_addr,
					  dst_addr, attr, soff,
					  nbytes, 0, iter, iter, doff, last_sg,
					  false, false, true);
		} else {
			last_sg = 0;
			fsl_edma_fill_tcd(fsl_desc->tcd[i].vtcd,
					  src_addr,
					  dst_addr, attr, soff,
					  nbytes, 0, iter, iter, doff, last_sg,
					  true, true, false);
		}
	}

	return vchan_tx_prep(&fsl_chan->vchan, &fsl_desc->vdesc, flags);
}
EXPORT_SYMBOL_GPL(fsl_edma_prep_slave_sg);

enum dma_status fsl_edma_tx_status(struct dma_chan *chan,
		dma_cookie_t cookie, struct dma_tx_state *txstate)
{
	struct fsl_edma_chan *fsl_chan = to_fsl_edma_chan(chan);
	struct virt_dma_desc *vdesc;
	enum dma_status status;
	unsigned long flags;

	status = dma_cookie_status(chan, cookie, txstate);
	if (status == DMA_COMPLETE)
		return status;

	if (!txstate)
		return fsl_chan->status;

	spin_lock_irqsave(&fsl_chan->vchan.lock, flags);
	vdesc = vchan_find_desc(&fsl_chan->vchan, cookie);
	if (fsl_chan->edesc && cookie == fsl_chan->edesc->vdesc.tx.cookie)
		txstate->residue =
				fsl_edma_desc_residue(fsl_chan, vdesc, true);
	else if (vdesc)
		txstate->residue =
				fsl_edma_desc_residue(fsl_chan, vdesc, false);
	else
		txstate->residue = 0;

	spin_unlock_irqrestore(&fsl_chan->vchan.lock, flags);

	return fsl_chan->status;
}
EXPORT_SYMBOL_GPL(fsl_edma_tx_status);

int fsl_edma_pause(struct dma_chan *chan)
{
	struct fsl_edma_chan *fsl_chan = to_fsl_edma_chan(chan);
	unsigned long flags;

	spin_lock_irqsave(&fsl_chan->vchan.lock, flags);
	if (fsl_chan->edesc) {
		fsl_edma_disable_request(fsl_chan);
		fsl_chan->status = DMA_PAUSED;
		fsl_chan->idle = true;
	}
	spin_unlock_irqrestore(&fsl_chan->vchan.lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(fsl_edma_pause);

int fsl_edma_resume(struct dma_chan *chan)
{
	struct fsl_edma_chan *fsl_chan = to_fsl_edma_chan(chan);
	unsigned long flags;

	spin_lock_irqsave(&fsl_chan->vchan.lock, flags);
	if (fsl_chan->edesc) {
		fsl_edma_enable_request(fsl_chan);
		fsl_chan->status = DMA_IN_PROGRESS;
		fsl_chan->idle = false;
	}
	spin_unlock_irqrestore(&fsl_chan->vchan.lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(fsl_edma_resume);

int fsl_edma_terminate_all(struct dma_chan *chan)
{
	struct fsl_edma_chan *fsl_chan = to_fsl_edma_chan(chan);
	unsigned long flags;
	LIST_HEAD(head);

	spin_lock_irqsave(&fsl_chan->vchan.lock, flags);
	fsl_edma_disable_request(fsl_chan);
	fsl_chan->edesc = NULL;
	fsl_chan->idle = true;
	vchan_get_all_descriptors(&fsl_chan->vchan, &head);
	spin_unlock_irqrestore(&fsl_chan->vchan.lock, flags);
	vchan_dma_desc_free_list(&fsl_chan->vchan, &head);
	return 0;
}
EXPORT_SYMBOL_GPL(fsl_edma_terminate_all);

void fsl_edma_issue_pending(struct dma_chan *chan)
{
	struct fsl_edma_chan *mcf_chan = to_fsl_edma_chan(chan);
	unsigned long flags;

	spin_lock_irqsave(&mcf_chan->vchan.lock, flags);

	if (vchan_issue_pending(&mcf_chan->vchan) && !mcf_chan->edesc)
		fsl_edma_xfer_desc(mcf_chan);

	spin_unlock_irqrestore(&mcf_chan->vchan.lock, flags);
}
EXPORT_SYMBOL_GPL(fsl_edma_issue_pending);

void fsl_edma_xfer_desc(struct fsl_edma_chan *fsl_chan)
{
	struct virt_dma_desc *vdesc;

	vdesc = vchan_next_desc(&fsl_chan->vchan);
	if (!vdesc)
		return;
	fsl_chan->edesc = to_fsl_edma_desc(vdesc);
	fsl_edma_set_tcd_regs(fsl_chan, fsl_chan->edesc->tcd[0].vtcd);
	fsl_edma_enable_request(fsl_chan);
	fsl_chan->status = DMA_IN_PROGRESS;
	fsl_chan->idle = false;
}
EXPORT_SYMBOL_GPL(fsl_edma_xfer_desc);

/*
 * On the 32 channels Vybrid/mpc577x edma version (here called "v1"),
 * register offsets are different compared to ColdFire mcf5441x 64 channels
 * edma (here called "v2").
 *
 * This function sets up register offsets as per proper declared version
 * so must be called in xxx_edma_probe() just after setting the
 * edma "version" and "membase" appropriately.
 */
void fsl_edma_setup_regs(struct fsl_edma_engine *edma)
{
	edma->regs.cr = edma->membase + EDMA_CR;
	edma->regs.es = edma->membase + EDMA_ES;
	edma->regs.erql = edma->membase + EDMA_ERQ;
	edma->regs.eeil = edma->membase + EDMA_EEI;

	edma->regs.serq = edma->membase + ((edma->version == v1) ?
			EDMA_SERQ : EDMA64_SERQ);
	edma->regs.cerq = edma->membase + ((edma->version == v1) ?
			EDMA_CERQ : EDMA64_CERQ);
	edma->regs.seei = edma->membase + ((edma->version == v1) ?
			EDMA_SEEI : EDMA64_SEEI);
	edma->regs.ceei = edma->membase + ((edma->version == v1) ?
			EDMA_CEEI : EDMA64_CEEI);
	edma->regs.cint = edma->membase + ((edma->version == v1) ?
			EDMA_CINT : EDMA64_CINT);
	edma->regs.cerr = edma->membase + ((edma->version == v1) ?
			EDMA_CERR : EDMA64_CERR);
	edma->regs.ssrt = edma->membase + ((edma->version == v1) ?
			EDMA_SSRT : EDMA64_SSRT);
	edma->regs.cdne = edma->membase + ((edma->version == v1) ?
			EDMA_CDNE : EDMA64_CDNE);
	edma->regs.intl = edma->membase + ((edma->version == v1) ?
			EDMA_INTR : EDMA64_INTL);
	edma->regs.errl = edma->membase + ((edma->version == v1) ?
			EDMA_ERR : EDMA64_ERRL);

	if (edma->version == v2) {
		edma->regs.erqh = edma->membase + EDMA64_ERQH;
		edma->regs.eeih = edma->membase + EDMA64_EEIH;
		edma->regs.errh = edma->membase + EDMA64_ERRH;
		edma->regs.inth = edma->membase + EDMA64_INTH;
	}

	edma->regs.tcd = edma->membase + EDMA_TCD;
}
EXPORT_SYMBOL_GPL(fsl_edma_setup_regs);

MODULE_LICENSE("GPL v2");
