/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2013-2014 Freescale Semiconductor, Inc.
 * Copyright 2018 Angelo Dureghello <angelo@sysam.it>
 */
#ifndef _FSL_EDMA_COMMON_H_
#define _FSL_EDMA_COMMON_H_

#include <linux/types.h>

#include "virt-dma.h"

#define DMAMUX_NR		2

#define EDMA_CR_EDBG		BIT(1)
#define EDMA_CR_ERCA		BIT(2)
#define EDMA_CR_ERGA		BIT(3)
#define EDMA_CR_HOE		BIT(4)
#define EDMA_CR_HALT		BIT(5)
#define EDMA_CR_CLM		BIT(6)
#define EDMA_CR_EMLM		BIT(7)
#define EDMA_CR_ECX		BIT(16)
#define EDMA_CR_CX		BIT(17)

#define EDMA_SEEI_SEEI(x)	((x) & GENMASK(6, 0))
#define EDMA_CEEI_CEEI(x)	((x) & GENMASK(6, 0))
#define EDMA_CINT_CINT(x)	((x) & GENMASK(6, 0))
#define EDMA_CERR_CERR(x)	((x) & GENMASK(6, 0))

#define FSL_EDMA_BUSWIDTHS	(BIT(DMA_SLAVE_BUSWIDTH_1_BYTE) | \
				 BIT(DMA_SLAVE_BUSWIDTH_2_BYTES) | \
				 BIT(DMA_SLAVE_BUSWIDTH_4_BYTES) | \
				 BIT(DMA_SLAVE_BUSWIDTH_8_BYTES))

enum fsl_edma_pm_state {
	RUNNING = 0,
	SUSPENDED,
};

/*
 * This are tcd regs, equal for both v32 and v64
 */
struct fsl_edma_hw_tcd {
	__le32	saddr;
	__le16	soff;
	__le16	attr;
	__le32	nbytes;
	__le32	slast;
	__le32	daddr;
	__le16	doff;
	__le16	citer;
	__le32	dlast_sga;
	__le16	csr;
	__le16	biter;
};

/*
 * This are iomem pointers, for both v32 and v64.
 */
struct edma_regs {
	void __iomem *cr;
	void __iomem *es;
	void __iomem *erq;
	void __iomem *erqh;
	void __iomem *erql;	/* aka erq on v32 */
	void __iomem *eeih;
	void __iomem *eeil;	/* aka eei on v32 */
	void __iomem *seei;
	void __iomem *ceei;
	void __iomem *serq;
	void __iomem *cerq;
	void __iomem *cint;
	void __iomem *cerr;
	void __iomem *ssrt;
	void __iomem *cdne;
	void __iomem *inth;
	void __iomem *intl;
	void __iomem *errh;
	void __iomem *errl;
	struct fsl_edma_hw_tcd __iomem *tcd;
};

struct fsl_edma_sw_tcd {
	dma_addr_t ptcd;
	struct fsl_edma_hw_tcd *vtcd;
};

struct fsl_edma_chan {
	struct virt_dma_chan vchan;
	enum dma_status status;
	enum fsl_edma_pm_state pm_state;
	bool idle;
	u32 slave_id;
	struct fsl_edma_engine *edma;
	struct fsl_edma_desc *edesc;
	struct dma_pool *tcd_pool;
	struct dma_slave_config cfg;
	enum dma_transfer_direction dir;
};

struct fsl_edma_desc {
	struct virt_dma_desc vdesc;
	struct fsl_edma_chan *echan;
	bool iscyclic;
	unsigned int n_tcds;
	struct fsl_edma_sw_tcd tcd[];
};

enum edma_version {
	v1, /* 32ch, Vybdir, mpc57x, etc */
	v2, /* 64ch Coldfire */
};

struct fsl_edma_engine {
	struct dma_device dma_dev;
	struct edma_regs regs;
	void __iomem *membase;
	void __iomem *muxbase[DMAMUX_NR];
	struct clk *muxclk[DMAMUX_NR];
	struct mutex fsl_edma_mutex;
	u32 n_chans;
	int txirq;
	int errirq;
	bool big_endian;
	enum edma_version version;
	struct fsl_edma_chan chans[];
};

u32 edma_readl(struct fsl_edma_engine *edma, void __iomem *addr);
void edma_writeb(struct fsl_edma_engine *edma, u8 val, void __iomem *addr);
void edma_writew(struct fsl_edma_engine *edma, u16 val, void __iomem *addr);
void edma_writel(struct fsl_edma_engine *edma, u32 val, void __iomem *addr);
struct fsl_edma_chan *to_fsl_edma_chan(struct dma_chan *chan);
struct fsl_edma_desc *to_fsl_edma_desc(struct virt_dma_desc *vd);
void fsl_edma_setup_regs(struct fsl_edma_engine *edma);
void fsl_edma_disable_request(struct fsl_edma_chan *fsl_chan);
void fsl_edma_free_desc(struct virt_dma_desc *vdesc);
void fsl_edma_xfer_desc(struct fsl_edma_chan *fsl_chan);
void fsl_edma_cleanup_vchan(struct dma_device *dmadev);

/* Operations */
int fsl_edma_alloc_chan_resources(struct dma_chan *chan);
void fsl_edma_free_chan_resources(struct dma_chan *chan);
int fsl_edma_slave_config(struct dma_chan *chan,
			  struct dma_slave_config *config);
struct dma_async_tx_descriptor *fsl_edma_prep_dma_cyclic(
		struct dma_chan *chan, dma_addr_t dma_addr, size_t buf_len,
		size_t period_len, enum dma_transfer_direction direction,
		unsigned long flags);
struct dma_async_tx_descriptor *fsl_edma_prep_slave_sg(
		struct dma_chan *chan, struct scatterlist *sgl,
		unsigned int sg_len, enum dma_transfer_direction direction,
		unsigned long flags, void *context);
enum dma_status fsl_edma_tx_status(struct dma_chan *chan,
		dma_cookie_t cookie, struct dma_tx_state *txstate);
int fsl_edma_pause(struct dma_chan *chan);
int fsl_edma_resume(struct dma_chan *chan);
int fsl_edma_terminate_all(struct dma_chan *chan);
void fsl_edma_issue_pending(struct dma_chan *chan);

#endif /* _FSL_EDMA_COMMON_H_ */

