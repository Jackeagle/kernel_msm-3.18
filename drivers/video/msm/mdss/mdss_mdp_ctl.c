/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/sort.h>
#include <linux/clk.h>
#include <linux/bitmap.h>

#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_mdp_trace.h"
#include "mdss_debug.h"

#define MDSS_MDP_WB_OUTPUT_BPP	3

static inline u64 fudge_factor(u64 val, u32 numer, u32 denom)
{
	u64 result = (val * (u64)numer);
	do_div(result, denom);
	return result;
}

static inline u64 apply_fudge_factor(u64 val,
	struct mult_factor *factor)
{
	return fudge_factor(val, factor->numer, factor->denom);
}

static inline u64 apply_inverse_fudge_factor(u64 val,
	struct mult_factor *factor)
{
	return fudge_factor(val, factor->denom, factor->numer);
}

static DEFINE_MUTEX(mdss_mdp_ctl_lock);

static inline int __mdss_mdp_ctl_get_mixer_off(struct mdss_mdp_mixer *mixer);
static u32 mdss_mdp_get_vbp_factor_max(struct mdss_mdp_ctl *ctl);

static inline u32 mdss_mdp_get_pclk_rate(struct mdss_mdp_ctl *ctl)
{
	struct mdss_panel_info *pinfo = &ctl->panel_data->panel_info;

	return (ctl->intf_type == MDSS_INTF_DSI) ?
		pinfo->mipi.dsi_pclk_rate :
		pinfo->clk_rate;
}

static inline u32 mdss_mdp_clk_fudge_factor(struct mdss_mdp_mixer *mixer,
						u32 rate)
{
	struct mdss_panel_info *pinfo = &mixer->ctl->panel_data->panel_info;

	rate = apply_fudge_factor(rate, &mdss_res->clk_factor);

	/*
	 * If the panel is video mode and its back porch period is
	 * small, the workaround of increasing mdp clk is needed to
	 * avoid underrun.
	 */
	if (mixer->ctl->is_video_mode && pinfo &&
		(pinfo->lcdc.v_back_porch < MDP_MIN_VBP))
		rate = apply_fudge_factor(rate, &mdss_res->clk_factor);

	return rate;
}

struct mdss_mdp_prefill_params {
	u32 smp_bytes;
	u32 xres;
	u32 src_w;
	u32 dst_w;
	u32 src_h;
	u32 dst_h;
	u32 dst_y;
	u32 bpp;
	u32 pnum;
	bool is_yuv;
	bool is_caf;
	bool is_fbc;
	bool is_bwc;
	bool is_tile;
	bool is_hflip;
	bool is_cmd;
	bool is_ubwc;
	bool is_nv12;
};

static inline bool mdss_mdp_perf_is_caf(struct mdss_mdp_pipe *pipe)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	/*
	 * CAF mode filter is enabled when format is yuv and
	 * upscaling. Post processing had the decision to use CAF
	 * under these conditions.
	 */
	return ((mdata->mdp_rev >= MDSS_MDP_HW_REV_102) &&
		pipe->src_fmt->is_yuv && ((pipe->src.h >> pipe->vert_deci) <=
			pipe->dst.h));
}

static inline u32 mdss_mdp_calc_y_scaler_bytes(struct mdss_mdp_prefill_params
	*params, struct mdss_prefill_data *prefill)
{
	u32 y_scaler_bytes = 0, y_scaler_lines = 0;

	if (params->is_yuv) {
		if (params->src_h != params->dst_h) {
			y_scaler_lines = (params->is_caf) ?
				prefill->y_scaler_lines_caf :
				prefill->y_scaler_lines_bilinear;
			/*
			 * y is src_width, u is src_width/2 and v is
			 * src_width/2, so the total is scaler_lines *
			 * src_w * 2
			 */
			y_scaler_bytes = y_scaler_lines * params->src_w * 2;
		}
	} else {
		if (params->src_h != params->dst_h) {
			y_scaler_lines = prefill->y_scaler_lines_bilinear;
			y_scaler_bytes = y_scaler_lines * params->src_w *
				params->bpp;
		}
	}

	return y_scaler_bytes;
}

static inline u32 mdss_mdp_align_latency_buf_bytes(
		u32 latency_buf_bytes, u32 percentage,
		u32 smp_bytes)
{
	u32 aligned_bytes;

	aligned_bytes = ((smp_bytes - latency_buf_bytes) * percentage) / 100;

	pr_debug("percentage=%d, extra_bytes(per)=%d smp_bytes=%d latency=%d\n",
		percentage, aligned_bytes, smp_bytes, latency_buf_bytes);
	return latency_buf_bytes + aligned_bytes;
}

/**
 * @ mdss_mdp_calc_latency_buf_bytes() -
 *                             Get the number of bytes for the
 *                             latency lines.
 * @is_yuv - true if format is yuv
 * @is_bwc - true if BWC is enabled
 * @is_tile - true if it is Tile format
 * @src_w - source rectangle width
 * @bpp - Bytes per pixel of source rectangle
 * @use_latency_buf_percentage - use an extra percentage for
 *				the latency bytes calculation.
 * @smp_bytes - size of the smp for alignment
 * @is_ubwc - true if UBWC is enabled
 * @is_nv12 - true if NV12 format is used
 * @is_hflip - true if HFLIP is enabled
 *
 * Return:
 * The amount of bytes to consider for the latency lines, where:
 *	If use_latency_buf_percentate is  TRUE:
 *		Function will return the amount of bytes for the
 *		latency lines plus a percentage of the
 *		additional bytes allocated to align with the
 *		SMP size. Percentage is determined by
 *		"latency_buff_per", which can be modified
 *		through debugfs.
 *	If use_latency_buf_percentage is FALSE:
 *		Function will return only the the amount of bytes
 *		for the latency lines without any
 *		extra bytes.
 */
u32 mdss_mdp_calc_latency_buf_bytes(bool is_yuv, bool is_bwc,
	bool is_tile, u32 src_w, u32 bpp, bool use_latency_buf_percentage,
	u32 smp_bytes, bool is_ubwc, bool is_nv12, bool is_hflip)
{
	u32 latency_lines = 0, latency_buf_bytes;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (is_hflip && !mdata->hflip_buffer_reused)
		latency_lines = 1;

	if (is_yuv) {
		if (is_ubwc) {
			if (is_nv12)
				latency_lines += 8;
			else
				latency_lines += 4;
			latency_buf_bytes = src_w * bpp * latency_lines;
		} else if (is_bwc) {
			latency_lines += 4;
			latency_buf_bytes = src_w * bpp * latency_lines;
		} else {
			if (!mdata->hflip_buffer_reused)
				latency_lines += 1;
			else
				latency_lines = 2;
			/* multiply * 2 for the two YUV planes */
			latency_buf_bytes = mdss_mdp_align_latency_buf_bytes(
				src_w * bpp * latency_lines,
				use_latency_buf_percentage ?
				mdata->latency_buff_per : 0, smp_bytes) * 2;
		}
	} else {
		if (is_ubwc) {
			latency_lines += 4;
			latency_buf_bytes = src_w * bpp * latency_lines;
		} else if (is_tile) {
			latency_lines += 8;
			latency_buf_bytes = src_w * bpp * latency_lines;
		} else if (is_bwc) {
			latency_lines += 4;
			latency_buf_bytes = src_w * bpp * latency_lines;
		} else {
			if (!mdata->hflip_buffer_reused)
				latency_lines += 1;
			else
				latency_lines = 2;
			latency_buf_bytes = mdss_mdp_align_latency_buf_bytes(
				src_w * bpp * latency_lines,
				use_latency_buf_percentage ?
				mdata->latency_buff_per : 0, smp_bytes);
		}
	}

	return latency_buf_bytes;
}

static inline u32 mdss_mdp_calc_scaling_w_h(u32 val, u32 src_h, u32 dst_h,
	u32 src_w, u32 dst_w)
{
	if (dst_h)
		val = mult_frac(val, src_h, dst_h);
	if (dst_w)
		val = mult_frac(val, src_w, dst_w);

	return val;
}

static u32 mdss_mdp_perf_calc_pipe_prefill_video(struct mdss_mdp_prefill_params
	*params)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_prefill_data *prefill = &mdata->prefill_data;
	u32 prefill_bytes;
	u32 latency_buf_bytes;
	u32 y_buf_bytes = 0;
	u32 y_scaler_bytes = 0;
	u32 pp_bytes = 0, pp_lines = 0;
	u32 post_scaler_bytes;
	u32 fbc_bytes = 0;

	prefill_bytes = prefill->ot_bytes;

	latency_buf_bytes = mdss_mdp_calc_latency_buf_bytes(params->is_yuv,
		params->is_bwc, params->is_tile, params->src_w, params->bpp,
		true, params->smp_bytes, params->is_ubwc, params->is_nv12,
		params->is_hflip);
	prefill_bytes += latency_buf_bytes;
	pr_debug("latency_buf_bytes bw_calc=%d actual=%d\n", latency_buf_bytes,
		params->smp_bytes);

	if (params->is_yuv)
		y_buf_bytes = prefill->y_buf_bytes;

	y_scaler_bytes = mdss_mdp_calc_y_scaler_bytes(params, prefill);

	prefill_bytes += y_buf_bytes + y_scaler_bytes;

	if (mdata->apply_post_scale_bytes || (params->src_h != params->dst_h) ||
			(params->src_w != params->dst_w)) {
		post_scaler_bytes = prefill->post_scaler_pixels * params->bpp;
		post_scaler_bytes = mdss_mdp_calc_scaling_w_h(post_scaler_bytes,
			params->src_h, params->dst_h, params->src_w,
			params->dst_w);
		prefill_bytes += post_scaler_bytes;
	}

	if (params->xres)
		pp_lines = DIV_ROUND_UP(prefill->pp_pixels, params->xres);
	if (params->xres && params->dst_h && (params->dst_y <= pp_lines))
		pp_bytes = ((params->src_w * params->bpp * prefill->pp_pixels /
				params->xres) * params->src_h) / params->dst_h;
	prefill_bytes += pp_bytes;

	if (params->is_fbc) {
		fbc_bytes = prefill->fbc_lines * params->bpp;
		fbc_bytes = mdss_mdp_calc_scaling_w_h(fbc_bytes, params->src_h,
			params->dst_h, params->src_w, params->dst_w);
	}
	prefill_bytes += fbc_bytes;

	trace_mdp_perf_prefill_calc(params->pnum, latency_buf_bytes,
		prefill->ot_bytes, y_buf_bytes, y_scaler_bytes, pp_lines,
		pp_bytes, post_scaler_bytes, fbc_bytes, prefill_bytes);

	pr_debug("ot=%d y_buf=%d pp_lines=%d pp=%d post_sc=%d fbc_bytes=%d\n",
		prefill->ot_bytes, y_buf_bytes, pp_lines, pp_bytes,
		post_scaler_bytes, fbc_bytes);

	return prefill_bytes;
}

static u32 mdss_mdp_perf_calc_pipe_prefill_cmd(struct mdss_mdp_prefill_params
	*params)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_prefill_data *prefill = &mdata->prefill_data;
	u32 prefill_bytes;
	u32 ot_bytes = 0;
	u32 latency_lines, latency_buf_bytes;
	u32 y_buf_bytes = 0;
	u32 y_scaler_bytes;
	u32 fbc_cmd_lines = 0, fbc_cmd_bytes = 0;
	u32 post_scaler_bytes = 0;

	/* y_scaler_bytes are same for the first or non first line */
	y_scaler_bytes = mdss_mdp_calc_y_scaler_bytes(params, prefill);
	prefill_bytes = y_scaler_bytes;

	/* 1st line if fbc is not enabled and 2nd line if fbc is enabled */
	if (((params->dst_y == 0) && !params->is_fbc) ||
		((params->dst_y <= 1) && params->is_fbc)) {
		if (params->is_ubwc) {
			if (params->is_nv12)
				latency_lines = 8;
			else
				latency_lines = 4;
		} else if (params->is_bwc || params->is_tile) {
			latency_lines = 4;
		} else if (params->is_hflip) {
			latency_lines = 1;
		} else {
			latency_lines = 0;
		}
		latency_buf_bytes = params->src_w * params->bpp * latency_lines;
		prefill_bytes += latency_buf_bytes;

		fbc_cmd_lines++;
		if (params->is_fbc)
			fbc_cmd_lines++;
		fbc_cmd_bytes = params->bpp * params->dst_w * fbc_cmd_lines;
		fbc_cmd_bytes = mdss_mdp_calc_scaling_w_h(fbc_cmd_bytes,
			params->src_h, params->dst_h, params->src_w,
			params->dst_w);
		prefill_bytes += fbc_cmd_bytes;
	} else {
		ot_bytes = prefill->ot_bytes;
		prefill_bytes += ot_bytes;

		latency_buf_bytes = mdss_mdp_calc_latency_buf_bytes(
			params->is_yuv, params->is_bwc, params->is_tile,
			params->src_w, params->bpp, true, params->smp_bytes,
			params->is_ubwc, params->is_nv12, params->is_hflip);
		prefill_bytes += latency_buf_bytes;

		if (params->is_yuv)
			y_buf_bytes = prefill->y_buf_bytes;
		prefill_bytes += y_buf_bytes;

		if (mdata->apply_post_scale_bytes ||
				(params->src_h != params->dst_h) ||
				(params->src_w != params->dst_w)) {
			post_scaler_bytes = prefill->post_scaler_pixels *
				params->bpp;
			post_scaler_bytes = mdss_mdp_calc_scaling_w_h(
				post_scaler_bytes, params->src_h,
				params->dst_h, params->src_w,
				params->dst_w);
			prefill_bytes += post_scaler_bytes;
		}
	}

	pr_debug("ot=%d bwc=%d smp=%d y_buf=%d fbc=%d\n", ot_bytes,
		params->is_bwc, latency_buf_bytes, y_buf_bytes, fbc_cmd_bytes);

	return prefill_bytes;
}

u32 mdss_mdp_perf_calc_pipe_prefill_single(struct mdss_mdp_prefill_params
	*params)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_prefill_data *prefill = &mdata->prefill_data;
	u32 prefill_bytes;
	u32 latency_lines, latency_buf_bytes;
	u32 y_scaler_bytes;
	u32 fbc_cmd_lines = 0, fbc_cmd_bytes = 0;

	if (params->is_ubwc) {
		if (params->is_nv12)
			latency_lines = 8;
		else
			latency_lines = 4;
	} else if (params->is_bwc || params->is_tile)
		/* can start processing after receiving 4 lines */
		latency_lines = 4;
	else if (params->is_hflip)
		/* need oneline before reading backwards */
		latency_lines = 1;
	else
		latency_lines = 0;
	latency_buf_bytes = params->src_w * params->bpp * latency_lines;
	prefill_bytes = latency_buf_bytes;

	y_scaler_bytes = mdss_mdp_calc_y_scaler_bytes(params, prefill);
	prefill_bytes += y_scaler_bytes;

	if (params->is_cmd)
		fbc_cmd_lines++;
	if (params->is_fbc)
		fbc_cmd_lines++;

	if (fbc_cmd_lines) {
		fbc_cmd_bytes = params->bpp * params->dst_w * fbc_cmd_lines;
		fbc_cmd_bytes = mdss_mdp_calc_scaling_w_h(fbc_cmd_bytes,
			params->src_h, params->dst_h, params->src_w,
			params->dst_w);
		prefill_bytes += fbc_cmd_bytes;
	}

	return prefill_bytes;
}

u32 mdss_mdp_perf_calc_smp_size(struct mdss_mdp_pipe *pipe,
	bool calc_smp_size)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 smp_bytes;

	if (pipe->type == PIPE_TYPE_CURSOR)
		return 0;

	/* Get allocated or fixed smp bytes */
	smp_bytes = mdss_mdp_smp_get_size(pipe);

	/*
	 * We need to calculate the SMP size for scenarios where
	 * allocation have not happened yet (i.e. during prepare IOCTL).
	 */
	if (calc_smp_size && !mdata->has_pixel_ram) {
		u32 calc_smp_total;
		calc_smp_total = mdss_mdp_smp_calc_num_blocks(pipe);
		calc_smp_total *= mdata->smp_mb_size;

		/*
		 * If the pipe has fixed SMPs, then we must consider
		 * the max smp size.
		 */
		if (calc_smp_total > smp_bytes)
			smp_bytes = calc_smp_total;
	}

	pr_debug("SMP size (bytes) %d for pnum=%d calc=%d\n",
		smp_bytes, pipe->num, calc_smp_size);
	BUG_ON(smp_bytes == 0);

	return smp_bytes;
}

static void mdss_mdp_get_bw_vote_mode(struct mdss_mdp_mixer *mixer,
	u32 mdp_rev, struct mdss_mdp_perf_params *perf, u32 flags)
{
	bitmap_zero(perf->bw_vote_mode, MDSS_MDP_BW_MODE_MAX);

	if (!mixer)
		goto exit;

	switch (mdp_rev) {
	case MDSS_MDP_HW_REV_105:
	case MDSS_MDP_HW_REV_109:
		if ((flags & PERF_CALC_PIPE_SINGLE_LAYER) &&
			!mixer->rotator_mode &&
			(mixer->type == MDSS_MDP_MIXER_TYPE_INTF)) {
				set_bit(MDSS_MDP_BW_MODE_SINGLE_LAYER,
					perf->bw_vote_mode);
		}
		break;
	default:
		break;
	};

	pr_debug("mode=0x%lx\n", *(perf->bw_vote_mode));

exit:
	return;
}

static u32 get_pipe_mdp_clk_rate(struct mdss_mdp_pipe *pipe,
	struct mdss_rect src, struct mdss_rect dst,
	u32 fps, u32 v_total, u32 flags)
{
	struct mdss_mdp_mixer *mixer;
	u32 rate, src_h;

	mixer = pipe->mixer_left;
	src_h = src.h >> pipe->vert_deci;

	if (mixer->rotator_mode) {

		rate = pipe->src.w * pipe->src.h * fps;
		rate /= 4; /* block mode fetch at 4 pix/clk */
	} else {

		rate = dst.w;
		if (src_h > dst.h)
			rate = (rate * src_h) / dst.h;

		rate *= v_total * fps;

		/* pipes decoding BWC content have different clk requirement */
		if (pipe->bwc_mode && !pipe->src_fmt->is_yuv &&
		    pipe->src_fmt->bpp == 4) {
			u32 bwc_rate =
			mult_frac((src.w * src_h * fps), v_total, dst.h << 1);
			pr_debug("src: w:%d h:%d fps:%d vtotal:%d dst h:%d\n",
				src.w, src_h, fps, v_total, dst.h);
			pr_debug("pipe%d: bwc_rate=%d normal_rate=%d\n",
				pipe->num, bwc_rate, rate);
			rate = max(bwc_rate, rate);
		}
	}

	if (flags & PERF_CALC_PIPE_APPLY_CLK_FUDGE)
		rate = mdss_mdp_clk_fudge_factor(mixer, rate);

	return rate;
}

static inline bool validate_comp_ratio(struct mult_factor *factor)
{
	return factor->numer && factor->denom;
}

u32 apply_comp_ratio_factor(u32 quota,
	struct mdss_mdp_format_params *fmt,
	struct mult_factor *factor)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!mdata || !test_bit(MDSS_QOS_OVERHEAD_FACTOR,
		      mdata->mdss_qos_map))
		return quota;

	/* apply compression ratio, only for compressed formats */
	if (mdss_mdp_is_ubwc_format(fmt) &&
	    validate_comp_ratio(factor))
		quota = apply_inverse_fudge_factor(quota , factor);

	return quota;
}

static u32 mdss_mdp_get_rotator_fps(struct mdss_mdp_pipe *pipe)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 fps = DEFAULT_FRAME_RATE;

	if (pipe->frame_rate)
		fps = pipe->frame_rate;

	if (mdata->traffic_shaper_en)
		fps = DEFAULT_ROTATOR_FRAME_RATE;

	if (pipe->src.w >= 3840 || pipe->src.h >= 3840)
		fps = ROTATOR_LOW_FRAME_RATE;

	pr_debug("rotator fps:%d\n", fps);

	return fps;
}

u64 mdss_mdp_perf_calc_simplified_prefill(struct mdss_mdp_pipe *pipe,
	u32 v_total, u32 fps, struct mdss_mdp_ctl *ctl)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct simplified_prefill_factors *pfactors =
			&mdata->prefill_data.prefill_factors;
	u64 prefill_per_pipe = 0;
	u32 prefill_lines = pfactors->xtra_ff_factor;


	/* do not calculate prefill for command mode */
	if (!ctl->is_video_mode)
		goto exit;

	prefill_per_pipe = pipe->src.w * pipe->src_fmt->bpp;

	/* format factors */
	if (mdss_mdp_is_tile_format(pipe->src_fmt)) {
		if (mdss_mdp_is_nv12_format(pipe->src_fmt))
			prefill_lines += pfactors->fmt_mt_nv12_factor;
		else
			prefill_lines += pfactors->fmt_mt_factor;
	} else {
		prefill_lines += pfactors->fmt_linear_factor;
	}

	/* scaling factors */
	if (pipe->src.h > pipe->dst.h) {
		prefill_lines += pfactors->scale_factor;

		prefill_per_pipe = fudge_factor(prefill_per_pipe,
			DECIMATED_DIMENSION(pipe->src.h, pipe->vert_deci),
			pipe->dst.h);
	}

	prefill_per_pipe *= prefill_lines * mdss_mdp_get_vbp_factor_max(ctl);

	pr_debug("pipe src: %dx%d bpp:%d\n",
		pipe->src.w, pipe->src.h, pipe->src_fmt->bpp);
	pr_debug("ff_factor:%d mt_nv12:%d mt:%d\n",
		pfactors->xtra_ff_factor,
		(mdss_mdp_is_tile_format(pipe->src_fmt) &&
		mdss_mdp_is_nv12_format(pipe->src_fmt)) ?
		pfactors->fmt_mt_nv12_factor : 0,
		mdss_mdp_is_tile_format(pipe->src_fmt) ?
		pfactors->fmt_mt_factor : 0);
	pr_debug("pipe prefill:%llu lines:%d\n",
		prefill_per_pipe, prefill_lines);

exit:
	return prefill_per_pipe;
}

/**
 * mdss_mdp_perf_calc_pipe() - calculate performance numbers required by pipe
 * @pipe:	Source pipe struct containing updated pipe params
 * @perf:	Structure containing values that should be updated for
 *		performance tuning
 * @flags: flags to determine how to perform some of the
 *		calculations, supported flags:
 *
 *	PERF_CALC_PIPE_APPLY_CLK_FUDGE:
 *		Determine if mdp clock fudge is applicable.
 *	PERF_CALC_PIPE_SINGLE_LAYER:
 *		Indicate if the calculation is for a single pipe staged
 *		in the layer mixer
 *	PERF_CALC_PIPE_CALC_SMP_SIZE:
 *		Indicate if the smp size needs to be calculated, this is
 *		for the cases where SMP haven't been allocated yet, so we need
 *		to estimate here the smp size (i.e. PREPARE IOCTL).
 *
 * Function calculates the minimum required performance calculations in order
 * to avoid MDP underflow. The calculations are based on the way MDP
 * fetches (bandwidth requirement) and processes data through MDP pipeline
 * (MDP clock requirement) based on frame size and scaling requirements.
 */
int mdss_mdp_perf_calc_pipe(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_perf_params *perf, struct mdss_rect *roi,
	u32 flags)
{
	struct mdss_mdp_mixer *mixer;
	int fps = DEFAULT_FRAME_RATE;
	u32 quota, v_total = 0, src_h, xres = 0, h_total = 0;
	struct mdss_rect src, dst;
	bool is_fbc = false;
	struct mdss_mdp_prefill_params prefill_params;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool calc_smp_size = false;

	if (!pipe || !perf || !pipe->mixer_left)
		return -EINVAL;

	mixer = pipe->mixer_left;

	dst = pipe->dst;
	src = pipe->src;

	if (mixer->rotator_mode) {
		fps = mdss_mdp_get_rotator_fps(pipe);
	} else if (mixer->type == MDSS_MDP_MIXER_TYPE_INTF) {
		struct mdss_panel_info *pinfo;

		if (!mixer->ctl)
			return -EINVAL;

		pinfo = &mixer->ctl->panel_data->panel_info;
		if (pinfo->type == MIPI_VIDEO_PANEL) {
			fps = pinfo->panel_max_fps;
			v_total = pinfo->panel_max_vtotal;
		} else {
			fps = mdss_panel_get_framerate(pinfo);
			v_total = mdss_panel_get_vtotal(pinfo);
		}
		xres = get_panel_width(mixer->ctl);
		is_fbc = pinfo->fbc.enabled;
		h_total = mdss_panel_get_htotal(pinfo, false);

		if (is_pingpong_split(mixer->ctl->mfd))
			h_total += mdss_panel_get_htotal(
				&mixer->ctl->panel_data->next->panel_info,
				false);
	} else {
		v_total = mixer->height;
		xres = mixer->width;
		h_total = mixer->width;
	}

	if (roi && !mixer->ctl->is_video_mode && !pipe->src_split_req)
		mdss_mdp_crop_rect(&src, &dst, roi);

	pr_debug("v_total=%d, xres=%d fps=%d\n", v_total, xres, fps);

	/*
	 * when doing vertical decimation lines will be skipped, hence there is
	 * no need to account for these lines in MDP clock or request bus
	 * bandwidth to fetch them.
	 */
	src_h = DECIMATED_DIMENSION(src.h, pipe->vert_deci);

	quota = fps * src.w * src_h;

	pr_debug("src(w,h)(%d,%d) dst(w,h)(%d,%d) dst_y=%d bpp=%d yuv=%d\n",
		 pipe->src.w, src_h, pipe->dst.w, pipe->dst.h, pipe->dst.y,
		 pipe->src_fmt->bpp, pipe->src_fmt->is_yuv);

	if (pipe->src_fmt->chroma_sample == MDSS_MDP_CHROMA_420)
		/*
		 * with decimation, chroma is not downsampled, this means we
		 * need to allocate bw for extra lines that will be fetched
		 */
		if (pipe->vert_deci)
			quota *= 2;
		else
			quota = (quota * 3) / 2;
	else
		quota *= pipe->src_fmt->bpp;

	if (mixer->rotator_mode) {
		if (test_bit(MDSS_QOS_OVERHEAD_FACTOR,
				mdata->mdss_qos_map)) {
			/* rotator read */
			quota = apply_comp_ratio_factor(quota,
				pipe->src_fmt, &pipe->comp_ratio);
			/*
			 * rotator write: here we are using src_fmt since
			 * current implementation only supports calculate
			 * bandwidth based in the source parameters.
			 * The correct fine-tuned calculation should use
			 * destination format and destination rectangles to
			 * calculate the bandwidth, but leaving this
			 * calculation as per current support.
			 */
			quota += apply_comp_ratio_factor(quota,
				pipe->src_fmt, &pipe->comp_ratio);
		} else {
			quota *= 2; /* bus read + write */
		}
	} else {

		quota = mult_frac(quota, v_total, dst.h);
		if (!mixer->ctl->is_video_mode)
			quota = mult_frac(quota, h_total, xres);

		if (test_bit(MDSS_QOS_OVERHEAD_FACTOR,
				mdata->mdss_qos_map))
			quota = apply_comp_ratio_factor(quota,
				pipe->src_fmt, &pipe->comp_ratio);
	}

	perf->bw_overlap = quota;

	perf->mdp_clk_rate = get_pipe_mdp_clk_rate(pipe, src, dst,
		fps, v_total, flags);

	if (mixer->ctl->intf_num == MDSS_MDP_NO_INTF ||
		mdata->disable_prefill ||
		mixer->ctl->disable_prefill ||
		(pipe->flags & MDP_SOLID_FILL)) {
		perf->prefill_bytes = 0;
		perf->bw_prefill = 0;
		goto exit;
	}

	if (test_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map)) {
		perf->bw_prefill = mdss_mdp_perf_calc_simplified_prefill(pipe,
			v_total, fps, mixer->ctl);
		goto exit;
	}

	calc_smp_size = (flags & PERF_CALC_PIPE_CALC_SMP_SIZE) ? true : false;
	prefill_params.smp_bytes = mdss_mdp_perf_calc_smp_size(pipe,
			calc_smp_size);
	prefill_params.xres = xres;
	prefill_params.src_w = src.w;
	prefill_params.src_h = src_h;
	prefill_params.dst_w = dst.w;
	prefill_params.dst_h = dst.h;
	prefill_params.dst_y = dst.y;
	prefill_params.bpp = pipe->src_fmt->bpp;
	prefill_params.is_yuv = pipe->src_fmt->is_yuv;
	prefill_params.is_caf = mdss_mdp_perf_is_caf(pipe);
	prefill_params.is_fbc = is_fbc;
	prefill_params.is_bwc = pipe->bwc_mode;
	prefill_params.is_tile = mdss_mdp_is_tile_format(pipe->src_fmt);
	prefill_params.is_hflip = pipe->flags & MDP_FLIP_LR;
	prefill_params.is_cmd = !mixer->ctl->is_video_mode;
	prefill_params.pnum = pipe->num;
	prefill_params.is_bwc = mdss_mdp_is_ubwc_format(pipe->src_fmt);
	prefill_params.is_nv12 = mdss_mdp_is_nv12_format(pipe->src_fmt);

	mdss_mdp_get_bw_vote_mode(mixer, mdata->mdp_rev, perf, flags);

	if (flags & PERF_CALC_PIPE_SINGLE_LAYER)
		perf->prefill_bytes =
			mdss_mdp_perf_calc_pipe_prefill_single(&prefill_params);
	else if (!prefill_params.is_cmd)
		perf->prefill_bytes =
			mdss_mdp_perf_calc_pipe_prefill_video(&prefill_params);
	else
		perf->prefill_bytes =
			mdss_mdp_perf_calc_pipe_prefill_cmd(&prefill_params);

exit:
	pr_debug("mixer=%d pnum=%d clk_rate=%u bw_overlap=%llu bw_prefill=%llu (%d) %s\n",
		 mixer->num, pipe->num, perf->mdp_clk_rate, perf->bw_overlap,
		 perf->bw_prefill, perf->prefill_bytes, mdata->disable_prefill ?
		 "prefill is disabled" : "");

	return 0;
}

static inline int mdss_mdp_perf_is_overlap(u32 y00, u32 y01, u32 y10, u32 y11)
{
	return (y10 < y00 && y11 >= y01) || (y10 >= y00 && y10 < y01);
}

static inline int cmpu32(const void *a, const void *b)
{
	return (*(u32 *)a < *(u32 *)b) ? -1 : 0;
}

static void mdss_mdp_perf_calc_mixer(struct mdss_mdp_mixer *mixer,
		struct mdss_mdp_perf_params *perf,
		struct mdss_mdp_pipe **pipe_list, int num_pipes,
		u32 flags)
{
	struct mdss_mdp_pipe *pipe;
	struct mdss_panel_info *pinfo = NULL;
	int fps = DEFAULT_FRAME_RATE;
	u32 v_total = 0, bpp = MDSS_MDP_WB_OUTPUT_BPP;
	int i;
	u32 max_clk_rate = 0;
	u64 bw_overlap_max = 0;
	u64 bw_overlap[MAX_PIPES_PER_LM] = { 0 };
	u64 bw_overlap_async = 0;
	u32 v_region[MAX_PIPES_PER_LM * 2] = { 0 };
	u32 prefill_val = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool apply_fudge = true;
	struct mdss_mdp_format_params *fmt;

	BUG_ON(num_pipes > MAX_PIPES_PER_LM);

	memset(perf, 0, sizeof(*perf));

	if (!mixer->rotator_mode) {
		if (mixer->type == MDSS_MDP_MIXER_TYPE_INTF) {
			pinfo = &mixer->ctl->panel_data->panel_info;
			if (pinfo->type == MIPI_VIDEO_PANEL) {
				fps = pinfo->panel_max_fps;
				v_total = pinfo->panel_max_vtotal;
			} else {
				fps = mdss_panel_get_framerate(pinfo);
				v_total = mdss_panel_get_vtotal(pinfo);
			}

			if (pinfo->type == WRITEBACK_PANEL) {
				fmt = mdss_mdp_get_format_params(
					mixer->ctl->dst_format);
				if (fmt)
					bpp = fmt->bpp;
				pinfo = NULL;
			}
		} else {
			v_total = mixer->height;
		}

		perf->mdp_clk_rate = mixer->width * v_total * fps;
		perf->mdp_clk_rate =
			mdss_mdp_clk_fudge_factor(mixer, perf->mdp_clk_rate);

		if (!pinfo) { /* perf for bus writeback */
			perf->bw_writeback =
				fps * mixer->width * mixer->height * bpp;

			if (test_bit(MDSS_QOS_OVERHEAD_FACTOR,
					mdata->mdss_qos_map))
				perf->bw_writeback = apply_comp_ratio_factor(
						perf->bw_writeback, fmt,
						&mixer->ctl->dst_comp_ratio);

		} else if (pinfo->type == MIPI_CMD_PANEL) {
			u32 dsi_transfer_rate = mixer->width * v_total;

			/* adjust transfer time from micro seconds */
			dsi_transfer_rate = mult_frac(dsi_transfer_rate,
				1000000, pinfo->mdp_transfer_time_us);

			if (dsi_transfer_rate > perf->mdp_clk_rate)
				perf->mdp_clk_rate = dsi_transfer_rate;
		}

		if (is_dsc_compression(pinfo) &&
		    mixer->ctl->opmode & MDSS_MDP_CTL_OP_PACK_3D_ENABLE)
			perf->mdp_clk_rate *= 2;
	}

	/*
	 * In case of border color, we still need enough mdp clock
	 * to avoid under-run. Clock requirement for border color is
	 * based on mixer width.
	 */
	if (num_pipes == 0)
		goto exit;

	memset(bw_overlap, 0, sizeof(u64) * MAX_PIPES_PER_LM);
	memset(v_region, 0, sizeof(u32) * MAX_PIPES_PER_LM * 2);

	/*
	* Apply this logic only for 8x26 to reduce clock rate
	* for single video playback use case
	*/
	if (IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev, MDSS_MDP_HW_REV_101)
		 && mixer->type == MDSS_MDP_MIXER_TYPE_INTF) {
		u32 npipes = 0;
		for (i = 0; i < num_pipes; i++) {
			pipe = pipe_list[i];
			if (pipe) {
				if (npipes) {
					apply_fudge = true;
					break;
				}
				npipes++;
				apply_fudge = !(pipe->src_fmt->is_yuv)
					|| !(pipe->flags
					& MDP_SOURCE_ROTATED_90);
			}
		}
	}

	if (apply_fudge)
		flags |= PERF_CALC_PIPE_APPLY_CLK_FUDGE;
	if (num_pipes == 1)
		flags |= PERF_CALC_PIPE_SINGLE_LAYER;

	for (i = 0; i < num_pipes; i++) {
		struct mdss_mdp_perf_params tmp;

		memset(&tmp, 0, sizeof(tmp));

		pipe = pipe_list[i];
		if (pipe == NULL)
			continue;

		/*
		 * if is pipe used across two LMs in source split configuration
		 * then it is staged on both LMs. In such cases skip BW calc
		 * for such pipe on right LM to prevent adding BW twice.
		 */
		if (pipe->src_split_req && mixer->is_right_mixer)
			continue;

		if (mdss_mdp_perf_calc_pipe(pipe, &tmp, &mixer->roi,
			flags))
			continue;

		if (!mdss_mdp_is_nrt_ctl_path(mixer->ctl))
			perf->max_per_pipe_ib = max(perf->max_per_pipe_ib,
			    tmp.bw_overlap);

		bitmap_or(perf->bw_vote_mode, perf->bw_vote_mode,
			tmp.bw_vote_mode, MDSS_MDP_BW_MODE_MAX);

		/*
		 * for async layers, the overlap calculation is skipped
		 * and the bandwidth is added at the end, accounting for
		 * worst case, that async layer might overlap with
		 * all the other layers.
		 */
		if (pipe->async_update) {
			bw_overlap[i] = 0;
			v_region[2*i] = 0;
			v_region[2*i + 1] = 0;
			bw_overlap_async += tmp.bw_overlap;
		} else {
			bw_overlap[i] = tmp.bw_overlap;
			v_region[2*i] = pipe->dst.y;
			v_region[2*i + 1] = pipe->dst.y + pipe->dst.h;
		}

		if (tmp.mdp_clk_rate > max_clk_rate)
			max_clk_rate = tmp.mdp_clk_rate;

		if (test_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map))
			prefill_val += tmp.bw_prefill;
		else
			prefill_val += tmp.prefill_bytes;
	}

	/*
	 * Sort the v_region array so the total display area can be
	 * divided into individual regions. Check how many pipes fetch
	 * data for each region and sum them up, then the worst case
	 * of all regions is ib request.
	 */
	sort(v_region, num_pipes * 2, sizeof(u32), cmpu32, NULL);
	for (i = 1; i < num_pipes * 2; i++) {
		int j;
		u64 bw_max_region = 0;
		u32 y0, y1;
		pr_debug("v_region[%d]%d\n", i, v_region[i]);
		if (v_region[i] == v_region[i-1])
			continue;
		y0 = v_region[i-1];
		y1 = v_region[i];
		for (j = 0; j < num_pipes; j++) {
			if (!bw_overlap[j])
				continue;
			pipe = pipe_list[j];
			if (mdss_mdp_perf_is_overlap(y0, y1, pipe->dst.y,
				(pipe->dst.y + pipe->dst.h)))
				bw_max_region += bw_overlap[j];
			pr_debug("v[%d](%d,%d)pipe[%d](%d,%d)bw(%llu %llu)\n",
				i, y0, y1, j, pipe->dst.y,
				pipe->dst.y + pipe->dst.h, bw_overlap[j],
				bw_max_region);
		}
		bw_overlap_max = max(bw_overlap_max, bw_max_region);
	}

	perf->bw_overlap += bw_overlap_max + bw_overlap_async;

	if (test_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map))
		perf->bw_prefill += prefill_val;
	else
		perf->prefill_bytes += prefill_val;

	if (max_clk_rate > perf->mdp_clk_rate)
		perf->mdp_clk_rate = max_clk_rate;

exit:
	pr_debug("final mixer=%d video=%d clk_rate=%u bw=%llu prefill=%d mode=0x%lx\n",
		mixer->num, mixer->ctl->is_video_mode, perf->mdp_clk_rate,
		perf->bw_overlap, prefill_val,
		*(perf->bw_vote_mode));
}

static bool is_mdp_prefetch_needed(struct mdss_panel_info *pinfo)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool enable_prefetch = false;

	if (mdata->mdp_rev >= MDSS_MDP_HW_REV_105) {
		if ((pinfo->lcdc.v_back_porch + pinfo->lcdc.v_pulse_width +
			pinfo->lcdc.v_front_porch) < mdata->min_prefill_lines)
			pr_warn_once("low vbp+vfp may lead to perf issues in some cases\n");

		enable_prefetch = true;

		if ((pinfo->lcdc.v_back_porch + pinfo->lcdc.v_pulse_width) >=
				MDSS_MDP_MAX_PREFILL_FETCH)
			enable_prefetch = false;
	} else {
		if ((pinfo->lcdc.v_back_porch + pinfo->lcdc.v_pulse_width) <
				mdata->min_prefill_lines)
			pr_warn_once("low vbp may lead to display performance issues");
	}

	return enable_prefetch;
}

/**
 * mdss_mdp_get_prefetch_lines: - Number of fetch lines in vertical front porch
 * @pinfo: Pointer to the panel information.
 *
 * Returns the number of fetch lines in vertical front porch at which mdp
 * can start fetching the next frame.
 *
 * In some cases, vertical front porch is too high. In such cases limit
 * the mdp fetch lines  as the last (25 - vbp - vpw) lines of vertical
 * front porch.
 */
int mdss_mdp_get_prefetch_lines(struct mdss_panel_info *pinfo)
{
	int prefetch_avail = 0;
	int v_total, vfp_start;
	u32 prefetch_needed;

	if (!is_mdp_prefetch_needed(pinfo))
		return 0;

	v_total = mdss_panel_get_vtotal(pinfo);
	vfp_start = (pinfo->lcdc.v_back_porch + pinfo->lcdc.v_pulse_width +
			pinfo->yres);

	prefetch_avail = v_total - vfp_start;
	prefetch_needed = MDSS_MDP_MAX_PREFILL_FETCH -
		pinfo->lcdc.v_back_porch -
		pinfo->lcdc.v_pulse_width;

	if (prefetch_avail > prefetch_needed)
		prefetch_avail = prefetch_needed;

	return prefetch_avail;
}

static u32 mdss_mdp_get_vbp_factor(struct mdss_mdp_ctl *ctl)
{
	u32 fps, v_total, vbp, vbp_fac;
	struct mdss_panel_info *pinfo;

	if (!ctl || !ctl->panel_data)
		return 0;

	pinfo = &ctl->panel_data->panel_info;
	fps = mdss_panel_get_framerate(pinfo);
	v_total = mdss_panel_get_vtotal(pinfo);
	vbp = pinfo->lcdc.v_back_porch + pinfo->lcdc.v_pulse_width;
	vbp += pinfo->prg_fet;

	vbp_fac = (vbp) ? fps * v_total / vbp : 0;
	pr_debug("vbp_fac=%d vbp=%d v_total=%d\n", vbp_fac, vbp, v_total);

	return vbp_fac;
}

static u32 mdss_mdp_get_vbp_factor_max(struct mdss_mdp_ctl *ctl)
{
	u32 vbp_max = 0;
	int i;
	struct mdss_data_type *mdata;

	if (!ctl || !ctl->mdata)
		return 0;

	mdata = ctl->mdata;
	for (i = 0; i < mdata->nctl; i++) {
		struct mdss_mdp_ctl *ctl = mdata->ctl_off + i;
		u32 vbp_fac;

		/* skip command mode interfaces */
		if (test_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map)
				&& !ctl->is_video_mode)
			continue;

		if (mdss_mdp_ctl_is_power_on(ctl)) {
			vbp_fac = mdss_mdp_get_vbp_factor(ctl);
			vbp_max = max(vbp_max, vbp_fac);
		}
	}

	return vbp_max;
}

static bool mdss_mdp_video_mode_intf_connected(struct mdss_mdp_ctl *ctl)
{
	int i;
	struct mdss_data_type *mdata;

	if (!ctl || !ctl->mdata)
		return 0;

	mdata = ctl->mdata;
	for (i = 0; i < mdata->nctl; i++) {
		struct mdss_mdp_ctl *ctl = mdata->ctl_off + i;

		if (ctl->is_video_mode && mdss_mdp_ctl_is_power_on(ctl)) {
			pr_debug("video interface connected ctl:%d\n",
				ctl->num);
			return true;
		}
	}

	return false;
}

static void __mdss_mdp_perf_calc_ctl_helper(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_perf_params *perf,
		struct mdss_mdp_pipe **left_plist, int left_cnt,
		struct mdss_mdp_pipe **right_plist, int right_cnt,
		u32 flags)
{
	struct mdss_mdp_perf_params tmp;
	struct mdss_data_type *mdata = ctl->mdata;

	memset(perf, 0, sizeof(*perf));

	if (ctl->mixer_left) {
		mdss_mdp_perf_calc_mixer(ctl->mixer_left, &tmp,
				left_plist, left_cnt, flags);

		bitmap_or(perf->bw_vote_mode, perf->bw_vote_mode,
			tmp.bw_vote_mode, MDSS_MDP_BW_MODE_MAX);

		perf->max_per_pipe_ib = tmp.max_per_pipe_ib;
		perf->bw_overlap += tmp.bw_overlap;
		perf->mdp_clk_rate = tmp.mdp_clk_rate;
		perf->bw_writeback += tmp.bw_writeback;

		if (test_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map))
			perf->bw_prefill += tmp.bw_prefill;
		else
			perf->prefill_bytes += tmp.prefill_bytes;
	}

	if (ctl->mixer_right) {
		mdss_mdp_perf_calc_mixer(ctl->mixer_right, &tmp,
				right_plist, right_cnt, flags);

		bitmap_or(perf->bw_vote_mode, perf->bw_vote_mode,
			tmp.bw_vote_mode, MDSS_MDP_BW_MODE_MAX);

		perf->max_per_pipe_ib = max(perf->max_per_pipe_ib,
			tmp.max_per_pipe_ib);
		perf->bw_overlap += tmp.bw_overlap;
		perf->bw_writeback += tmp.bw_writeback;
		if (tmp.mdp_clk_rate > perf->mdp_clk_rate)
			perf->mdp_clk_rate = tmp.mdp_clk_rate;

		if (test_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map))
			perf->bw_prefill += tmp.bw_prefill;
		else
			perf->prefill_bytes += tmp.prefill_bytes;

		if (ctl->intf_type) {
			u32 clk_rate = mdss_mdp_get_pclk_rate(ctl);
			/* minimum clock rate due to inefficiency in 3dmux */
			clk_rate = mult_frac(clk_rate >> 1, 9, 8);
			if (clk_rate > perf->mdp_clk_rate)
				perf->mdp_clk_rate = clk_rate;
		}
	}

	/* request minimum bandwidth to have bus clock on when display is on */
	if (perf->bw_overlap == 0)
		perf->bw_overlap = SZ_16M;

	if (!test_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map) &&
		(ctl->intf_type != MDSS_MDP_NO_INTF)) {
		u32 vbp_fac = mdss_mdp_get_vbp_factor_max(ctl);

		perf->bw_prefill = perf->prefill_bytes;
		/*
		 * Prefill bandwidth equals the amount of data (number
		 * of prefill_bytes) divided by the the amount time
		 * available (blanking period). It is equivalent that
		 * prefill bytes times a factor in unit Hz, which is
		 * the reciprocal of time.
		 */
		perf->bw_prefill *= vbp_fac;
	}

	perf->bw_ctl = max(perf->bw_prefill, perf->bw_overlap);
	pr_debug("ctl=%d prefill bw=%llu overlap bw=%llu mode=0x%lx writeback:%llu\n",
			ctl->num, perf->bw_prefill, perf->bw_overlap,
			*(perf->bw_vote_mode), perf->bw_writeback);
}

static u32 mdss_check_for_flip(struct mdss_mdp_ctl *ctl)
{
	u32 i, panel_orientation;
	struct mdss_mdp_pipe *pipe;
	u32 flags = 0;

	panel_orientation = ctl->mfd->panel_orientation;
	if (panel_orientation & MDP_FLIP_LR)
		flags |= MDSS_MAX_BW_LIMIT_HFLIP;
	if (panel_orientation & MDP_FLIP_UD)
		flags |= MDSS_MAX_BW_LIMIT_VFLIP;

	for (i = 0; i < MAX_PIPES_PER_LM; i++) {
		if ((flags & MDSS_MAX_BW_LIMIT_HFLIP) &&
				(flags & MDSS_MAX_BW_LIMIT_VFLIP))
			return flags;

		if (ctl->mixer_left && ctl->mixer_left->stage_pipe[i]) {
			pipe = ctl->mixer_left->stage_pipe[i];
			if (pipe->flags & MDP_FLIP_LR)
				flags |= MDSS_MAX_BW_LIMIT_HFLIP;
			if (pipe->flags & MDP_FLIP_UD)
				flags |= MDSS_MAX_BW_LIMIT_VFLIP;
		}

		if (ctl->mixer_right && ctl->mixer_right->stage_pipe[i]) {
			pipe = ctl->mixer_right->stage_pipe[i];
			if (pipe->flags & MDP_FLIP_LR)
				flags |= MDSS_MAX_BW_LIMIT_HFLIP;
			if (pipe->flags & MDP_FLIP_UD)
				flags |= MDSS_MAX_BW_LIMIT_VFLIP;
		}
	}

	return flags;
}

static int mdss_mdp_set_threshold_max_bandwidth(struct mdss_mdp_ctl *ctl)
{
	u32 mode, threshold = 0, max = INT_MAX;
	u32 i = 0;
	struct mdss_max_bw_settings *max_bw_settings =
		ctl->mdata->max_bw_settings;

	if (!ctl->mdata->max_bw_settings_cnt && !ctl->mdata->max_bw_settings)
		return 0;

	mode = ctl->mdata->bw_mode_bitmap;

	if (!((mode & MDSS_MAX_BW_LIMIT_HFLIP) &&
				(mode & MDSS_MAX_BW_LIMIT_VFLIP)))
		mode |= mdss_check_for_flip(ctl);

	pr_debug("final mode = %d, bw_mode_bitmap = %d\n", mode,
			ctl->mdata->bw_mode_bitmap);

	/* Select BW mode with smallest limit */
	while (mode) {
		if (mode & BIT(0)) {
			threshold = max_bw_settings[i].mdss_max_bw_val;
			if (threshold < max)
				max = threshold;
		}
		mode >>= 1;
		i++;
	}

	return max;
}

int mdss_mdp_perf_bw_check(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_pipe **left_plist, int left_cnt,
		struct mdss_mdp_pipe **right_plist, int right_cnt)
{
	struct mdss_data_type *mdata = ctl->mdata;
	struct mdss_mdp_perf_params perf;
	u32 bw, threshold, i, mode_switch, max_bw;
	u64 bw_sum_of_intfs = 0;
	bool is_video_mode;

	/* we only need bandwidth check on real-time clients (interfaces) */
	if (ctl->intf_type == MDSS_MDP_NO_INTF)
		return 0;

	__mdss_mdp_perf_calc_ctl_helper(ctl, &perf,
			left_plist, left_cnt, right_plist, right_cnt,
			PERF_CALC_PIPE_CALC_SMP_SIZE);
	ctl->bw_pending = perf.bw_ctl;

	for (i = 0; i < mdata->nctl; i++) {
		struct mdss_mdp_ctl *temp = mdata->ctl_off + i;
		if (temp->power_state == MDSS_PANEL_POWER_ON  &&
				(temp->intf_type != MDSS_MDP_NO_INTF))
			bw_sum_of_intfs += temp->bw_pending;
	}

	/* convert bandwidth to kb */
	bw = DIV_ROUND_UP_ULL(bw_sum_of_intfs, 1000);
	pr_debug("calculated bandwidth=%uk\n", bw);

	/* mfd validation happens in func */
	mode_switch = mdss_fb_get_mode_switch(ctl->mfd);
	if (mode_switch)
		is_video_mode = (mode_switch == MIPI_VIDEO_PANEL);
	else
		is_video_mode = ctl->is_video_mode;
	threshold = (is_video_mode ||
		mdss_mdp_video_mode_intf_connected(ctl)) ?
		mdata->max_bw_low : mdata->max_bw_high;

	max_bw = mdss_mdp_set_threshold_max_bandwidth(ctl);

	if (max_bw && (max_bw < threshold))
		threshold = max_bw;

	pr_debug("final threshold bw limit = %d\n", threshold);

	if (bw > threshold) {
		ctl->bw_pending = 0;
		pr_debug("exceeds bandwidth: %ukb > %ukb\n", bw, threshold);
		return -E2BIG;
	}

	return 0;
}

static u32 mdss_mdp_get_bw_by_mode(struct mdss_max_bw_settings *settings,
					int count, int key)
{
	u32 value = 0, i = 0;

	while (i < count) {
		if (settings[i].mdss_max_bw_mode == key) {
			value = settings[i].mdss_max_bw_val;
			break;
		}
		++i;
	}
	return value;
}

static u32 mdss_mdp_get_max_pipe_bw(struct mdss_mdp_pipe *pipe)
{

	struct mdss_data_type *mdata = pipe->mixer_left->ctl->mdata;
	struct mdss_mdp_ctl *ctl = pipe->mixer_left->ctl;
	u32 flags = 0, threshold = 0, panel_orientation;

	panel_orientation = ctl->mfd->panel_orientation;

	/* Check for panel orienatation */
	panel_orientation = ctl->mfd->panel_orientation;
	if (panel_orientation & MDP_FLIP_LR)
		flags |= MDSS_MAX_BW_LIMIT_HFLIP;
	if (panel_orientation & MDP_FLIP_UD)
		flags |= MDSS_MAX_BW_LIMIT_VFLIP;

	/* check for Hflip/Vflip in pipe */
	if (pipe->flags & MDP_FLIP_LR)
		flags |= MDSS_MAX_BW_LIMIT_HFLIP;
	if (pipe->flags & MDP_FLIP_UD)
		flags |= MDSS_MAX_BW_LIMIT_VFLIP;

	if ((flags & MDSS_MAX_BW_LIMIT_HFLIP) &&
			(flags &  MDSS_MAX_BW_LIMIT_VFLIP)) {
		threshold = mdata->min_bw_per_pipe;
	} else if (flags & MDSS_MAX_BW_LIMIT_HFLIP) {
		threshold = mdss_mdp_get_bw_by_mode(
					mdata->max_per_pipe_bw_settings,
					mdata->mdss_per_pipe_bw_cnt,
					MDSS_MAX_BW_LIMIT_HFLIP);
	} else if (flags & MDSS_MAX_BW_LIMIT_VFLIP) {
		threshold = mdss_mdp_get_bw_by_mode(
					mdata->max_per_pipe_bw_settings,
					mdata->mdss_per_pipe_bw_cnt,
					MDSS_MAX_BW_LIMIT_VFLIP);
	}

	return threshold ? threshold : mdata->max_bw_per_pipe;
}

int mdss_mdp_perf_bw_check_pipe(struct mdss_mdp_perf_params *perf,
		struct mdss_mdp_pipe *pipe)
{
	struct mdss_data_type *mdata = pipe->mixer_left->ctl->mdata;
	struct mdss_mdp_ctl *ctl = pipe->mixer_left->ctl;
	u32 vbp_fac, threshold;
	u64 prefill_bw, pipe_bw;

	/* we only need bandwidth check on real-time clients (interfaces) */
	if (ctl->intf_type == MDSS_MDP_NO_INTF)
		return 0;

	if (test_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map)) {
		prefill_bw = perf->bw_prefill;
	} else {
		vbp_fac = mdss_mdp_get_vbp_factor_max(ctl);
		prefill_bw = perf->prefill_bytes * vbp_fac;
	}
	pipe_bw = max(prefill_bw, perf->bw_overlap);
	pr_debug("prefill=%llu, vbp_fac=%u, overlap=%llu\n",
			prefill_bw, vbp_fac, perf->bw_overlap);

	/* convert bandwidth to kb */
	pipe_bw = DIV_ROUND_UP_ULL(pipe_bw, 1000);


	if (!mdata->max_per_pipe_bw_settings)
		threshold = mdata->max_bw_per_pipe;
	else
		threshold = mdss_mdp_get_max_pipe_bw(pipe);

	pr_debug("bw=%llu threshold=%u\n", pipe_bw, threshold);

	if (threshold && pipe_bw > threshold) {
		pr_debug("pipe exceeds bandwidth: %llukb > %ukb\n", pipe_bw,
				threshold);
		return -E2BIG;
	}

	return 0;
}

static void mdss_mdp_perf_calc_ctl(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_perf_params *perf)
{
	struct mdss_mdp_pipe *left_plist[MAX_PIPES_PER_LM];
	struct mdss_mdp_pipe *right_plist[MAX_PIPES_PER_LM];
	int i, left_cnt = 0, right_cnt = 0;

	for (i = 0; i < MAX_PIPES_PER_LM; i++) {
		if (ctl->mixer_left && ctl->mixer_left->stage_pipe[i]) {
			left_plist[left_cnt] =
					ctl->mixer_left->stage_pipe[i];
			left_cnt++;
		}

		if (ctl->mixer_right && ctl->mixer_right->stage_pipe[i]) {
			right_plist[right_cnt] =
					ctl->mixer_right->stage_pipe[i];
			right_cnt++;
		}
	}

	__mdss_mdp_perf_calc_ctl_helper(ctl, perf,
		left_plist, left_cnt, right_plist, right_cnt, 0);

	if (ctl->is_video_mode || ((ctl->intf_type != MDSS_MDP_NO_INTF) &&
		mdss_mdp_video_mode_intf_connected(ctl))) {
		perf->bw_ctl =
			max(apply_fudge_factor(perf->bw_overlap,
				&mdss_res->ib_factor_overlap),
			apply_fudge_factor(perf->bw_prefill,
				&mdss_res->ib_factor));
		perf->bw_writeback = apply_fudge_factor(perf->bw_writeback,
				&mdss_res->ib_factor);
	}
	pr_debug("ctl=%d clk_rate=%u\n", ctl->num, perf->mdp_clk_rate);
	pr_debug("bw_overlap=%llu bw_prefill=%llu prefill_bytes=%d\n",
		 perf->bw_overlap, perf->bw_prefill, perf->prefill_bytes);
}

static void set_status(u32 *value, bool status, u32 bit_num)
{
	if (status)
		*value |= BIT(bit_num);
	else
		*value &= ~BIT(bit_num);
}

/**
 * @ mdss_mdp_ctl_perf_set_transaction_status() -
 *                             Set the status of the on-going operations
 *                             for the command mode panels.
 * @ctl - pointer to a ctl
 *
 * This function is called to set the status bit in the perf_transaction_status
 * according to the operation that it is on-going for the command mode
 * panels, where:
 *
 * PERF_SW_COMMIT_STATE:
 *           1 - If SW operation has been commited and bw
 *               has been requested (HW transaction have not started yet).
 *           0 - If there is no SW operation pending
 * PERF_HW_MDP_STATE:
 *           1 - If HW transaction is on-going
 *           0 - If there is no HW transaction on going (ping-pong interrupt
 *               has finished)
 * Only if both states are zero there are no pending operations and
 * BW could be released.
 * State can be queried calling "mdss_mdp_ctl_perf_get_transaction_status"
 */
void mdss_mdp_ctl_perf_set_transaction_status(struct mdss_mdp_ctl *ctl,
	enum mdss_mdp_perf_state_type component, bool new_status)
{
	u32  previous_transaction;
	bool previous_status;
	unsigned long flags;

	if (!ctl || !ctl->panel_data ||
		(ctl->panel_data->panel_info.type != MIPI_CMD_PANEL))
		return;

	spin_lock_irqsave(&ctl->spin_lock, flags);

	previous_transaction = ctl->perf_transaction_status;
	previous_status = previous_transaction & BIT(component) ?
		PERF_STATUS_BUSY : PERF_STATUS_DONE;

	/*
	 * If we set "done" state when previous state was not "busy",
	 * we want to print a warning since maybe there is a state
	 * that we are not considering
	 */
	WARN((PERF_STATUS_DONE == new_status) &&
		(PERF_STATUS_BUSY != previous_status),
		"unexpected previous state for component: %d\n", component);

	set_status(&ctl->perf_transaction_status, new_status,
		(u32)component);

	pr_debug("ctl:%d component:%d previous:%d status:%d\n",
		ctl->num, component, previous_transaction,
		ctl->perf_transaction_status);
	pr_debug("ctl:%d new_status:%d prev_status:%d\n",
		ctl->num, new_status, previous_status);

	spin_unlock_irqrestore(&ctl->spin_lock, flags);
}

/**
 * @ mdss_mdp_ctl_perf_get_transaction_status() -
 *                             Get the status of the on-going operations
 *                             for the command mode panels.
 * @ctl - pointer to a ctl
 *
 * Return:
 * The status of the transactions for the command mode panels,
 * note that the bandwidth can be released only if all transaction
 * status bits are zero.
 */
u32 mdss_mdp_ctl_perf_get_transaction_status(struct mdss_mdp_ctl *ctl)
{
	unsigned long flags;
	u32 transaction_status;

	if (!ctl)
		return PERF_STATUS_BUSY;

	/*
	 * If Rotator mode and bandwidth has been released; return STATUS_DONE
	 * so the bandwidth is re-calculated.
	 */
	if (ctl->mixer_left && ctl->mixer_left->rotator_mode &&
		!ctl->perf_release_ctl_bw)
			return PERF_STATUS_DONE;

	/*
	 * If Video Mode or not valid data to determine the status, return busy
	 * status, so the bandwidth cannot be freed by the caller
	 */
	if (!ctl || !ctl->panel_data ||
		(ctl->panel_data->panel_info.type != MIPI_CMD_PANEL)) {
		return PERF_STATUS_BUSY;
	}

	spin_lock_irqsave(&ctl->spin_lock, flags);
	transaction_status = ctl->perf_transaction_status;
	spin_unlock_irqrestore(&ctl->spin_lock, flags);
	pr_debug("ctl:%d status:%d\n", ctl->num,
		transaction_status);

	return transaction_status;
}

/**
 * @ mdss_mdp_ctl_perf_update_traffic_shaper_bw  -
 *				Apply BW fudge factor to rotator
 *				if mdp clock increased during
 *				rotation session.
 * @ctl - pointer to the controller
 * @mdp_clk - new mdp clock
 *
 * If mdp clock increased and traffic shaper is enabled, we need to
 * account for the additional bandwidth that will be requested by
 * the rotator when running at a higher clock, so we apply a fudge
 * factor proportional to the mdp clock increment.
 */
static void mdss_mdp_ctl_perf_update_traffic_shaper_bw(struct mdss_mdp_ctl *ctl,
		u32 mdp_clk)
{
	if ((mdp_clk > 0) && (mdp_clk > ctl->traffic_shaper_mdp_clk)) {
		ctl->cur_perf.bw_ctl = fudge_factor(ctl->cur_perf.bw_ctl,
			mdp_clk, ctl->traffic_shaper_mdp_clk);
		pr_debug("traffic shaper bw:%llu, clk: %d,  mdp_clk:%d\n",
			ctl->cur_perf.bw_ctl, ctl->traffic_shaper_mdp_clk,
				mdp_clk);
	}
}

static u64 mdss_mdp_ctl_calc_client_vote(struct mdss_data_type *mdata,
	struct mdss_mdp_perf_params *perf, bool nrt_client, u32 mdp_clk)
{
	u64 bw_sum_of_intfs = 0;
	int i;
	struct mdss_mdp_ctl *ctl;
	struct mdss_mdp_mixer *mixer;

	for (i = 0; i < mdata->nctl; i++) {
		ctl = mdata->ctl_off + i;
		mixer = ctl->mixer_left;
		if (mdss_mdp_ctl_is_power_on(ctl) &&
		    /* RealTime clients */
		    ((!nrt_client && ctl->mixer_left &&
			!ctl->mixer_left->rotator_mode) ||
		    /* Non-RealTime clients */
		    (nrt_client && mdss_mdp_is_nrt_ctl_path(ctl)))) {
			/* Skip rotation layers as bw calc by rot driver */
			if (ctl->mixer_left && ctl->mixer_left->rotator_mode)
				continue;
			/*
			 * If traffic shaper is enabled we must check
			 * if additional bandwidth is required.
			 */
			if (ctl->traffic_shaper_enabled)
				mdss_mdp_ctl_perf_update_traffic_shaper_bw
					(ctl, mdp_clk);

			if (ctl->cur_perf.bw_vote_mode)
				bitmap_or(perf->bw_vote_mode,
					perf->bw_vote_mode,
					ctl->cur_perf.bw_vote_mode,
					MDSS_MDP_BW_MODE_MAX);

			if (nrt_client && ctl->mixer_left &&
				!ctl->mixer_left->rotator_mode) {
				bw_sum_of_intfs += ctl->cur_perf.bw_writeback;
				continue;
			}

			perf->max_per_pipe_ib = max(perf->max_per_pipe_ib,
				ctl->cur_perf.max_per_pipe_ib);

			bw_sum_of_intfs += ctl->cur_perf.bw_ctl;

			pr_debug("ctl_num=%d bw=%llu mode=0x%lx\n", ctl->num,
				ctl->cur_perf.bw_ctl,
				*(ctl->cur_perf.bw_vote_mode));
		}
	}

	return bw_sum_of_intfs;
}

static void mdss_mdp_ctl_update_client_vote(struct mdss_data_type *mdata,
	struct mdss_mdp_perf_params *perf, bool nrt_client, u64 bw_vote)
{
	u64 bus_ab_quota, bus_ib_quota;

	bus_ab_quota = max(bw_vote, mdata->perf_tune.min_bus_vote);

	if (test_bit(MDSS_QOS_PER_PIPE_IB, mdata->mdss_qos_map)) {
		if (!nrt_client)
			bus_ib_quota = perf->max_per_pipe_ib;
		else
			bus_ib_quota = 0;
	} else {
		bus_ib_quota = bw_vote;
	}

	if (test_bit(MDSS_MDP_BW_MODE_SINGLE_LAYER,
		perf->bw_vote_mode) &&
		(bus_ib_quota >= PERF_SINGLE_PIPE_BW_FLOOR)) {
		struct mult_factor ib_factor_vscaling;
		ib_factor_vscaling.numer = 2;
		ib_factor_vscaling.denom = 1;
		bus_ib_quota = apply_fudge_factor(bus_ib_quota,
			&ib_factor_vscaling);
	}

	if (test_bit(MDSS_QOS_PER_PIPE_IB, mdata->mdss_qos_map) &&
			!nrt_client)
		bus_ib_quota = apply_fudge_factor(bus_ib_quota,
			&mdata->per_pipe_ib_factor);

	bus_ab_quota = apply_fudge_factor(bus_ab_quota, &mdss_res->ab_factor);
	ATRACE_INT("bus_quota", bus_ib_quota);

	mdss_bus_scale_set_quota(nrt_client ? MDSS_MDP_NRT : MDSS_MDP_RT,
		bus_ab_quota, bus_ib_quota);
	pr_debug("client:%s ab=%llu ib=%llu\n", nrt_client ? "nrt" : "rt",
		bus_ab_quota, bus_ib_quota);
}

static void mdss_mdp_ctl_perf_update_bus(struct mdss_data_type *mdata,
	struct mdss_mdp_ctl *ctl, u32 mdp_clk)
{
	u64 bw_sum_of_rt_intfs = 0, bw_sum_of_nrt_intfs = 0;
	struct mdss_mdp_perf_params perf = {0};

	ATRACE_BEGIN(__func__);

	/*
	 * non-real time client
	 * 1. rotator path
	 * 2. writeback output path
	 */
	if (mdss_mdp_is_nrt_ctl_path(ctl)) {
		bitmap_zero(perf.bw_vote_mode, MDSS_MDP_BW_MODE_MAX);
		bw_sum_of_nrt_intfs = mdss_mdp_ctl_calc_client_vote(mdata,
			&perf, true, mdp_clk);
		mdss_mdp_ctl_update_client_vote(mdata, &perf, true,
			bw_sum_of_nrt_intfs);
	}

	/*
	 * real time client
	 * 1. any realtime interface - primary or secondary interface
	 * 2. writeback input path
	 */
	if (!mdss_mdp_is_nrt_ctl_path(ctl) ||
		(ctl->intf_num ==  MDSS_MDP_NO_INTF)) {
		bitmap_zero(perf.bw_vote_mode, MDSS_MDP_BW_MODE_MAX);
		bw_sum_of_rt_intfs = mdss_mdp_ctl_calc_client_vote(mdata,
			&perf, false, mdp_clk);
		mdss_mdp_ctl_update_client_vote(mdata, &perf, false,
			bw_sum_of_rt_intfs);
	}

	ATRACE_END(__func__);
}

/**
 * @mdss_mdp_ctl_perf_release_bw() - request zero bandwidth
 * @ctl - pointer to a ctl
 *
 * Function checks a state variable for the ctl, if all pending commit
 * requests are done, meaning no more bandwidth is needed, release
 * bandwidth request.
 */
void mdss_mdp_ctl_perf_release_bw(struct mdss_mdp_ctl *ctl)
{
	int transaction_status;
	struct mdss_data_type *mdata;
	int i;

	/* only do this for command panel */
	if (!ctl || !ctl->mdata || !ctl->panel_data ||
		(ctl->panel_data->panel_info.type != MIPI_CMD_PANEL))
		return;

	mutex_lock(&mdss_mdp_ctl_lock);
	mdata = ctl->mdata;
	/*
	 * If video interface present, cmd panel bandwidth cannot be
	 * released.
	 */
	for (i = 0; i < mdata->nctl; i++) {
		struct mdss_mdp_ctl *ctl_local = mdata->ctl_off + i;

		if (mdss_mdp_ctl_is_power_on(ctl_local) &&
			ctl_local->is_video_mode)
			goto exit;
	}

	transaction_status = mdss_mdp_ctl_perf_get_transaction_status(ctl);
	pr_debug("transaction_status=0x%x\n", transaction_status);

	/*Release the bandwidth only if there are no transactions pending*/
	if (!transaction_status && mdata->enable_bw_release) {
		/*
		 * for splitdisplay if release_bw is called using secondary
		 * then find the main ctl and release BW for main ctl because
		 * BW is always calculated/stored using main ctl.
		 */
		struct mdss_mdp_ctl *ctl_local =
			mdss_mdp_get_main_ctl(ctl) ? : ctl;

		trace_mdp_cmd_release_bw(ctl_local->num);
		ctl_local->cur_perf.bw_ctl = 0;
		ctl_local->new_perf.bw_ctl = 0;
		pr_debug("Release BW ctl=%d\n", ctl_local->num);
		mdss_mdp_ctl_perf_update_bus(mdata, ctl, 0);
	}
exit:
	mutex_unlock(&mdss_mdp_ctl_lock);
}

static int mdss_mdp_select_clk_lvl(struct mdss_data_type *mdata,
			u32 clk_rate)
{
	int i;
	for (i = 0; i < mdata->nclk_lvl; i++) {
		if (clk_rate > mdata->clock_levels[i]) {
			continue;
		} else {
			clk_rate = mdata->clock_levels[i];
			break;
		}
	}

	return clk_rate;
}

static void mdss_mdp_perf_release_ctl_bw(struct mdss_mdp_ctl *ctl,
	struct mdss_mdp_perf_params *perf)
{
	/* Set to zero controller bandwidth. */
	memset(perf, 0, sizeof(*perf));
	ctl->perf_release_ctl_bw = false;
}

u32 mdss_mdp_get_mdp_clk_rate(struct mdss_data_type *mdata)
{
	u32 clk_rate = 0;
	uint i;
	struct clk *clk = mdss_mdp_get_clk(MDSS_CLK_MDP_CORE);

	for (i = 0; i < mdata->nctl; i++) {
		struct mdss_mdp_ctl *ctl;
		ctl = mdata->ctl_off + i;
		if (mdss_mdp_ctl_is_power_on(ctl)) {
			clk_rate = max(ctl->cur_perf.mdp_clk_rate,
							clk_rate);
			clk_rate = clk_round_rate(clk, clk_rate);
		}
	}
	clk_rate  = mdss_mdp_select_clk_lvl(mdata, clk_rate);

	pr_debug("clk:%u nctl:%d\n", clk_rate, mdata->nctl);
	return clk_rate;
}

static bool is_traffic_shaper_enabled(struct mdss_data_type *mdata)
{
	uint i;
	for (i = 0; i < mdata->nctl; i++) {
		struct mdss_mdp_ctl *ctl;
		ctl = mdata->ctl_off + i;
		if (mdss_mdp_ctl_is_power_on(ctl))
			if (ctl->traffic_shaper_enabled)
				return true;
	}
	return false;
}

static void mdss_mdp_ctl_perf_update(struct mdss_mdp_ctl *ctl,
		int params_changed, bool stop_req)
{
	struct mdss_mdp_perf_params *new, *old;
	int update_bus = 0, update_clk = 0;
	struct mdss_data_type *mdata;
	bool is_bw_released;
	u32 clk_rate = 0;

	if (!ctl || !ctl->mdata)
		return;
	ATRACE_BEGIN(__func__);
	mutex_lock(&mdss_mdp_ctl_lock);

	mdata = ctl->mdata;
	old = &ctl->cur_perf;
	new = &ctl->new_perf;

	/*
	 * We could have released the bandwidth if there were no transactions
	 * pending, so we want to re-calculate the bandwidth in this situation.
	 */
	is_bw_released = !mdss_mdp_ctl_perf_get_transaction_status(ctl);

	if (mdss_mdp_ctl_is_power_on(ctl)) {
		/* Skip perf update if ctl is used for rotation */
		if (ctl->mixer_left && ctl->mixer_left->rotator_mode)
			goto end;

		if (ctl->perf_release_ctl_bw &&
			mdata->enable_rotator_bw_release)
			mdss_mdp_perf_release_ctl_bw(ctl, new);
		else if (is_bw_released || params_changed)
			mdss_mdp_perf_calc_ctl(ctl, new);

		/*
		 * three cases for bus bandwidth update.
		 * 1. new bandwidth vote or writeback output vote
		 *    are higher than current vote for update request.
		 * 2. new bandwidth vote or writeback output vote are
		 *    lower than current vote at end of commit or stop.
		 * 3. end of writeback/rotator session - last chance to
		 *    non-realtime remove vote.
		 */
		if ((params_changed && ((new->bw_ctl > old->bw_ctl) ||
			(new->bw_writeback > old->bw_writeback))) ||
		    (!params_changed && ((new->bw_ctl < old->bw_ctl) ||
			(new->bw_writeback < old->bw_writeback))) ||
			(stop_req && mdss_mdp_is_nrt_ctl_path(ctl))) {

			pr_debug("c=%d p=%d new_bw=%llu,old_bw=%llu\n",
				ctl->num, params_changed, new->bw_ctl,
				old->bw_ctl);
			if (stop_req) {
				old->bw_writeback = 0;
				old->bw_ctl = 0;
				old->max_per_pipe_ib = 0;
			} else {
				old->bw_ctl = new->bw_ctl;
				old->max_per_pipe_ib = new->max_per_pipe_ib;
				old->bw_writeback = new->bw_writeback;
			}
			bitmap_copy(old->bw_vote_mode, new->bw_vote_mode,
				MDSS_MDP_BW_MODE_MAX);
			update_bus = 1;
		}

		/*
		 * If traffic shaper is enabled, we do not decrease the clock,
		 * otherwise we would increase traffic shaper latency. Clock
		 * would be decreased after traffic shaper is done.
		 */
		if ((params_changed && (new->mdp_clk_rate > old->mdp_clk_rate))
			 || (!params_changed &&
			 (new->mdp_clk_rate < old->mdp_clk_rate) &&
			(false == is_traffic_shaper_enabled(mdata)))) {
			old->mdp_clk_rate = new->mdp_clk_rate;
			update_clk = 1;
		}
	} else {
		memset(old, 0, sizeof(*old));
		memset(new, 0, sizeof(*new));
		update_bus = 1;
		update_clk = 1;
	}

	/*
	 * Calculate mdp clock before bandwidth calculation. If traffic shaper
	 * is enabled and clock increased, the bandwidth calculation can
	 * use the new clock for the rotator bw calculation.
	 */
	if (update_clk)
		clk_rate = mdss_mdp_get_mdp_clk_rate(mdata);

	if (update_bus)
		mdss_mdp_ctl_perf_update_bus(mdata, ctl, clk_rate);

	/*
	 * Update the clock after bandwidth vote to ensure
	 * bandwidth is available before clock rate is increased.
	 */
	if (update_clk) {
		ATRACE_INT("mdp_clk", clk_rate);
		mdss_mdp_set_clk_rate(clk_rate);
		pr_debug("update clk rate = %d HZ\n", clk_rate);
	}

end:
	mutex_unlock(&mdss_mdp_ctl_lock);
	ATRACE_END(__func__);
}

struct mdss_mdp_ctl *mdss_mdp_ctl_alloc(struct mdss_data_type *mdata,
					       u32 off)
{
	struct mdss_mdp_ctl *ctl = NULL;
	u32 cnum;
	u32 nctl = mdata->nctl;

	mutex_lock(&mdss_mdp_ctl_lock);
	if (mdata->wfd_mode == MDSS_MDP_WFD_SHARED)
		nctl++;

	for (cnum = off; cnum < nctl; cnum++) {
		ctl = mdata->ctl_off + cnum;
		if (ctl->ref_cnt == 0) {
			ctl->ref_cnt++;
			ctl->mdata = mdata;
			mutex_init(&ctl->lock);
			mutex_init(&ctl->offlock);
			mutex_init(&ctl->flush_lock);
			mutex_init(&ctl->rsrc_lock);
			spin_lock_init(&ctl->spin_lock);
			BLOCKING_INIT_NOTIFIER_HEAD(&ctl->notifier_head);
			pr_debug("alloc ctl_num=%d\n", ctl->num);
			break;
		}
		ctl = NULL;
	}
	mutex_unlock(&mdss_mdp_ctl_lock);

	return ctl;
}

int mdss_mdp_ctl_free(struct mdss_mdp_ctl *ctl)
{
	if (!ctl)
		return -ENODEV;

	pr_debug("free ctl_num=%d ref_cnt=%d\n", ctl->num, ctl->ref_cnt);

	if (!ctl->ref_cnt) {
		pr_err("called with ref_cnt=0\n");
		return -EINVAL;
	}

	if (ctl->mixer_left && ctl->mixer_left->ref_cnt)
		mdss_mdp_mixer_free(ctl->mixer_left);

	if (ctl->mixer_right && ctl->mixer_right->ref_cnt)
		mdss_mdp_mixer_free(ctl->mixer_right);

	if (ctl->wb)
		mdss_mdp_wb_free(ctl->wb);

	mutex_lock(&mdss_mdp_ctl_lock);
	ctl->ref_cnt--;
	ctl->intf_num = MDSS_MDP_NO_INTF;
	ctl->intf_type = MDSS_MDP_NO_INTF;
	ctl->is_secure = false;
	ctl->power_state = MDSS_PANEL_POWER_OFF;
	ctl->mixer_left = NULL;
	ctl->mixer_right = NULL;
	ctl->wb = NULL;
	ctl->cdm = NULL;
	memset(&ctl->ops, 0, sizeof(ctl->ops));
	mutex_unlock(&mdss_mdp_ctl_lock);

	return 0;
}

/**
 * mdss_mdp_mixer_alloc() - allocate mdp mixer.
 * @ctl: mdp controller.
 * @type: specifying type of mixer requested. interface or writeback.
 * @mux: specifies if mixer allocation is for split_fb cases.
 * @rotator: specifies if the mixer requested for rotator operations.
 *
 * This function is called to request allocation of mdp mixer
 * during mdp controller path setup.
 *
 * Return: mdp mixer structure that is allocated.
 *	   NULL if mixer allocation fails.
 */
struct mdss_mdp_mixer *mdss_mdp_mixer_alloc(
		struct mdss_mdp_ctl *ctl, u32 type, int mux, int rotator)
{
	struct mdss_mdp_mixer *mixer = NULL, *alt_mixer = NULL;
	u32 nmixers_intf;
	u32 nmixers_wb;
	u32 i;
	u32 nmixers;
	struct mdss_mdp_mixer *mixer_pool = NULL;

	if (!ctl || !ctl->mdata)
		return NULL;

	mutex_lock(&mdss_mdp_ctl_lock);
	nmixers_intf = ctl->mdata->nmixers_intf;
	nmixers_wb = ctl->mdata->nmixers_wb;

	switch (type) {
	case MDSS_MDP_MIXER_TYPE_INTF:
		mixer_pool = ctl->mdata->mixer_intf;
		nmixers = nmixers_intf;

		/*
		 * try to reserve first layer mixer for write back if
		 * assertive display needs to be supported through wfd
		 */
		if (ctl->mdata->has_wb_ad && ctl->intf_num &&
			((ctl->panel_data->panel_info.type != MIPI_CMD_PANEL) ||
			!mux)) {
			alt_mixer = mixer_pool;
			mixer_pool++;
			nmixers--;
		} else if ((ctl->panel_data->panel_info.type == WRITEBACK_PANEL)
			&& (ctl->mdata->ndspp < nmixers)) {
			mixer_pool += ctl->mdata->ndspp;
			nmixers -= ctl->mdata->ndspp;
		}
		break;

	case MDSS_MDP_MIXER_TYPE_WRITEBACK:
		mixer_pool = ctl->mdata->mixer_wb;
		nmixers = nmixers_wb;
		break;

	default:
		nmixers = 0;
		pr_err("invalid pipe type %d\n", type);
		break;
	}

	/*Allocate virtual wb mixer if no dedicated wfd wb blk is present*/
	if ((ctl->mdata->wfd_mode == MDSS_MDP_WFD_SHARED) &&
			(type == MDSS_MDP_MIXER_TYPE_WRITEBACK))
		nmixers += 1;

	for (i = 0; i < nmixers; i++) {
		mixer = mixer_pool + i;
		if (mixer->ref_cnt == 0)
			break;
		mixer = NULL;
	}

	if (!mixer && alt_mixer && (alt_mixer->ref_cnt == 0))
		mixer = alt_mixer;

	if (mixer) {
		mixer->ref_cnt++;
		mixer->params_changed++;
		mixer->ctl = ctl;
		mixer->next_pipe_map = 0;
		mixer->pipe_mapped = 0;
		pr_debug("alloc mixer num %d for ctl=%d\n",
				mixer->num, ctl->num);
	}
	mutex_unlock(&mdss_mdp_ctl_lock);

	return mixer;
}

struct mdss_mdp_mixer *mdss_mdp_mixer_assign(u32 id, bool wb)
{
	struct mdss_mdp_mixer *mixer = NULL;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	mutex_lock(&mdss_mdp_ctl_lock);

	if (wb && id < mdata->nmixers_wb)
		mixer = mdata->mixer_wb + id;
	else if (!wb && id < mdata->nmixers_intf)
		mixer = mdata->mixer_intf + id;

	if (mixer && mixer->ref_cnt == 0) {
		mixer->ref_cnt++;
		mixer->params_changed++;
	} else {
		pr_err("mixer is in use already = %d\n", id);
		mixer = NULL;
	}
	mutex_unlock(&mdss_mdp_ctl_lock);
	return mixer;
}

int mdss_mdp_mixer_free(struct mdss_mdp_mixer *mixer)
{
	if (!mixer)
		return -ENODEV;

	pr_debug("free mixer_num=%d ref_cnt=%d\n", mixer->num, mixer->ref_cnt);

	if (!mixer->ref_cnt) {
		pr_err("called with ref_cnt=0\n");
		return -EINVAL;
	}

	mutex_lock(&mdss_mdp_ctl_lock);
	mixer->ref_cnt--;
	mixer->is_right_mixer = false;
	mutex_unlock(&mdss_mdp_ctl_lock);

	return 0;
}

struct mdss_mdp_mixer *mdss_mdp_block_mixer_alloc(void)
{
	struct mdss_mdp_ctl *ctl = NULL;
	struct mdss_mdp_mixer *mixer = NULL;
	struct mdss_mdp_writeback *wb = NULL;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 offset = mdss_mdp_get_wb_ctl_support(mdata, true);
	int ret = 0;

	ctl = mdss_mdp_ctl_alloc(mdss_res, offset);
	if (!ctl) {
		pr_debug("unable to allocate wb ctl\n");
		return NULL;
	}

	mixer = mdss_mdp_mixer_alloc(ctl, MDSS_MDP_MIXER_TYPE_WRITEBACK,
							false, true);
	if (!mixer) {
		pr_debug("unable to allocate wb mixer\n");
		goto error;
	}

	mixer->rotator_mode = 1;

	switch (mixer->num) {
	case MDSS_MDP_WB_LAYERMIXER0:
		ctl->opmode = MDSS_MDP_CTL_OP_ROT0_MODE;
		break;
	case MDSS_MDP_WB_LAYERMIXER1:
		ctl->opmode = MDSS_MDP_CTL_OP_ROT1_MODE;
		break;
	default:
		pr_err("invalid layer mixer=%d\n", mixer->num);
		goto error;
	}

	wb = mdss_mdp_wb_alloc(MDSS_MDP_WB_ROTATOR, ctl->num);
	if (!wb) {
		pr_err("Unable to allocate writeback block\n");
		goto error;
	}

	ctl->mixer_left = mixer;

	ctl->ops.start_fnc = mdss_mdp_writeback_start;
	ctl->power_state = MDSS_PANEL_POWER_ON;
	ctl->wb_type = MDSS_MDP_WB_CTL_TYPE_BLOCK;
	mixer->ctl = ctl;
	ctl->wb = wb;

	if (ctl->ops.start_fnc)
		ret = ctl->ops.start_fnc(ctl);

	if (!ret)
		return mixer;
error:
	if (wb)
		mdss_mdp_wb_free(wb);
	if (mixer)
		mdss_mdp_mixer_free(mixer);
	if (ctl)
		mdss_mdp_ctl_free(ctl);

	return NULL;
}

int mdss_mdp_block_mixer_destroy(struct mdss_mdp_mixer *mixer)
{
	struct mdss_mdp_ctl *ctl;

	if (!mixer || !mixer->ctl) {
		pr_err("invalid ctl handle\n");
		return -ENODEV;
	}

	ctl = mixer->ctl;
	mixer->rotator_mode = 0;

	pr_debug("destroy ctl=%d mixer=%d\n", ctl->num, mixer->num);

	if (ctl->ops.stop_fnc)
		ctl->ops.stop_fnc(ctl, MDSS_PANEL_POWER_OFF);

	mdss_mdp_ctl_free(ctl);

	mdss_mdp_ctl_perf_update(ctl, 0, true);

	return 0;
}

int mdss_mdp_ctl_cmd_autorefresh_enable(struct mdss_mdp_ctl *ctl,
		int frame_cnt)
{
	int ret = 0;
	if (ctl->panel_data->panel_info.type == MIPI_CMD_PANEL) {
		ret = mdss_mdp_cmd_set_autorefresh_mode(ctl, frame_cnt);
	} else {
		pr_err("Mode not supported for this panel\n");
		ret = -EINVAL;
	}
	return ret;
}

int mdss_mdp_ctl_splash_finish(struct mdss_mdp_ctl *ctl, bool handoff)
{
	switch (ctl->panel_data->panel_info.type) {
	case MIPI_VIDEO_PANEL:
	case EDP_PANEL:
		return mdss_mdp_video_reconfigure_splash_done(ctl, handoff);
	case MIPI_CMD_PANEL:
		return mdss_mdp_cmd_reconfigure_splash_done(ctl, handoff);
	default:
		return 0;
	}
}

static inline int mdss_mdp_set_split_ctl(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_ctl *split_ctl)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_panel_info *pinfo;


	if (!ctl || !split_ctl || !mdata)
		return -ENODEV;

	/* setup split ctl mixer as right mixer of original ctl so that
	 * original ctl can work the same way as dual pipe solution */
	ctl->mixer_right = split_ctl->mixer_left;
	pinfo = &ctl->panel_data->panel_info;

	/* add x offset from left ctl's border */
	split_ctl->border_x_off += (pinfo->lcdc.border_left +
					pinfo->lcdc.border_right);

	return 0;
}

static inline void mdss_mdp_ctl_dsc_enable(struct mdss_mdp_mixer *mixer)
{
	mdss_mdp_pingpong_write(mixer->pingpong_base,
			MDSS_MDP_REG_PP_DSC_MODE, 1);
}

static inline void mdss_mdp_ctl_dsc_disable(struct mdss_mdp_mixer *mixer)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	char __iomem *offset = mdata->mdp_base;

	mdss_mdp_pingpong_write(mixer->pingpong_base,
			MDSS_MDP_REG_PP_DSC_MODE, 0);

	if (mixer->num == MDSS_MDP_INTF_LAYERMIXER0) {
		offset += MDSS_MDP_DSC_0_OFFSET;
	} else if (mixer->num == MDSS_MDP_INTF_LAYERMIXER1) {
		offset += MDSS_MDP_DSC_1_OFFSET;
	} else {
		pr_err("invalid mixer numer=%d\n", mixer->num);
		return;
	}
	writel_relaxed(0, offset + MDSS_MDP_REG_DSC_COMMON_MODE);
}

static void mdss_mdp_ctl_dsc_config(struct mdss_mdp_mixer *mixer,
	struct dsc_desc *dsc, u32 mode, bool ich_reset_override)
{
	u32 data;
	int bpp, lsb;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	char __iomem *offset = mdata->mdp_base;

	data = mdss_mdp_pingpong_read(mixer->pingpong_base,
			MDSS_MDP_REG_PP_DCE_DATA_OUT_SWAP);
	data |= BIT(18); /* endian flip */
	mdss_mdp_pingpong_write(mixer->pingpong_base,
		MDSS_MDP_REG_PP_DCE_DATA_OUT_SWAP, data);

	if (mixer->num == MDSS_MDP_INTF_LAYERMIXER0) {
		offset += MDSS_MDP_DSC_0_OFFSET;
	} else if (mixer->num == MDSS_MDP_INTF_LAYERMIXER1) {
		offset += MDSS_MDP_DSC_1_OFFSET;
	} else {
		pr_err("invalid mixer numer=%d\n", mixer->num);
		return;
	}

	writel_relaxed(mode, offset + MDSS_MDP_REG_DSC_COMMON_MODE);

	data = 0;
	if (ich_reset_override)
		data = 3 << 28;

	data |= (dsc->initial_lines << 20);
	data |= ((dsc->slice_last_group_size - 1) << 18);
	/* bpp is 6.4 format, 4 LSBs bits are for fractional part */
	lsb = dsc->bpp % 4;
	bpp = dsc->bpp / 4;
	bpp *= 4;	/* either 8 or 12 */
	bpp <<= 4;
	bpp |= lsb;
	data |= (bpp << 8);
	data |= (dsc->block_pred_enable << 7);
	data |= (dsc->line_buf_depth << 3);
	data |= (dsc->enable_422 << 2);
	data |= (dsc->convert_rgb << 1);
	data |= dsc->input_10_bits;

	pr_debug("%d %d %d %d %d %d %d %d %d, data=%x\n",
		ich_reset_override,
		dsc->initial_lines , dsc->slice_last_group_size,
		dsc->bpp, dsc->block_pred_enable, dsc->line_buf_depth,
		dsc->enable_422, dsc->convert_rgb, dsc->input_10_bits, data);

	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_ENC);

	data = dsc->pic_width << 16;
	data |= dsc->pic_height;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_PICTURE);

	data = dsc->slice_width << 16;
	data |= dsc->slice_height;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_SLICE);

	data = dsc->chunk_size << 16;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_CHUNK_SIZE);

	pr_debug("mix%d pic_w=%d pic_h=%d, slice_w=%d slice_h=%d, chunk=%d\n",
		mixer->num, dsc->pic_width, dsc->pic_height,
		dsc->slice_width, dsc->slice_height, dsc->chunk_size);
	MDSS_XLOG(mixer->num, dsc->pic_width, dsc->pic_height,
		dsc->slice_width, dsc->slice_height, dsc->chunk_size);

	data = dsc->initial_dec_delay << 16;
	data |= dsc->initial_xmit_delay;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_DELAY);

	data = dsc->initial_scale_value;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_SCALE_INITIAL);

	data = dsc->scale_decrement_interval;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_SCALE_DEC_INTERVAL);

	data = dsc->scale_increment_interval;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_SCALE_INC_INTERVAL);

	data = dsc->first_line_bpg_offset;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_FIRST_LINE_BPG_OFFSET);

	data = dsc->nfl_bpg_offset << 16;
	data |= dsc->slice_bpg_offset;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_BPG_OFFSET);

	data = dsc->initial_offset << 16;
	data |= dsc->final_offset;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_DSC_OFFSET);

	data = dsc->det_thresh_flatness << 10;
	data |= dsc->max_qp_flatness << 5;
	data |= dsc->min_qp_flatness;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_FLATNESS);
	writel_relaxed(0x983, offset + MDSS_MDP_REG_DSC_FLATNESS);

	data = dsc->rc_model_size;	/* rate_buffer_size */
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_RC_MODEL_SIZE);

	data = dsc->tgt_offset_lo << 18;
	data |= dsc->tgt_offset_hi << 14;
	data |= dsc->quant_incr_limit1 << 9;
	data |= dsc->quant_incr_limit0 << 4;
	data |= dsc->edge_factor;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_RC);
}

static void mdss_mdp_ctl_dsc_config_thresh(struct mdss_mdp_mixer *mixer,
				struct mdss_panel_info *pinfo)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct dsc_desc *dsc = &pinfo->dsc;
	char __iomem *offset, *off;
	u32 *lp;
	char *cp;
	int i;

	offset = mdata->mdp_base;

	if (mixer->num == MDSS_MDP_INTF_LAYERMIXER0) {
		offset += MDSS_MDP_DSC_0_OFFSET;
	} else if (mixer->num == MDSS_MDP_INTF_LAYERMIXER1) {
		offset += MDSS_MDP_DSC_1_OFFSET;
	} else {
		pr_err("invalid mixer numer=%d\n", mixer->num);
		return;
	}

	lp = dsc->buf_thresh;
	off = offset + MDSS_MDP_REG_DSC_RC_BUF_THRESH;
	for (i = 0; i < 14; i++) {
		writel_relaxed(*lp++, off);
		off += 4;
	}

	cp = dsc->range_min_qp;
	off = offset + MDSS_MDP_REG_DSC_RANGE_MIN_QP;
	for (i = 0; i < 15; i++) {
		writel_relaxed(*cp++, off);
		off += 4;
	}

	cp = dsc->range_max_qp;
	off = offset + MDSS_MDP_REG_DSC_RANGE_MAX_QP;
	for (i = 0; i < 15; i++) {
		writel_relaxed(*cp++, off);
		off += 4;
	}

	cp = dsc->range_bpg_offset;
	off = offset + MDSS_MDP_REG_DSC_RANGE_BPG_OFFSET;
	for (i = 0; i < 15; i++) {
		writel_relaxed(*cp++, off);
		off += 4;
	}
}

void mdss_mdp_ctl_dsc_setup(struct mdss_mdp_ctl *ctl,
	struct mdss_panel_info *pinfo)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_mixer *mixer_left = ctl->mixer_left;
	struct mdss_mdp_mixer *mixer_right = NULL;
	struct dsc_desc *dsc = &pinfo->dsc;
	u32 pic_width = 0, pic_height = 0;
	u32 mode = 0;
	bool recalc_dsc_params = false;
	bool ich_reset_override = false;
	bool dsc_merge = false, mux_3d = false;
	bool left_valid = false, right_valid = false;

	if (!ctl->is_master) {
		pr_debug("skip slave ctl because master will program for both\n");
		return;
	}

	if (pinfo->type == MIPI_VIDEO_PANEL)
		mode = BIT(2);

	/* pingpong split with DSC needs to be handled */
	if (is_split_lm(ctl->mfd)) {
		mixer_right = ctl->mixer_right;
		if (is_dual_lm_single_display(ctl->mfd)) {
			if (pinfo->dsc_enc_total == 2) {
				/* DSC Merge */
				if (mdss_mdp_is_both_lm_valid(ctl)) {
					mode |= BIT(1);
					dsc_merge = true;
				}
			} else {
				mux_3d = true;
			}
		}

		/*
		 * two independent decoders on DDIC requires
		 * split 2p2d mode but it is not supported yet.
		 */
		if (mdss_mdp_is_both_lm_valid(ctl) && !mux_3d)
			mode |= BIT(0); /* assumming 1 decoder on panel side */
	}

	left_valid = mixer_left->valid_roi;
	right_valid = mixer_right && mixer_right->valid_roi;

	if (left_valid) {
		pic_width = mixer_left->roi.w;
		pic_height = mixer_left->roi.h;
		recalc_dsc_params = true;
	}

	if (mixer_right && right_valid) {
		pic_width += mixer_right->roi.w;
		pic_height = mixer_right->roi.h; /* height on both lm is same */

		if (is_dual_lm_single_display(ctl->mfd)) {
			recalc_dsc_params = true;
		} else { /* DUAL_LM_DUAL_DISPLAY */
			struct mdss_mdp_ctl *sctl = mdss_mdp_get_split_ctl(ctl);
			struct mdss_panel_info *spinfo =
				&sctl->panel_data->panel_info;

			mdss_dsc_parameters_calc(&spinfo->dsc,
				pic_width, pic_height);
		}
	}

	/* re-calculate DSC params before configuring them to MDP */
	if (recalc_dsc_params)
		mdss_dsc_parameters_calc(dsc, pic_width, pic_height);

	/*
	 * As per the DSC spec, ICH_RESET can be either end of the slice line
	 * or at the end of the slice. HW internally generates ich_reset at
	 * end of the slice line if DSC_MERGE is used or encoder has two
	 * soft slices. However, if encoder has only 1 soft slice and DSC_MERGE
	 * is not used then it will generate ich_reset at the end of slice.
	 *
	 * Now as per the spec, during one PPS session, position where
	 * ich_reset is generated should not change. Now if full-screen frame
	 * has more than 1 soft slice then HW will automatically generate
	 * ich_reset at the end of slice_line. But for the same panel, if
	 * partial frame is enabled and only 1 encoder is used with 1 slice,
	 * then HW will generate ich_reset at end of the slice. This is a
	 * mismatch. Prevent this by overriding HW's decision.
	 */
	if (pinfo->partial_update_enabled && !dsc_merge &&
	    (dsc->full_frame_slices > 1) &&
	    (dsc->slice_width == dsc->pic_width))
		ich_reset_override = true;

	/*
	 * From this point onwards, left_valid and right_valid variables
	 * will represent only DSC encoders and does not represent use of
	 * layer mixers. So if both are valid, both encoders are used. If
	 * left_valid is true then left DSC encoder is used, same for right.
	 */
	if (mdss_mdp_is_lm_swap_needed(mdata, ctl)) {
		right_valid = false;
		left_valid = true;
	}

	if ((is_dual_lm_single_display(ctl->mfd)) &&
	    (pinfo->partial_update_enabled) &&
	    (pinfo->dsc_enc_total == 2) && (dsc->full_frame_slices == 4) &&
	    (mdss_has_quirk(mdata, MDSS_QUIRK_DSC_2SLICE_PU_THRPUT))) {

		if (mdss_mdp_is_both_lm_valid(ctl)) {
			/* left + right */

			pr_debug("full line (4 slices) or middle 2 slice partial update\n");
			writel_relaxed(0x0,
				mdata->mdp_base + mdata->ppb[0].ctl_off);
			writel_relaxed(0x0,
				mdata->mdp_base + MDSS_MDP_REG_DCE_SEL);
		} else if (mixer_left->valid_roi || mixer_right->valid_roi) {
			/* left-only or right-only */

			u32 this_frame_slices =
				dsc->pic_width / dsc->slice_width;

			if (this_frame_slices == 2) {
				pr_debug("2 slice parital update, use merge\n");

				/* tandem + merge */
				mode = BIT(1) | BIT(0);

				right_valid = true;
				left_valid = true;

				writel_relaxed(0x2 << 4, mdata->mdp_base +
					mdata->ppb[0].ctl_off);
				writel_relaxed(BIT(0),
					mdata->mdp_base + MDSS_MDP_REG_DCE_SEL);
			} else {
				pr_debug("only one slice partial update\n");
				writel_relaxed(0x0,
					mdata->mdp_base +
					mdata->ppb[0].ctl_off);
				writel_relaxed(0x0,
					mdata->mdp_base + MDSS_MDP_REG_DCE_SEL);
			}
		}
	}

	if (left_valid) {
		mdss_mdp_ctl_dsc_config(mixer_left, dsc, mode,
			ich_reset_override);
		mdss_mdp_ctl_dsc_config_thresh(mixer_left, pinfo);
		mdss_mdp_ctl_dsc_enable(mixer_left);
	} else {
		mdss_mdp_ctl_dsc_disable(mixer_left);
	}

	if (mixer_right) {
		bool enable_right_dsc = right_valid;

		if (mux_3d && left_valid)
			enable_right_dsc = false;

		if (enable_right_dsc) {
			mdss_mdp_ctl_dsc_config(mixer_right, dsc, mode,
				ich_reset_override);
			mdss_mdp_ctl_dsc_config_thresh(mixer_right, pinfo);
			mdss_mdp_ctl_dsc_enable(mixer_right);
		} else {
			mdss_mdp_ctl_dsc_disable(mixer_right);
		}
	}
	pr_debug("mix%d: valid_roi=%d mix%d: valid_roi=%d mode=%d, pic_dim:%dx%d\n",
		mixer_left->num, left_valid,
		mixer_right ? mixer_right->num : -1, right_valid,
		mode, pic_width, pic_height);
	MDSS_XLOG(mixer_left->num, left_valid,
		mixer_right ? mixer_right->num : -1, right_valid,
		mode, pic_width, pic_height);
}

static int mdss_mdp_ctl_fbc_enable(int enable,
		struct mdss_mdp_mixer *mixer, struct mdss_panel_info *pdata)
{
	struct fbc_panel_info *fbc;
	u32 mode = 0, budget_ctl = 0, lossy_mode = 0, width;

	if (!pdata) {
		pr_err("Invalid pdata\n");
		return -EINVAL;
	}

	fbc = &pdata->fbc;

	if (mixer->num == MDSS_MDP_INTF_LAYERMIXER0 ||
			mixer->num == MDSS_MDP_INTF_LAYERMIXER1) {
		pr_debug("Mixer supports FBC.\n");
	} else {
		pr_debug("Mixer doesn't support FBC.\n");
		return -EINVAL;
	}

	if (enable) {
		if (fbc->enc_mode && pdata->bpp) {
			/* width is the compressed width */
			width = mult_frac(pdata->xres, fbc->target_bpp,
					pdata->bpp);
		} else {
			/* width is the source width */
			width = pdata->xres;
		}

		mode = ((width) << 16) | ((fbc->slice_height) << 11) |
			((fbc->pred_mode) << 10) | ((fbc->enc_mode) << 9) |
			((fbc->comp_mode) << 8) | ((fbc->qerr_enable) << 7) |
			((fbc->cd_bias) << 4) | ((fbc->pat_enable) << 3) |
			((fbc->vlc_enable) << 2) | ((fbc->bflc_enable) << 1) |
			enable;

		budget_ctl = ((fbc->line_x_budget) << 12) |
			((fbc->block_x_budget) << 8) | fbc->block_budget;

		lossy_mode = ((fbc->max_pred_err) << 28) |
			((fbc->lossless_mode_thd) << 16) |
			((fbc->lossy_mode_thd) << 8) |
			((fbc->lossy_rgb_thd) << 4) | fbc->lossy_mode_idx;
	}

	mdss_mdp_pingpong_write(mixer->pingpong_base,
		MDSS_MDP_REG_PP_FBC_MODE, mode);
	mdss_mdp_pingpong_write(mixer->pingpong_base,
		MDSS_MDP_REG_PP_FBC_BUDGET_CTL, budget_ctl);
	mdss_mdp_pingpong_write(mixer->pingpong_base,
		MDSS_MDP_REG_PP_FBC_LOSSY_MODE, lossy_mode);

	return 0;
}

int mdss_mdp_ctl_setup(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_ctl *split_ctl;
	u32 width, height;
	int split_fb;
	u32 max_mixer_width;
	struct mdss_panel_info *pinfo;

	if (!ctl || !ctl->panel_data) {
		pr_err("invalid ctl handle\n");
		return -ENODEV;
	}

	pinfo = &ctl->panel_data->panel_info;
	if (pinfo->type == WRITEBACK_PANEL) {
		pr_err("writeback panel, ignore\n");
		return 0;
	}

	split_ctl = mdss_mdp_get_split_ctl(ctl);

	width = get_panel_width(ctl);
	height = get_panel_yres(pinfo);

	max_mixer_width = ctl->mdata->max_mixer_width;

	split_fb = ((is_dual_lm_single_display(ctl->mfd)) &&
		    (ctl->mfd->split_fb_left <= max_mixer_width) &&
		    (ctl->mfd->split_fb_right <= max_mixer_width)) ? 1 : 0;
	pr_debug("max=%d xres=%d left=%d right=%d\n", max_mixer_width,
		 width, ctl->mfd->split_fb_left, ctl->mfd->split_fb_right);

	if ((split_ctl && (width > max_mixer_width)) ||
			(width > (2 * max_mixer_width))) {
		pr_err("Unsupported panel resolution: %dx%d\n", width, height);
		return -ENOTSUPP;
	}

	ctl->width = width;
	ctl->height = height;
	ctl->roi = (struct mdss_rect) {0, 0, width, height};

	if (!ctl->mixer_left) {
		ctl->mixer_left =
			mdss_mdp_mixer_alloc(ctl, MDSS_MDP_MIXER_TYPE_INTF,
			 ((width > max_mixer_width) || split_fb), 0);
		if (!ctl->mixer_left) {
			pr_err("unable to allocate layer mixer\n");
			return -ENOMEM;
		} else if (split_fb && ctl->mixer_left->num >= 1 &&
			(ctl->panel_data->panel_info.type == MIPI_CMD_PANEL)) {
			pr_err("use only DSPP0 and DSPP1 with cmd split\n");
			return -EPERM;
		}
	}

	if (split_fb) {
		width = ctl->mfd->split_fb_left;
		width += (pinfo->lcdc.border_left +
				pinfo->lcdc.border_right);
	} else if (width > max_mixer_width) {
		width /= 2;
	}

	ctl->mixer_left->width = width;
	ctl->mixer_left->height = height;
	ctl->mixer_left->roi = (struct mdss_rect) {0, 0, width, height};
	ctl->mixer_left->valid_roi = true;
	ctl->mixer_left->roi_changed = true;

	if (ctl->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY) {
		pr_debug("dual display detected\n");
		return 0;
	}

	if (split_fb)
		width = ctl->mfd->split_fb_right;

	if (width < ctl->width) {
		if (ctl->mixer_right == NULL) {
			ctl->mixer_right = mdss_mdp_mixer_alloc(ctl,
					MDSS_MDP_MIXER_TYPE_INTF, true, 0);
			if (!ctl->mixer_right) {
				pr_err("unable to allocate right mixer\n");
				if (ctl->mixer_left)
					mdss_mdp_mixer_free(ctl->mixer_left);
				return -ENOMEM;
			}
		}
		ctl->mixer_right->is_right_mixer = true;
		ctl->mixer_right->width = width;
		ctl->mixer_right->height = height;
		ctl->mixer_right->roi = (struct mdss_rect)
						{0, 0, width, height};
		ctl->mixer_right->valid_roi = true;
		ctl->mixer_right->roi_changed = true;
	} else if (ctl->mixer_right) {
		ctl->mixer_right->valid_roi = false;
		ctl->mixer_right->roi_changed = false;
		mdss_mdp_mixer_free(ctl->mixer_right);
		ctl->mixer_right = NULL;
	}

	if (ctl->mixer_right) {
		if (!is_dsc_compression(pinfo) ||
		    (pinfo->dsc_enc_total == 1))
			ctl->opmode |= MDSS_MDP_CTL_OP_PACK_3D_ENABLE |
				       MDSS_MDP_CTL_OP_PACK_3D_H_ROW_INT;
	} else {
		ctl->opmode &= ~(MDSS_MDP_CTL_OP_PACK_3D_ENABLE |
				  MDSS_MDP_CTL_OP_PACK_3D_H_ROW_INT);
	}

	return 0;
}

/**
 * mdss_mdp_ctl_reconfig() - re-configure ctl for new mode
 * @ctl: mdp controller.
 * @pdata: panel data
 *
 * This function is called when we are trying to dynamically change
 * the DSI mode. We need to change various mdp_ctl properties to
 * the new mode of operation.
 */
int mdss_mdp_ctl_reconfig(struct mdss_mdp_ctl *ctl,
		struct mdss_panel_data *pdata)
{
	void *tmp;
	int ret = 0;

	/*
	 * Switch first to prevent deleting important data in the case
	 * where panel type is not supported in reconfig
	 */
	if ((pdata->panel_info.type != MIPI_VIDEO_PANEL) &&
			(pdata->panel_info.type != MIPI_CMD_PANEL)) {
		pr_err("unsupported panel type (%d)\n", pdata->panel_info.type);
		return -EINVAL;
	}

	/* if only changing resolution there is no need for intf reconfig */
	if (!ctl->is_video_mode == (pdata->panel_info.type == MIPI_CMD_PANEL))
		goto skip_intf_reconfig;

	/*
	 * Intentionally not clearing stop function, as stop will
	 * be called after panel is instructed mode switch is happening
	 */
	tmp = ctl->ops.stop_fnc;
	memset(&ctl->ops, 0, sizeof(ctl->ops));
	ctl->ops.stop_fnc = tmp;

	switch (pdata->panel_info.type) {
	case MIPI_VIDEO_PANEL:
		ctl->is_video_mode = true;
		ctl->intf_type = MDSS_INTF_DSI;
		ctl->opmode = MDSS_MDP_CTL_OP_VIDEO_MODE;
		ctl->ops.start_fnc = mdss_mdp_video_start;
		break;
	case MIPI_CMD_PANEL:
		ctl->is_video_mode = false;
		ctl->intf_type = MDSS_INTF_DSI;
		ctl->opmode = MDSS_MDP_CTL_OP_CMD_MODE;
		ctl->ops.start_fnc = mdss_mdp_cmd_start;
		break;
	}

	ctl->is_secure = false;
	ctl->split_flush_en = false;
	ctl->perf_release_ctl_bw = false;
	ctl->play_cnt = 0;

	ctl->opmode |= (ctl->intf_num << 4);

skip_intf_reconfig:
	ctl->width = get_panel_xres(&pdata->panel_info);
	ctl->height = get_panel_yres(&pdata->panel_info);
	if (ctl->mixer_left) {
		ctl->mixer_left->width = ctl->width;
		ctl->mixer_left->height = ctl->height;
	}
	ctl->border_x_off = pdata->panel_info.lcdc.border_left;
	ctl->border_y_off = pdata->panel_info.lcdc.border_top;

	return ret;
}

struct mdss_mdp_ctl *mdss_mdp_ctl_init(struct mdss_panel_data *pdata,
				       struct msm_fb_data_type *mfd)
{
	int ret = 0, offset;
	struct mdss_mdp_ctl *ctl;
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_panel_info *pinfo;

	if (pdata->panel_info.type == WRITEBACK_PANEL)
		offset = mdss_mdp_get_wb_ctl_support(mdata, false);
	else
		offset = MDSS_MDP_CTL0;

	if (is_pingpong_split(mfd) && !mdata->has_pingpong_split) {
		pr_err("Error: pp_split cannot be enabled on fb%d if HW doesn't support it\n",
			mfd->index);
		return ERR_PTR(-EINVAL);
	}

	ctl = mdss_mdp_ctl_alloc(mdata, offset);
	if (!ctl) {
		pr_err("unable to allocate ctl\n");
		return ERR_PTR(-ENOMEM);
	}

	pinfo = &pdata->panel_info;
	ctl->mfd = mfd;
	ctl->panel_data = pdata;
	ctl->is_video_mode = false;
	ctl->perf_release_ctl_bw = false;
	ctl->border_x_off = pinfo->lcdc.border_left;
	ctl->border_y_off = pinfo->lcdc.border_top;
	ctl->disable_prefill = false;

	switch (pdata->panel_info.type) {
	case EDP_PANEL:
		ctl->is_video_mode = true;
		ctl->intf_num = MDSS_MDP_INTF0;
		ctl->intf_type = MDSS_INTF_EDP;
		ctl->opmode = MDSS_MDP_CTL_OP_VIDEO_MODE;
		ctl->ops.start_fnc = mdss_mdp_video_start;
		break;
	case MIPI_VIDEO_PANEL:
		ctl->is_video_mode = true;
		if (pdata->panel_info.pdest == DISPLAY_1)
			ctl->intf_num = mdp5_data->mixer_swap ? MDSS_MDP_INTF2 :
				MDSS_MDP_INTF1;
		else
			ctl->intf_num = mdp5_data->mixer_swap ? MDSS_MDP_INTF1 :
				MDSS_MDP_INTF2;
		ctl->intf_type = MDSS_INTF_DSI;
		ctl->opmode = MDSS_MDP_CTL_OP_VIDEO_MODE;
		ctl->ops.start_fnc = mdss_mdp_video_start;
		break;
	case MIPI_CMD_PANEL:
		if (pdata->panel_info.pdest == DISPLAY_1)
			ctl->intf_num = mdp5_data->mixer_swap ? MDSS_MDP_INTF2 :
				MDSS_MDP_INTF1;
		else
			ctl->intf_num = mdp5_data->mixer_swap ? MDSS_MDP_INTF1 :
				MDSS_MDP_INTF2;
		ctl->intf_type = MDSS_INTF_DSI;
		ctl->opmode = MDSS_MDP_CTL_OP_CMD_MODE;
		ctl->ops.start_fnc = mdss_mdp_cmd_start;
		break;
	case DTV_PANEL:
		ctl->is_video_mode = true;
		ctl->intf_num = MDSS_MDP_INTF3;
		ctl->intf_type = MDSS_INTF_HDMI;
		ctl->opmode = MDSS_MDP_CTL_OP_VIDEO_MODE;
		ctl->ops.start_fnc = mdss_mdp_video_start;
		break;
	case WRITEBACK_PANEL:
		ctl->intf_num = MDSS_MDP_NO_INTF;
		ctl->ops.start_fnc = mdss_mdp_writeback_start;
		break;
	default:
		pr_err("unsupported panel type (%d)\n", pdata->panel_info.type);
		ret = -EINVAL;
		goto ctl_init_fail;
	}

	ctl->opmode |= (ctl->intf_num << 4);

	if (ctl->intf_num == MDSS_MDP_NO_INTF) {
		ctl->dst_format = pdata->panel_info.out_format;
	} else {
		switch (pdata->panel_info.bpp) {
		case 18:
			if (ctl->intf_type == MDSS_INTF_DSI)
				ctl->dst_format = MDSS_MDP_PANEL_FORMAT_RGB666 |
					MDSS_MDP_PANEL_FORMAT_PACK_ALIGN_MSB;
			else
				ctl->dst_format = MDSS_MDP_PANEL_FORMAT_RGB666;
			break;
		case 24:
		default:
			ctl->dst_format = MDSS_MDP_PANEL_FORMAT_RGB888;
			break;
		}
	}

	return ctl;
ctl_init_fail:
	mdss_mdp_ctl_free(ctl);

	return ERR_PTR(ret);
}

int mdss_mdp_ctl_split_display_setup(struct mdss_mdp_ctl *ctl,
		struct mdss_panel_data *pdata)
{
	struct mdss_mdp_ctl *sctl;
	struct mdss_mdp_mixer *mixer;

	if (!ctl || !pdata)
		return -ENODEV;

	if (pdata->panel_info.xres > ctl->mdata->max_mixer_width) {
		pr_err("Unsupported second panel resolution: %dx%d\n",
				pdata->panel_info.xres, pdata->panel_info.yres);
		return -ENOTSUPP;
	}

	if (ctl->mixer_right) {
		pr_err("right mixer already setup for ctl=%d\n", ctl->num);
		return -EPERM;
	}

	sctl = mdss_mdp_ctl_init(pdata, ctl->mfd);
	if (!sctl) {
		pr_err("unable to setup split display\n");
		return -ENODEV;
	}

	sctl->width = get_panel_xres(&pdata->panel_info);
	sctl->height = get_panel_yres(&pdata->panel_info);

	sctl->roi = (struct mdss_rect){0, 0, sctl->width, sctl->height};

	if (!ctl->mixer_left) {
		ctl->mixer_left = mdss_mdp_mixer_alloc(ctl,
				MDSS_MDP_MIXER_TYPE_INTF,
				false, 0);
		if (!ctl->mixer_left) {
			pr_err("unable to allocate layer mixer\n");
			mdss_mdp_ctl_destroy(sctl);
			return -ENOMEM;
		}
	}

	mixer = mdss_mdp_mixer_alloc(sctl, MDSS_MDP_MIXER_TYPE_INTF, false, 0);
	if (!mixer) {
		pr_err("unable to allocate layer mixer\n");
		mdss_mdp_ctl_destroy(sctl);
		return -ENOMEM;
	}

	mixer->is_right_mixer = true;
	mixer->width = sctl->width;
	mixer->height = sctl->height;
	mixer->roi = (struct mdss_rect)
				{0, 0, mixer->width, mixer->height};
	mixer->valid_roi = true;
	mixer->roi_changed = true;
	sctl->mixer_left = mixer;

	return mdss_mdp_set_split_ctl(ctl, sctl);
}

static void mdss_mdp_ctl_split_display_enable(int enable,
	struct mdss_mdp_ctl *main_ctl, struct mdss_mdp_ctl *slave_ctl)
{
	u32 upper = 0, lower = 0;

	pr_debug("split main ctl=%d intf=%d\n",
			main_ctl->num, main_ctl->intf_num);

	if (slave_ctl)
		pr_debug("split slave ctl=%d intf=%d\n",
			slave_ctl->num, slave_ctl->intf_num);

	if (enable) {
		if (main_ctl->opmode & MDSS_MDP_CTL_OP_CMD_MODE) {
			/* interface controlling sw trigger (cmd mode) */
			lower |= BIT(1);
			if (main_ctl->intf_num == MDSS_MDP_INTF2)
				lower |= BIT(4);
			else
				lower |= BIT(8);
			/*
			 * Enable SMART_PANEL_FREE_RUN if ping pong split
			 * is enabled.
			 */
			if (is_pingpong_split(main_ctl->mfd))
				lower |= BIT(2);
			upper = lower;
		} else {
			/* interface controlling sw trigger (video mode) */
			if (main_ctl->intf_num == MDSS_MDP_INTF2) {
				lower |= BIT(4);
				upper |= BIT(8);
			} else {
				lower |= BIT(8);
				upper |= BIT(4);
			}
		}
	}
	writel_relaxed(upper, main_ctl->mdata->mdp_base +
		MDSS_MDP_REG_SPLIT_DISPLAY_UPPER_PIPE_CTRL);
	writel_relaxed(lower, main_ctl->mdata->mdp_base +
		MDSS_MDP_REG_SPLIT_DISPLAY_LOWER_PIPE_CTRL);
	writel_relaxed(enable, main_ctl->mdata->mdp_base +
		MDSS_MDP_REG_SPLIT_DISPLAY_EN);

	if ((main_ctl->mdata->mdp_rev >= MDSS_MDP_HW_REV_103)
		&& main_ctl->is_video_mode) {
		struct mdss_overlay_private *mdp5_data;
		bool mixer_swap = false;

		if (main_ctl->mfd) {
			mdp5_data = mfd_to_mdp5_data(main_ctl->mfd);
			mixer_swap = mdp5_data->mixer_swap;
		}

		main_ctl->split_flush_en = !mixer_swap;
		if (main_ctl->split_flush_en)
			writel_relaxed(enable ? 0x1 : 0x0,
				main_ctl->mdata->mdp_base +
				MMSS_MDP_MDP_SSPP_SPARE_0);
	}
}

static void mdss_mdp_ctl_pp_split_display_enable(bool enable,
		struct mdss_mdp_ctl *ctl)
{
	u32 cfg = 0, cntl = 0;

	if (ctl->mdata->nppb == 0) {
		pr_err("No PPB to enable PP split\n");
		BUG();
	}

	mdss_mdp_ctl_split_display_enable(enable, ctl, NULL);

	if (enable) {
		cfg = ctl->slave_intf_num << 20; /* Set slave intf */
		cfg |= BIT(16);			 /* Set horizontal split */
		cntl = BIT(5);			 /* enable dst split */
	}

	writel_relaxed(cfg, ctl->mdata->mdp_base + ctl->mdata->ppb[0].cfg_off);
	writel_relaxed(cntl, ctl->mdata->mdp_base + ctl->mdata->ppb[0].ctl_off);
}

int mdss_mdp_ctl_destroy(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_ctl *sctl;
	int rc;

	rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_CLOSE, NULL,
				     CTL_INTF_EVENT_FLAG_DEFAULT);
	WARN(rc, "unable to close panel for intf=%d\n", ctl->intf_num);

	sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl) {
		pr_debug("destroying split display ctl=%d\n", sctl->num);
		mdss_mdp_ctl_free(sctl);
	}

	mdss_mdp_ctl_free(ctl);

	return 0;
}

int mdss_mdp_ctl_intf_event(struct mdss_mdp_ctl *ctl, int event, void *arg,
	u32 flags)
{
	struct mdss_panel_data *pdata;
	int rc = 0;

	if (!ctl || !ctl->panel_data)
		return -ENODEV;

	pdata = ctl->panel_data;

	if (flags & CTL_INTF_EVENT_FLAG_SLAVE_INTF) {
		pdata = pdata->next;
		if (!pdata) {
			pr_err("Error: event=%d flags=0x%x, ctl%d slave intf is not present\n",
				event, flags, ctl->num);
			return -EINVAL;
		}
	}

	pr_debug("sending ctl=%d event=%d flag=0x%x\n", ctl->num, event, flags);

	do {
		if (pdata->event_handler)
			rc = pdata->event_handler(pdata, event, arg);
		pdata = pdata->next;
	} while (rc == 0 && pdata && pdata->active &&
		!(flags & CTL_INTF_EVENT_FLAG_SKIP_BROADCAST));

	return rc;
}

static void mdss_mdp_ctl_restore_sub(struct mdss_mdp_ctl *ctl)
{
	u32 temp;
	int ret = 0;

	temp = readl_relaxed(ctl->mdata->mdp_base +
			MDSS_MDP_REG_DISP_INTF_SEL);
	temp |= (ctl->intf_type << ((ctl->intf_num - MDSS_MDP_INTF0) * 8));
	writel_relaxed(temp, ctl->mdata->mdp_base +
			MDSS_MDP_REG_DISP_INTF_SEL);

	if (ctl->mfd && ctl->panel_data) {
		mdss_mdp_pp_resume(ctl->mfd);

		if (ctl->panel_data->panel_info.compression_mode ==
				COMPRESSION_DSC) {
			mdss_mdp_ctl_dsc_setup(ctl,
					&ctl->panel_data->panel_info);
		} else if (ctl->panel_data->panel_info.compression_mode ==
				COMPRESSION_FBC) {
			ret = mdss_mdp_ctl_fbc_enable(1, ctl->mixer_left,
					&ctl->panel_data->panel_info);
			if (ret)
				pr_err("Failed to restore FBC mode\n");
		}
	}
}

/*
 * mdss_mdp_ctl_restore() - restore mdp ctl path
 * @locked - boolean to signal that clock lock is already acquired
 *
 * This function is called whenever MDP comes out of a power collapse as
 * a result of a screen update. It restores the MDP controller's software
 * state to the hardware registers.
 * Function does not enable the clocks, so caller must make sure
 * clocks are enabled before calling.
 * The locked boolean in the parametrs signals that synchronization
 * with mdp clocks access is not required downstream.
 * Only call this function setting this value to true if the clocks access
 * synchronization is guaranteed by the caller.
 */
void mdss_mdp_ctl_restore(bool locked)
{
	struct mdss_mdp_ctl *ctl = NULL;
	struct mdss_mdp_ctl *sctl;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 cnum;

	for (cnum = MDSS_MDP_CTL0; cnum < mdata->nctl; cnum++) {
		ctl = mdata->ctl_off + cnum;
		if (!mdss_mdp_ctl_is_power_on(ctl))
			continue;

		pr_debug("restoring ctl%d, intf_type=%d\n", cnum,
			ctl->intf_type);
		ctl->play_cnt = 0;
		sctl = mdss_mdp_get_split_ctl(ctl);
		mdss_mdp_ctl_restore_sub(ctl);
		if (sctl) {
			mdss_mdp_ctl_restore_sub(sctl);
			mdss_mdp_ctl_split_display_enable(1, ctl, sctl);
		}
		if (ctl->ops.restore_fnc)
			ctl->ops.restore_fnc(ctl, locked);
	}
}

static int mdss_mdp_ctl_start_sub(struct mdss_mdp_ctl *ctl, bool handoff)
{
	struct mdss_mdp_mixer *mixer;
	u32 outsize, temp;
	int ret = 0;
	int i, nmixers;

	pr_debug("ctl_num=%d\n", ctl->num);

	/*
	 * Need start_fnc in 2 cases:
	 * (1) handoff
	 * (2) continuous splash finished.
	 */
	if (handoff || !ctl->panel_data->panel_info.cont_splash_enabled) {
		if (ctl->ops.start_fnc)
			ret = ctl->ops.start_fnc(ctl);
		else
			pr_warn("no start function for ctl=%d type=%d\n",
					ctl->num,
					ctl->panel_data->panel_info.type);

		if (ret) {
			pr_err("unable to start intf\n");
			return ret;
		}
	}

	if (!ctl->panel_data->panel_info.cont_splash_enabled) {
		nmixers = MDSS_MDP_INTF_MAX_LAYERMIXER +
			MDSS_MDP_WB_MAX_LAYERMIXER;
		for (i = 0; i < nmixers; i++)
			mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_LAYER(i), 0);
	}

	temp = readl_relaxed(ctl->mdata->mdp_base +
		MDSS_MDP_REG_DISP_INTF_SEL);
	temp |= (ctl->intf_type << ((ctl->intf_num - MDSS_MDP_INTF0) * 8));
	if (is_pingpong_split(ctl->mfd))
		temp |= (ctl->intf_type << (ctl->intf_num * 8));

	writel_relaxed(temp, ctl->mdata->mdp_base +
		MDSS_MDP_REG_DISP_INTF_SEL);

	mixer = ctl->mixer_left;
	if (mixer) {
		struct mdss_panel_info *pinfo = &ctl->panel_data->panel_info;

		mixer->params_changed++;

		outsize = (mixer->height << 16) | mixer->width;
		mdp_mixer_write(mixer, MDSS_MDP_REG_LM_OUT_SIZE, outsize);

		if (pinfo->compression_mode == COMPRESSION_DSC) {
			mdss_mdp_ctl_dsc_setup(ctl, pinfo);
		} else if (pinfo->compression_mode == COMPRESSION_FBC) {
			ret = mdss_mdp_ctl_fbc_enable(1, ctl->mixer_left,
					pinfo);
		}
	}
	return ret;
}

int mdss_mdp_ctl_start(struct mdss_mdp_ctl *ctl, bool handoff)
{
	struct mdss_mdp_ctl *sctl;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	int ret = 0;

	pr_debug("ctl_num=%d, power_state=%d\n", ctl->num, ctl->power_state);

	if (mdss_mdp_ctl_is_power_on_interactive(ctl)
			&& !(ctl->pending_mode_switch)) {
		pr_debug("%d: panel already on!\n", __LINE__);
		return 0;
	}

	ret = mdss_mdp_ctl_setup(ctl);
	if (ret)
		return ret;

	sctl = mdss_mdp_get_split_ctl(ctl);

	mutex_lock(&ctl->lock);

	if (mdss_mdp_ctl_is_power_off(ctl))
		memset(&ctl->cur_perf, 0, sizeof(ctl->cur_perf));

	/*
	 * keep power_on false during handoff to avoid unexpected
	 * operations to overlay.
	 */
	if (!handoff || ctl->pending_mode_switch)
		ctl->power_state = MDSS_PANEL_POWER_ON;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

	ret = mdss_mdp_ctl_start_sub(ctl, handoff);
	if (ret == 0) {
		if (sctl && ctl->mfd &&
		    ctl->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY) {
			/*split display available */
			ret = mdss_mdp_ctl_start_sub(sctl, handoff);
			if (!ret)
				mdss_mdp_ctl_split_display_enable(1, ctl, sctl);
		} else if (ctl->mixer_right) {
			struct mdss_mdp_mixer *mixer = ctl->mixer_right;
			u32 out;

			mixer->params_changed++;
			out = (mixer->height << 16) | mixer->width;
			mdp_mixer_write(mixer, MDSS_MDP_REG_LM_OUT_SIZE, out);
			mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_PACK_3D, 0);
		} else if (is_pingpong_split(ctl->mfd)) {
			ctl->slave_intf_num = (ctl->intf_num + 1);
			mdss_mdp_ctl_pp_split_display_enable(true, ctl);
		}
	}

	mdss_mdp_hist_intr_setup(&mdata->hist_intr, MDSS_IRQ_RESUME);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
	mutex_unlock(&ctl->lock);

	return ret;
}

int mdss_mdp_ctl_stop(struct mdss_mdp_ctl *ctl, int power_state)
{
	struct mdss_mdp_ctl *sctl;
	int ret = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 off;

	pr_debug("ctl_num=%d, power_state=%d\n", ctl->num, ctl->power_state);

	if (!mdss_mdp_ctl_is_power_on(ctl)) {
		pr_debug("%s %d already off!\n", __func__, __LINE__);
		return 0;
	}

	sctl = mdss_mdp_get_split_ctl(ctl);

	mutex_lock(&ctl->lock);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

	mdss_mdp_hist_intr_setup(&mdata->hist_intr, MDSS_IRQ_SUSPEND);

	if (ctl->ops.stop_fnc) {
		ret = ctl->ops.stop_fnc(ctl, power_state);
		mdss_mdp_ctl_fbc_enable(0, ctl->mixer_left,
				&ctl->panel_data->panel_info);
	} else {
		pr_warn("no stop func for ctl=%d\n", ctl->num);
	}

	if (sctl && sctl->ops.stop_fnc) {
		ret = sctl->ops.stop_fnc(sctl, power_state);
		mdss_mdp_ctl_fbc_enable(0, sctl->mixer_left,
				&sctl->panel_data->panel_info);
	}
	if (ret) {
		pr_warn("error powering off intf ctl=%d\n", ctl->num);
		goto end;
	}

	if (mdss_panel_is_power_on(power_state)) {
		pr_debug("panel is not off, leaving ctl power on\n");
		goto end;
	}

	if (sctl)
		mdss_mdp_ctl_split_display_enable(0, ctl, sctl);

	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_TOP, 0);
	if (sctl)
		mdss_mdp_ctl_write(sctl, MDSS_MDP_REG_CTL_TOP, 0);

	if (ctl->mixer_left) {
		off = __mdss_mdp_ctl_get_mixer_off(ctl->mixer_left);
		mdss_mdp_ctl_write(ctl, off, 0);
	}

	if (ctl->mixer_right) {
		off = __mdss_mdp_ctl_get_mixer_off(ctl->mixer_right);
		mdss_mdp_ctl_write(ctl, off, 0);
	}

	ctl->play_cnt = 0;

end:
	if (!ret) {
		ctl->power_state = power_state;
		if (!ctl->pending_mode_switch)
			mdss_mdp_ctl_perf_update(ctl, 0, true);
	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	mutex_unlock(&ctl->lock);

	return ret;
}

/*
 * mdss_mdp_pipe_reset() - Halts all the pipes during ctl reset.
 * @mixer: Mixer from which to reset all pipes.
 * This function called during control path reset and will halt
 * all the pipes staged on the mixer.
 */
static void mdss_mdp_pipe_reset(struct mdss_mdp_mixer *mixer, bool is_recovery)
{
	unsigned long pipe_map = mixer->pipe_mapped;
	u32 bit = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool sw_rst_avail = mdss_mdp_pipe_is_sw_reset_available(mdata);

	pr_debug("pipe_map=0x%lx\n", pipe_map);
	for_each_set_bit_from(bit, &pipe_map, MAX_PIPES_PER_LM) {
		struct mdss_mdp_pipe *pipe;

		pipe = mdss_mdp_pipe_search(mdata, 1 << bit);
		if (pipe) {
			mdss_mdp_pipe_fetch_halt(pipe, is_recovery);
			if (sw_rst_avail)
				mdss_mdp_pipe_clk_force_off(pipe);
		}
	}
}

/*
 * mdss_mdp_ctl_reset() - reset mdp ctl path.
 * @ctl: mdp controller.
 * this function called when underflow happen,
 * it will reset mdp ctl path and poll for its completion
 *
 * Note: called within atomic context.
 */
int mdss_mdp_ctl_reset(struct mdss_mdp_ctl *ctl, bool is_recovery)
{
	u32 status = 1;
	int cnt = 20;
	struct mdss_mdp_mixer *mixer = ctl->mixer_left;

	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_SW_RESET, 1);

	/*
	 * it takes around 30us to have mdp finish resetting its ctl path
	 * poll every 50us so that reset should be completed at 1st poll
	 */
	do {
		udelay(50);
		status = mdss_mdp_ctl_read(ctl, MDSS_MDP_REG_CTL_SW_RESET);
		status &= 0x01;
		pr_debug("status=%x\n", status);
		cnt--;
	} while (cnt > 0 && status);

	if (mixer) {
		mdss_mdp_pipe_reset(mixer, is_recovery);

		if (is_dual_lm_single_display(ctl->mfd))
			mdss_mdp_pipe_reset(ctl->mixer_right, is_recovery);
	}

	if (!cnt)
		pr_err("ctl%d reset timedout\n", ctl->num);

	return (!cnt) ? -EAGAIN : 0;
}

/*
 * mdss_mdp_mixer_update_pipe_map() - keep track of pipe configuration in  mixer
 * @master_ctl: mdp controller.
 *
 * This function keeps track of the current mixer configuration in the hardware.
 * It's callers responsibility to call with master control.
 */
static void mdss_mdp_mixer_update_pipe_map(struct mdss_mdp_ctl *master_ctl,
		       int mixer_mux)
{
	struct mdss_mdp_mixer *mixer = mdss_mdp_mixer_get(master_ctl,
			mixer_mux);

	if (!mixer)
		return;

	pr_debug("mixer%d pipe_mapped=0x%x next_pipes=0x%x\n",
		mixer->num, mixer->pipe_mapped, mixer->next_pipe_map);

	mixer->pipe_mapped = mixer->next_pipe_map;
}

static void mdss_mdp_set_mixer_roi(struct mdss_mdp_mixer *mixer,
	struct mdss_rect *roi)
{
	mixer->valid_roi = (roi->w && roi->h);
	mixer->roi_changed = false;

	if (!mdss_rect_cmp(roi, &mixer->roi)) {
		mixer->roi = *roi;
		mixer->params_changed++;
		mixer->roi_changed = true;
	}

	pr_debug("mixer%d ROI %s: [%d, %d, %d, %d]\n",
		mixer->num, mixer->roi_changed ? "changed" : "not changed",
		mixer->roi.x, mixer->roi.y, mixer->roi.w, mixer->roi.h);
	MDSS_XLOG(mixer->num, mixer->roi_changed, mixer->valid_roi,
		mixer->roi.x, mixer->roi.y, mixer->roi.w, mixer->roi.h);
}

/* only call from master ctl */
void mdss_mdp_set_roi(struct mdss_mdp_ctl *ctl,
	struct mdss_rect *l_roi, struct mdss_rect *r_roi)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	/* Reset ROI when we have (1) invalid ROI (2) feature disabled */
	if ((!l_roi->w && l_roi->h) || (l_roi->w && !l_roi->h) ||
	    (!r_roi->w && r_roi->h) || (r_roi->w && !r_roi->h) ||
	    (!l_roi->w && !l_roi->h && !r_roi->w && !r_roi->h) ||
	    !ctl->panel_data->panel_info.partial_update_enabled) {

		*l_roi = (struct mdss_rect) {0, 0,
				ctl->mixer_left->width,
				ctl->mixer_left->height};

		if (ctl->mixer_right) {
			*r_roi = (struct mdss_rect) {0, 0,
					ctl->mixer_right->width,
					ctl->mixer_right->height};
		}
	}

	mdss_mdp_set_mixer_roi(ctl->mixer_left, l_roi);
	ctl->roi = ctl->mixer_left->roi;

	if (ctl->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY) {
		struct mdss_mdp_ctl *sctl = mdss_mdp_get_split_ctl(ctl);

		mdss_mdp_set_mixer_roi(sctl->mixer_left, r_roi);
		sctl->roi = sctl->mixer_left->roi;
	} else if (is_dual_lm_single_display(ctl->mfd)) {

		mdss_mdp_set_mixer_roi(ctl->mixer_right, r_roi);

		/* in this case, CTL_ROI is a union of left+right ROIs. */
		ctl->roi.w += ctl->mixer_right->roi.w;

		/* right_only, update roi.x as per CTL ROI guidelines */
		if (!ctl->mixer_left->valid_roi) {
			ctl->roi = ctl->mixer_right->roi;
			ctl->roi.x = left_lm_w_from_mfd(ctl->mfd) +
				ctl->mixer_right->roi.x;
		}
	}

	/*
	 * When source split is enabled, dst_x of all pipes staged on right
	 * layer-mixer (LM) has to be greater than left LM's width.
	 * Now when partial update is enabled, let's say frame N is updating
	 * full panel width so both LMs are valid thus source split is enabled
	 * and dst_x of all pipes is as per above requirement. Now when frame
	 * N+1 is right-only update without any geomatry changes, pipe's params
	 * are not changed and right LM's roi is also not changed. So if
	 * params are not changed then pipe's register programming is skipped.
	 * So for that right-only update, pipe's dst_x remains same
	 * as frame N, which not correct and can lead to unknown behaviour.
	 * Fix this by identifying this condition and forcing roi changed for
	 * right LM.
	 */
	if (is_split_lm(ctl->mfd) && mdata->has_src_split &&
	    (!ctl->mixer_left->valid_roi && ctl->mixer_left->roi_changed) &&
	    (ctl->mixer_right->valid_roi && !ctl->mixer_right->roi_changed))
		ctl->mixer_right->roi_changed = true;
}

u32 mdss_mdp_get_mixer_mask(u32 pipe_num, u32 stage)
{
	u32 mask = 0;

	if ((pipe_num == MDSS_MDP_SSPP_VIG3 ||
			pipe_num == MDSS_MDP_SSPP_RGB3)) {
		/* Add 2 to account for Cursor & Border bits */
		mask = stage << ((3 * pipe_num) + 2);
	} else {
		mask = stage << (3 * pipe_num);
	}
	return mask;
}

u32 mdss_mdp_get_mixer_extn_mask(u32 pipe_num, u32 stage)
{
	u32 mask = 0;

	/*
	 * The ctl layer extension bits are ordered
	 * VIG0-3, RGB0-3, DMA0-1
	 */
	if (pipe_num < MDSS_MDP_SSPP_RGB0) {
		mask = BIT(pipe_num << 1);
	} else if (pipe_num >= MDSS_MDP_SSPP_RGB0 &&
			pipe_num < MDSS_MDP_SSPP_DMA0) {
		mask = BIT((pipe_num + 1) << 1);
	} else if (pipe_num >= MDSS_MDP_SSPP_DMA0 &&
			pipe_num < MDSS_MDP_SSPP_VIG3) {
		mask = BIT((pipe_num + 2) << 1);
	} else if (pipe_num >= MDSS_MDP_SSPP_CURSOR0 &&
			pipe_num <= MDSS_MDP_SSPP_CURSOR1) {
		mask = stage << (20 + (6 * (pipe_num - MDSS_MDP_SSPP_CURSOR0)));
	} else if (pipe_num == MDSS_MDP_SSPP_VIG3) {
		mask = BIT(6);
	} else if (pipe_num == MDSS_MDP_SSPP_RGB3) {
		mask = BIT(14);
	}

	return mask;
}

static void mdss_mdp_mixer_setup(struct mdss_mdp_ctl *master_ctl,
	int mixer_mux, bool lm_swap)
{
	int i;
	int stage, screen_state, outsize;
	u32 off, blend_op, blend_stage;
	u32 mixercfg = 0, mixer_op_mode = 0, bg_alpha_enable = 0,
	    mixercfg_extn = 0;
	u32 fg_alpha = 0, bg_alpha = 0;
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_ctl *ctl = NULL;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_mixer *mixer_hw = mdss_mdp_mixer_get(master_ctl,
		mixer_mux);
	struct mdss_mdp_mixer *mixer;

	if (!mixer_hw)
		return;

	ctl = mixer_hw->ctl;
	if (!ctl)
		return;

	mixer_hw->params_changed = 0;
	/* check if mixer setup for rotator is needed */
	if (mixer_hw->rotator_mode) {
		int nmixers = mdata->nmixers_intf + mdata->nmixers_wb;
		for (i = 0; i < nmixers; i++)
			mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_LAYER(i), 0);
		return;
	}

	if (lm_swap) {
		if (mixer_mux == MDSS_MDP_MIXER_MUX_RIGHT)
			mixer = mdss_mdp_mixer_get(master_ctl,
				MDSS_MDP_MIXER_MUX_LEFT);
		else
			mixer = mdss_mdp_mixer_get(master_ctl,
				MDSS_MDP_MIXER_MUX_RIGHT);
	} else {
		mixer = mixer_hw;
	}

	if (!mixer->valid_roi) {
		/*
		 * resetting mixer config is specifically needed when split
		 * mode is MDP_DUAL_LM_SINGLE_DISPLAY but update is only on
		 * one side.
		 */
		off = __mdss_mdp_ctl_get_mixer_off(mixer_hw);
		mdss_mdp_ctl_write(ctl, off, 0);
		/* Program ctl layer extension bits */
		mdss_mdp_ctl_write(ctl,
			off + MDSS_MDP_REG_CTL_LAYER_EXTN_OFFSET, 0);

		MDSS_XLOG(mixer->num, mixer_hw->num, XLOG_FUNC_EXIT);
		return;
	}

	trace_mdp_mixer_update(mixer_hw->num);
	pr_debug("setup mixer=%d hw=%d\n", mixer->num, mixer_hw->num);
	screen_state = ctl->force_screen_state;

	outsize = (mixer->roi.h << 16) | mixer->roi.w;
	mdp_mixer_write(mixer_hw, MDSS_MDP_REG_LM_OUT_SIZE, outsize);

	if (screen_state == MDSS_SCREEN_FORCE_BLANK) {
		mixercfg = MDSS_MDP_LM_BORDER_COLOR;
		goto update_mixer;
	}

	pipe = mixer->stage_pipe[MDSS_MDP_STAGE_BASE * MAX_PIPES_PER_STAGE];
	if (pipe == NULL) {
		mixercfg = MDSS_MDP_LM_BORDER_COLOR;
	} else {
		if (pipe->type == MDSS_MDP_PIPE_TYPE_CURSOR)
			mixercfg_extn |= mdss_mdp_get_mixer_extn_mask(
						pipe->num, 1);
		else
			mixercfg  |= mdss_mdp_get_mixer_mask(pipe->num, 1);

		if (pipe->src_fmt->alpha_enable)
			bg_alpha_enable = 1;
	}

	i = MDSS_MDP_STAGE_0 * MAX_PIPES_PER_STAGE;
	for (; i < MAX_PIPES_PER_LM; i++) {
		pipe = mixer->stage_pipe[i];
		if (pipe == NULL)
			continue;

		stage = i / MAX_PIPES_PER_STAGE;
		if (stage != pipe->mixer_stage) {
			pr_err("pipe%d mixer:%d mixer:%d stage mismatch. pipe->mixer_stage=%d, mixer->stage_pipe=%d. skip staging it\n",
			    pipe->num, mixer->num, mixer->num,
			    pipe->mixer_stage, stage);
			mixer->stage_pipe[i] = NULL;
			continue;
		}

		/*
		 * pipe which is staged on both LMs will be tracked through
		 * left mixer only.
		 */
		if (!pipe->src_split_req || !mixer->is_right_mixer)
			mixer->next_pipe_map |= pipe->ndx;

		blend_stage = stage - MDSS_MDP_STAGE_0;
		off = MDSS_MDP_REG_LM_BLEND_OFFSET(blend_stage);

		/*
		 * Account for additional blending stages
		 * from MDP v1.5 onwards
		 */
		if (blend_stage > 3)
			off += MDSS_MDP_REG_LM_BLEND_STAGE4;
		blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_CONST |
			    MDSS_MDP_BLEND_BG_ALPHA_BG_CONST);
		fg_alpha = pipe->alpha;
		bg_alpha = 0xFF - pipe->alpha;
		/* keep fg alpha */
		mixer_op_mode |= 1 << (blend_stage + 1);

		switch (pipe->blend_op) {
		case BLEND_OP_OPAQUE:

			blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_CONST |
				    MDSS_MDP_BLEND_BG_ALPHA_BG_CONST);

			pr_debug("pnum=%d stg=%d op=OPAQUE\n", pipe->num,
					stage);
			break;

		case BLEND_OP_PREMULTIPLIED:
			if (pipe->src_fmt->alpha_enable) {
				blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_CONST |
					    MDSS_MDP_BLEND_BG_ALPHA_FG_PIXEL);
				if (fg_alpha != 0xff) {
					bg_alpha = fg_alpha;
					blend_op |=
						MDSS_MDP_BLEND_BG_MOD_ALPHA |
						MDSS_MDP_BLEND_BG_INV_MOD_ALPHA;
				} else {
					blend_op |= MDSS_MDP_BLEND_BG_INV_ALPHA;
				}
			}
			pr_debug("pnum=%d stg=%d op=PREMULTIPLIED\n", pipe->num,
					stage);
			break;

		case BLEND_OP_COVERAGE:
			if (pipe->src_fmt->alpha_enable) {
				blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_PIXEL |
					    MDSS_MDP_BLEND_BG_ALPHA_FG_PIXEL);
				if (fg_alpha != 0xff) {
					bg_alpha = fg_alpha;
					blend_op |=
					       MDSS_MDP_BLEND_FG_MOD_ALPHA |
					       MDSS_MDP_BLEND_FG_INV_MOD_ALPHA |
					       MDSS_MDP_BLEND_BG_MOD_ALPHA |
					       MDSS_MDP_BLEND_BG_INV_MOD_ALPHA;
				} else {
					blend_op |= MDSS_MDP_BLEND_BG_INV_ALPHA;
				}
			}
			pr_debug("pnum=%d stg=%d op=COVERAGE\n", pipe->num,
					stage);
			break;

		default:
			blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_CONST |
				    MDSS_MDP_BLEND_BG_ALPHA_BG_CONST);
			pr_debug("pnum=%d stg=%d op=NONE\n", pipe->num,
					stage);
			break;
		}

		if (!pipe->src_fmt->alpha_enable && bg_alpha_enable)
			mixer_op_mode = 0;

		if ((stage < MDSS_MDP_STAGE_6) &&
			(pipe->type != MDSS_MDP_PIPE_TYPE_CURSOR))
			mixercfg |= mdss_mdp_get_mixer_mask(pipe->num, stage);
		else
			mixercfg_extn |= mdss_mdp_get_mixer_extn_mask(
						pipe->num, stage);

		trace_mdp_sspp_change(pipe);

		pr_debug("stg=%d op=%x fg_alpha=%x bg_alpha=%x\n", stage,
					blend_op, fg_alpha, bg_alpha);
		mdp_mixer_write(mixer_hw,
			off + MDSS_MDP_REG_LM_OP_MODE, blend_op);
		mdp_mixer_write(mixer_hw,
			off + MDSS_MDP_REG_LM_BLEND_FG_ALPHA, fg_alpha);
		mdp_mixer_write(mixer_hw,
			off + MDSS_MDP_REG_LM_BLEND_BG_ALPHA, bg_alpha);
	}

	if (mixer->cursor_enabled)
		mixercfg |= MDSS_MDP_LM_CURSOR_OUT;

update_mixer:
	if (mixer_hw->num == MDSS_MDP_INTF_LAYERMIXER3)
		ctl->flush_bits |= BIT(20);
	else if (mixer_hw->type == MDSS_MDP_MIXER_TYPE_WRITEBACK)
		ctl->flush_bits |= BIT(9) << mixer_hw->num;
	else
		ctl->flush_bits |= BIT(6) << mixer_hw->num;

	/* Read GC enable/disable status on LM */
	mixer_op_mode |=
		(mdp_mixer_read(mixer_hw, MDSS_MDP_REG_LM_OP_MODE) & BIT(0));

	if (mixer->src_split_req && mixer_mux == MDSS_MDP_MIXER_MUX_RIGHT)
		mixer_op_mode |= BIT(31);

	mdp_mixer_write(mixer_hw, MDSS_MDP_REG_LM_OP_MODE, mixer_op_mode);

	mdp_mixer_write(mixer_hw, MDSS_MDP_REG_LM_BORDER_COLOR_0,
		(mdata->bcolor0 & 0xFFF) | ((mdata->bcolor1 & 0xFFF) << 16));
	mdp_mixer_write(mixer_hw, MDSS_MDP_REG_LM_BORDER_COLOR_1,
		mdata->bcolor2 & 0xFFF);

	off = __mdss_mdp_ctl_get_mixer_off(mixer_hw);
	mdss_mdp_ctl_write(ctl, off, mixercfg);
	/* Program ctl layer extension bits */
	mdss_mdp_ctl_write(ctl, off + MDSS_MDP_REG_CTL_LAYER_EXTN_OFFSET,
		mixercfg_extn);

	pr_debug("mixer=%d hw=%d cfg=0%08x cfg_extn=0x%08x op_mode=0x%08x w=%d h=%d bc0=0x%x bc1=0x%x\n",
		mixer->num, mixer_hw->num, mixercfg, mixercfg_extn,
		mixer_op_mode, mixer->roi.w, mixer->roi.h,
		(mdata->bcolor0 & 0xFFF) | ((mdata->bcolor1 & 0xFFF) << 16),
		mdata->bcolor2 & 0xFFF);
	MDSS_XLOG(mixer->num, mixer_hw->num, mixercfg, mixercfg_extn,
		mixer_op_mode, mixer->roi.h, mixer->roi.w);
}

int mdss_mdp_mixer_addr_setup(struct mdss_data_type *mdata,
	 u32 *mixer_offsets, u32 *dspp_offsets, u32 *pingpong_offsets,
	 u32 type, u32 len)
{
	struct mdss_mdp_mixer *head;
	u32 i;
	int rc = 0;
	u32 size = len;

	if ((type == MDSS_MDP_MIXER_TYPE_WRITEBACK) &&
			(mdata->wfd_mode == MDSS_MDP_WFD_SHARED))
		size++;

	head = devm_kzalloc(&mdata->pdev->dev, sizeof(struct mdss_mdp_mixer) *
			size, GFP_KERNEL);

	if (!head) {
		pr_err("unable to setup mixer type=%d :kzalloc fail\n",
			type);
		return -ENOMEM;
	}

	for (i = 0; i < len; i++) {
		head[i].type = type;
		head[i].base = mdata->mdss_io.base + mixer_offsets[i];
		head[i].ref_cnt = 0;
		head[i].num = i;
		if (type == MDSS_MDP_MIXER_TYPE_INTF && dspp_offsets
				&& pingpong_offsets) {
			if (mdata->ndspp > i)
				head[i].dspp_base = mdata->mdss_io.base +
						dspp_offsets[i];
			head[i].pingpong_base = mdata->mdss_io.base +
					pingpong_offsets[i];
		}
	}

	/*
	 * Duplicate the last writeback mixer for concurrent line and block mode
	 * operations
	*/
	if ((type == MDSS_MDP_MIXER_TYPE_WRITEBACK) &&
			(mdata->wfd_mode == MDSS_MDP_WFD_SHARED))
		head[len] = head[len - 1];

	switch (type) {

	case MDSS_MDP_MIXER_TYPE_INTF:
		mdata->mixer_intf = head;
		break;

	case MDSS_MDP_MIXER_TYPE_WRITEBACK:
		mdata->mixer_wb = head;
		break;

	default:
		pr_err("Invalid mixer type=%d\n", type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

int mdss_mdp_ctl_addr_setup(struct mdss_data_type *mdata,
	u32 *ctl_offsets,  u32 len)
{
	struct mdss_mdp_ctl *head;
	struct mutex *shared_lock = NULL;
	u32 i;
	u32 size = len;

	if (mdata->wfd_mode == MDSS_MDP_WFD_SHARED) {
		size++;
		shared_lock = devm_kzalloc(&mdata->pdev->dev,
					   sizeof(struct mutex),
					   GFP_KERNEL);
		if (!shared_lock) {
			pr_err("unable to allocate mem for mutex\n");
			return -ENOMEM;
		}
		mutex_init(shared_lock);
	}

	head = devm_kzalloc(&mdata->pdev->dev, sizeof(struct mdss_mdp_ctl) *
			size, GFP_KERNEL);

	if (!head) {
		pr_err("unable to setup ctl and wb: kzalloc fail\n");
		return -ENOMEM;
	}

	for (i = 0; i < len; i++) {
		head[i].num = i;
		head[i].base = (mdata->mdss_io.base) + ctl_offsets[i];
		head[i].ref_cnt = 0;
	}

	if (mdata->wfd_mode == MDSS_MDP_WFD_SHARED) {
		head[len - 1].shared_lock = shared_lock;
		/*
		 * Allocate a virtual ctl to be able to perform simultaneous
		 * line mode and block mode operations on the same
		 * writeback block
		*/
		head[len] = head[len - 1];
		head[len].num = head[len - 1].num;
	}
	mdata->ctl_off = head;

	return 0;
}

int mdss_mdp_wb_addr_setup(struct mdss_data_type *mdata,
	u32 num_block_wb, u32 num_intf_wb)
{
	struct mdss_mdp_writeback *wb;
	u32 total, i;

	total = num_block_wb + num_intf_wb;
	wb = devm_kzalloc(&mdata->pdev->dev, sizeof(struct mdss_mdp_writeback) *
			total, GFP_KERNEL);
	if (!wb) {
		pr_err("unable to setup wb: kzalloc fail\n");
		return -ENOMEM;
	}

	for (i = 0; i < total; i++) {
		wb[i].num = i;
		if (i < num_block_wb) {
			wb[i].caps = MDSS_MDP_WB_ROTATOR | MDSS_MDP_WB_WFD;
			if (mdss_mdp_is_ubwc_supported(mdata))
				wb[i].caps |= MDSS_MDP_WB_UBWC;
		} else {
			wb[i].caps = MDSS_MDP_WB_WFD | MDSS_MDP_WB_INTF;
		}
	}

	mdata->wb = wb;
	mdata->nwb = total;
	mutex_init(&mdata->wb_lock);

	return 0;
}

struct mdss_mdp_mixer *mdss_mdp_mixer_get(struct mdss_mdp_ctl *ctl, int mux)
{
	struct mdss_mdp_mixer *mixer = NULL;

	if (!ctl) {
		pr_err("ctl not initialized\n");
		return NULL;
	}

	switch (mux) {
	case MDSS_MDP_MIXER_MUX_DEFAULT:
	case MDSS_MDP_MIXER_MUX_LEFT:
		mixer = ctl->mixer_left;
		break;
	case MDSS_MDP_MIXER_MUX_RIGHT:
		mixer = ctl->mixer_right;
		break;
	}

	return mixer;
}

struct mdss_mdp_pipe *mdss_mdp_get_staged_pipe(struct mdss_mdp_ctl *ctl,
	int mux, int stage, bool is_right_blend)
{
	struct mdss_mdp_pipe *pipe = NULL;
	struct mdss_mdp_mixer *mixer;
	int index = (stage * MAX_PIPES_PER_STAGE) + (int)is_right_blend;

	if (!ctl)
		return NULL;

	BUG_ON(index > MAX_PIPES_PER_LM);

	mixer = mdss_mdp_mixer_get(ctl, mux);
	if (mixer && (index < MAX_PIPES_PER_LM))
		pipe = mixer->stage_pipe[index];

	pr_debug("%pS index=%d pipe%d\n", __builtin_return_address(0),
		index, pipe ? pipe->num : -1);
	return pipe;
}

int mdss_mdp_get_pipe_flush_bits(struct mdss_mdp_pipe *pipe)
{
	u32 flush_bits;

	if (pipe->type == MDSS_MDP_PIPE_TYPE_DMA)
		flush_bits |= BIT(pipe->num) << 5;
	else if (pipe->num == MDSS_MDP_SSPP_VIG3 ||
			pipe->num == MDSS_MDP_SSPP_RGB3)
		flush_bits |= BIT(pipe->num) << 10;
	else if (pipe->type == MDSS_MDP_PIPE_TYPE_CURSOR)
		flush_bits |= BIT(22 + pipe->num - MDSS_MDP_SSPP_CURSOR0);
	else /* RGB/VIG 0-2 pipes */
		flush_bits |= BIT(pipe->num);

	return flush_bits;
}

int mdss_mdp_async_ctl_flush(struct msm_fb_data_type *mfd,
		u32 flush_bits)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_ctl *ctl = mdp5_data->ctl;
	struct mdss_mdp_ctl *sctl = mdss_mdp_get_split_ctl(ctl);
	int ret = 0;

	mutex_lock(&ctl->flush_lock);

	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_FLUSH, flush_bits);
	if ((!ctl->split_flush_en) && sctl)
		mdss_mdp_ctl_write(sctl, MDSS_MDP_REG_CTL_FLUSH, flush_bits);

	mutex_unlock(&ctl->flush_lock);
	return ret;
}

int mdss_mdp_mixer_pipe_update(struct mdss_mdp_pipe *pipe,
			 struct mdss_mdp_mixer *mixer, int params_changed)
{
	struct mdss_mdp_ctl *ctl;
	int i, j, k;

	if (!pipe)
		return -EINVAL;
	if (!mixer)
		return -EINVAL;
	ctl = mixer->ctl;
	if (!ctl)
		return -EINVAL;

	if (pipe->mixer_stage >= MDSS_MDP_MAX_STAGE) {
		pr_err("invalid mixer stage\n");
		return -EINVAL;
	}

	pr_debug("pnum=%x mixer=%d stage=%d\n", pipe->num, mixer->num,
			pipe->mixer_stage);

	mutex_lock(&ctl->flush_lock);

	if (params_changed) {
		mixer->params_changed++;
		for (i = MDSS_MDP_STAGE_UNUSED; i < MDSS_MDP_MAX_STAGE; i++) {
			j = i * MAX_PIPES_PER_STAGE;

			/*
			 * 1. If pipe is on the right side of the blending
			 *    stage, on either left LM or right LM but it is not
			 *    crossing LM boundry then right_blend ndx is used.
			 * 2. If pipe is on the right side of the blending
			 *    stage on left LM and it is crossing LM boundry
			 *    then for left LM it is placed into right_blend
			 *    index but for right LM it still placed into
			 *    left_blend index.
			 */
			if (pipe->is_right_blend && (!pipe->src_split_req ||
			    (pipe->src_split_req && !mixer->is_right_mixer)))
				j++;

			/* First clear all blend containers for current stage */
			for (k = 0; k < MAX_PIPES_PER_STAGE; k++) {
				u32 ndx = (i * MAX_PIPES_PER_STAGE) + k;

				if (mixer->stage_pipe[ndx] == pipe)
					mixer->stage_pipe[ndx] = NULL;
			}

			/* then stage actual pipe on specific blend container */
			if (i == pipe->mixer_stage)
				mixer->stage_pipe[j] = pipe;
		}
	}

	ctl->flush_bits |= mdss_mdp_get_pipe_flush_bits(pipe);

	mutex_unlock(&ctl->flush_lock);

	return 0;
}

/**
 * mdss_mdp_mixer_unstage_all() - Unstage all pipes from mixer
 * @mixer:	Mixer from which to unstage all pipes
 *
 * Unstage any pipes that are currently attached to mixer.
 *
 * NOTE: this will not update the pipe structure, and thus a full
 * deinitialization or reconfiguration of all pipes is expected after this call.
 */
void mdss_mdp_mixer_unstage_all(struct mdss_mdp_mixer *mixer)
{
	struct mdss_mdp_pipe *tmp;
	int i;

	if (!mixer)
		return;

	for (i = 0; i < MAX_PIPES_PER_LM; i++) {
		tmp = mixer->stage_pipe[i];
		if (tmp) {
			mixer->stage_pipe[i] = NULL;
			mixer->params_changed++;
			tmp->params_changed++;
		}
	}
}

int mdss_mdp_mixer_pipe_unstage(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_mixer *mixer)
{
	int index;
	u8 right_blend_index;

	if (!pipe)
		return -EINVAL;
	if (!mixer)
		return -EINVAL;

	right_blend_index = pipe->is_right_blend &&
		!(pipe->src_split_req && mixer->is_right_mixer);
	index = (pipe->mixer_stage * MAX_PIPES_PER_STAGE) + right_blend_index;

	if (index < MAX_PIPES_PER_LM && pipe == mixer->stage_pipe[index]) {
		pr_debug("unstage p%d from %s side of stage=%d lm=%d ndx=%d\n",
			pipe->num, pipe->is_right_blend ? "right" : "left",
			pipe->mixer_stage, mixer->num, index);

		mixer->params_changed++;
		mixer->stage_pipe[index] = NULL;
	}

	return 0;
}

int mdss_mdp_ctl_update_fps(struct mdss_mdp_ctl *ctl)
{
	struct mdss_panel_info *pinfo;
	struct mdss_overlay_private *mdp5_data;
	int ret = 0;
	int new_fps;

	if (!ctl->panel_data || !ctl->mfd)
		return -ENODEV;

	pinfo = &ctl->panel_data->panel_info;

	if (!pinfo->dynamic_fps || !ctl->ops.config_fps_fnc)
		return 0;

	if (!pinfo->default_fps) {
		/* we haven't got any call to update the fps */
		return 0;
	}

	mdp5_data = mfd_to_mdp5_data(ctl->mfd);
	if (!mdp5_data)
		return -ENODEV;

	/*
	 * Panel info is already updated with the new fps info,
	 * so we need to lock the data to make sure the panel info
	 * is not updated while we reconfigure the HW.
	 */
	mutex_lock(&mdp5_data->dfps_lock);

	if ((pinfo->dfps_update == DFPS_IMMEDIATE_PORCH_UPDATE_MODE_VFP) ||
		(pinfo->dfps_update == DFPS_IMMEDIATE_PORCH_UPDATE_MODE_HFP)) {
		new_fps = mdss_panel_get_framerate(pinfo);
	} else {
		new_fps = pinfo->new_fps;
	}

	pr_debug("fps new:%d old:%d\n", new_fps,
		pinfo->current_fps);

	if (new_fps == pinfo->current_fps) {
		pr_debug("FPS is already %d\n", new_fps);
		ret = 0;
		goto exit;
	}

	ret = ctl->ops.config_fps_fnc(ctl, new_fps);
	if (!ret)
		pr_debug("fps set to %d\n", new_fps);
	else
		pr_err("Failed to configure %d fps rc=%d\n",
			new_fps, ret);

exit:
	mutex_unlock(&mdp5_data->dfps_lock);
	return ret;
}

int mdss_mdp_display_wakeup_time(struct mdss_mdp_ctl *ctl,
				 ktime_t *wakeup_time)
{
	struct mdss_panel_info *pinfo;
	u32 clk_rate, clk_period;
	u32 current_line, total_line;
	u32 time_of_line, time_to_vsync, adjust_line_ns;

	ktime_t current_time = ktime_get();

	if (!ctl->ops.read_line_cnt_fnc)
		return -ENOSYS;

	pinfo = &ctl->panel_data->panel_info;
	if (!pinfo)
		return -ENODEV;

	clk_rate = mdss_mdp_get_pclk_rate(ctl);

	clk_rate /= 1000;	/* in kHz */
	if (!clk_rate)
		return -EINVAL;

	/*
	 * calculate clk_period as pico second to maintain good
	 * accuracy with high pclk rate and this number is in 17 bit
	 * range.
	 */
	clk_period = 1000000000 / clk_rate;
	if (!clk_period)
		return -EINVAL;

	time_of_line = (pinfo->lcdc.h_back_porch +
		 pinfo->lcdc.h_front_porch +
		 pinfo->lcdc.h_pulse_width +
		 pinfo->xres) * clk_period;

	time_of_line /= 1000;	/* in nano second */
	if (!time_of_line)
		return -EINVAL;

	current_line = ctl->ops.read_line_cnt_fnc(ctl);

	total_line = pinfo->lcdc.v_back_porch +
		pinfo->lcdc.v_front_porch +
		pinfo->lcdc.v_pulse_width +
		pinfo->yres;

	if (current_line > total_line)
		return -EINVAL;

	time_to_vsync = time_of_line * (total_line - current_line);

	if (pinfo->adjust_timer_delay_ms) {
		adjust_line_ns = pinfo->adjust_timer_delay_ms
			* 1000000; /* convert to ns */

		/* Ignore large values of adjust_line_ns\ */
		if (time_to_vsync > adjust_line_ns)
			time_to_vsync -= adjust_line_ns;
	}

	if (!time_to_vsync)
		return -EINVAL;

	*wakeup_time = ktime_add_ns(current_time, time_to_vsync);

	pr_debug("clk_rate=%dkHz clk_period=%d cur_line=%d tot_line=%d\n",
		clk_rate, clk_period, current_line, total_line);
	pr_debug("time_to_vsync=%d current_time=%d wakeup_time=%d\n",
		time_to_vsync, (int)ktime_to_ms(current_time),
		(int)ktime_to_ms(*wakeup_time));

	return 0;
}

int mdss_mdp_display_wait4comp(struct mdss_mdp_ctl *ctl)
{
	int ret;
	u32 reg_data, flush_data;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!ctl) {
		pr_err("invalid ctl\n");
		return -ENODEV;
	}

	ret = mutex_lock_interruptible(&ctl->lock);
	if (ret)
		return ret;

	if (!mdss_mdp_ctl_is_power_on(ctl)) {
		mutex_unlock(&ctl->lock);
		return 0;
	}

	ATRACE_BEGIN("wait_fnc");
	if (ctl->ops.wait_fnc)
		ret = ctl->ops.wait_fnc(ctl, NULL);
	ATRACE_END("wait_fnc");

	trace_mdp_commit(ctl);

	mdss_mdp_ctl_perf_update(ctl, 0, false);

	if (IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev, MDSS_MDP_HW_REV_103)) {
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
		reg_data = mdss_mdp_ctl_read(ctl, MDSS_MDP_REG_CTL_FLUSH);
		flush_data = readl_relaxed(mdata->mdp_base + AHB_CLK_OFFSET);
		if ((flush_data & BIT(28)) &&
		    !(ctl->flush_reg_data & reg_data)) {

			flush_data &= ~(BIT(28));
			writel_relaxed(flush_data,
					 mdata->mdp_base + AHB_CLK_OFFSET);
			ctl->flush_reg_data = 0;
		}
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
	}

	mutex_unlock(&ctl->lock);
	return ret;
}

int mdss_mdp_display_wait4pingpong(struct mdss_mdp_ctl *ctl, bool use_lock)
{
	struct mdss_mdp_ctl *sctl = NULL;
	int ret;
	bool recovery_needed = false;

	if (use_lock) {
		ret = mutex_lock_interruptible(&ctl->lock);
		if (ret)
			return ret;
	}

	if (!mdss_mdp_ctl_is_power_on(ctl) || !ctl->ops.wait_pingpong) {
		if (use_lock)
			mutex_unlock(&ctl->lock);
		return 0;
	}

	ATRACE_BEGIN("wait_pingpong");
	ret = ctl->ops.wait_pingpong(ctl, NULL);
	ATRACE_END("wait_pingpong");
	if (ret)
		recovery_needed = true;

	sctl = mdss_mdp_get_split_ctl(ctl);

	if (sctl && sctl->ops.wait_pingpong) {
		ATRACE_BEGIN("wait_pingpong sctl");
		ret = sctl->ops.wait_pingpong(sctl, NULL);
		ATRACE_END("wait_pingpong sctl");
		if (ret)
			recovery_needed = true;
	}

	if (recovery_needed) {
		mdss_mdp_ctl_reset(ctl, true);
		if (sctl)
			mdss_mdp_ctl_reset(sctl, true);

		mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_DSI_RESET_WRITE_PTR,
			NULL, CTL_INTF_EVENT_FLAG_DEFAULT);

		pr_debug("pingpong timeout recovery finished\n");
	}

	if (use_lock)
		mutex_unlock(&ctl->lock);

	return ret;
}

static void mdss_mdp_force_border_color(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_ctl *sctl = mdss_mdp_get_split_ctl(ctl);
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool lm_swap = mdss_mdp_is_lm_swap_needed(mdata, ctl);

	ctl->force_screen_state = MDSS_SCREEN_FORCE_BLANK;

	if (sctl)
		sctl->force_screen_state = MDSS_SCREEN_FORCE_BLANK;

	mdss_mdp_mixer_setup(ctl, MDSS_MDP_MIXER_MUX_LEFT, lm_swap);
	mdss_mdp_mixer_setup(ctl, MDSS_MDP_MIXER_MUX_RIGHT, lm_swap);

	ctl->force_screen_state = MDSS_SCREEN_DEFAULT;
	if (sctl)
		sctl->force_screen_state = MDSS_SCREEN_DEFAULT;

	/*
	 * Update the params changed for mixer for the next frame to
	 * configure the mixer setup properly.
	 */
	if (ctl->mixer_left)
		ctl->mixer_left->params_changed++;
	if (ctl->mixer_right)
		ctl->mixer_right->params_changed++;
}

int mdss_mdp_display_commit(struct mdss_mdp_ctl *ctl, void *arg,
	struct mdss_mdp_commit_cb *commit_cb)
{
	struct mdss_mdp_ctl *sctl = NULL;
	int ret = 0;
	bool is_bw_released, split_lm_valid;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 ctl_flush_bits = 0, sctl_flush_bits = 0;

	if (!ctl) {
		pr_err("display function not set\n");
		return -ENODEV;
	}

	mutex_lock(&ctl->lock);
	pr_debug("commit ctl=%d play_cnt=%d\n", ctl->num, ctl->play_cnt);

	if (!mdss_mdp_ctl_is_power_on(ctl)) {
		mutex_unlock(&ctl->lock);
		return 0;
	}

	split_lm_valid = mdss_mdp_is_both_lm_valid(ctl);

	sctl = mdss_mdp_get_split_ctl(ctl);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

	mutex_lock(&ctl->flush_lock);

	/*
	 * We could have released the bandwidth if there were no transactions
	 * pending, so we want to re-calculate the bandwidth in this situation
	 */
	is_bw_released = !mdss_mdp_ctl_perf_get_transaction_status(ctl);
	if (is_bw_released) {
		if (sctl)
			is_bw_released =
				!mdss_mdp_ctl_perf_get_transaction_status(sctl);
	}

	/*
	 * left update on any topology or
	 * any update on MDP_DUAL_LM_SINGLE_DISPLAY topology.
	 */
	if (ctl->mixer_left->valid_roi ||
	    (is_dual_lm_single_display(ctl->mfd) &&
	     ctl->mixer_right->valid_roi))
		mdss_mdp_ctl_perf_set_transaction_status(ctl,
				PERF_SW_COMMIT_STATE, PERF_STATUS_BUSY);

	/* right update on MDP_DUAL_LM_DUAL_DISPLAY */
	if (sctl && sctl->mixer_left->valid_roi)
		mdss_mdp_ctl_perf_set_transaction_status(sctl,
			PERF_SW_COMMIT_STATE, PERF_STATUS_BUSY);

	if (ctl->mixer_right)
		ctl->mixer_right->src_split_req =
			mdata->has_src_split && split_lm_valid;

	if (is_bw_released || ctl->force_screen_state ||
	    (ctl->mixer_left->params_changed) ||
	    (ctl->mixer_right && ctl->mixer_right->params_changed)) {
		bool lm_swap = mdss_mdp_is_lm_swap_needed(mdata, ctl);

		ATRACE_BEGIN("prepare_fnc");
		if (ctl->ops.prepare_fnc)
			ret = ctl->ops.prepare_fnc(ctl, arg);
		ATRACE_END("prepare_fnc");
		if (ret) {
			pr_err("error preparing display\n");
			mutex_unlock(&ctl->flush_lock);
			goto done;
		}

		ATRACE_BEGIN("mixer_programming");
		mdss_mdp_ctl_perf_update(ctl, 1, false);

		mdss_mdp_mixer_setup(ctl, MDSS_MDP_MIXER_MUX_LEFT, lm_swap);
		mdss_mdp_mixer_setup(ctl, MDSS_MDP_MIXER_MUX_RIGHT, lm_swap);

		mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_TOP, ctl->opmode);
		ctl->flush_bits |= BIT(17);	/* CTL */

		if (sctl) {
			mdss_mdp_ctl_write(sctl, MDSS_MDP_REG_CTL_TOP,
					sctl->opmode);
			sctl->flush_bits |= BIT(17);
			sctl_flush_bits = sctl->flush_bits;
		}
		ATRACE_END("mixer_programming");
	}

	/*
	 * With partial frame update, enable split display bit only
	 * when validity of ROI's on both the DSI's are identical.
	 */
	if (sctl)
		mdss_mdp_ctl_split_display_enable(split_lm_valid, ctl, sctl);

	ATRACE_BEGIN("postproc_programming");
	if (ctl->mfd && ctl->mfd->dcm_state != DTM_ENTER)
		/* postprocessing setup, including dspp */
		mdss_mdp_pp_setup_locked(ctl);

	if (sctl) {
		if (ctl->split_flush_en) {
			ctl->flush_bits |= sctl->flush_bits;
			sctl->flush_bits = 0;
			sctl_flush_bits = 0;
		} else {
			sctl_flush_bits = sctl->flush_bits;
		}
	}
	ctl_flush_bits = ctl->flush_bits;

	ATRACE_END("postproc_programming");

	mutex_unlock(&ctl->flush_lock);

	ATRACE_BEGIN("frame_ready");
	mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_CFG_DONE);
	if (commit_cb)
		commit_cb->commit_cb_fnc(
			MDP_COMMIT_STAGE_SETUP_DONE,
			commit_cb->data);
	ret = mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_READY);

	/*
	 * When wait for fence timed out, driver ignores the fences
	 * for signalling. Hardware needs to access only on the buffers
	 * that are valid and driver needs to ensure it. This function
	 * would set the mixer state to border when there is timeout.
	 */
	if (ret == NOTIFY_BAD) {
		mdss_mdp_force_border_color(ctl);
		ret = 0;
	}

	ATRACE_END("frame_ready");

	if (ctl->ops.wait_pingpong && !mdata->serialize_wait4pp)
		mdss_mdp_display_wait4pingpong(ctl, false);

	/*
	 * if serialize_wait4pp is false then roi_bkup used in wait4pingpong
	 * will be of previous frame as expected.
	 */
	ctl->roi_bkup.w = ctl->roi.w;
	ctl->roi_bkup.h = ctl->roi.h;

	/*
	 * update roi of panel_info which will be
	 * used by dsi to set col_page addr of panel.
	 */
	if (ctl->panel_data &&
	    ctl->panel_data->panel_info.partial_update_enabled) {

		if (is_pingpong_split(ctl->mfd)) {
			bool pp_split = false;
			struct mdss_rect l_roi, r_roi, temp = {0};
			u32 opmode = mdss_mdp_ctl_read(ctl,
			     MDSS_MDP_REG_CTL_TOP) & ~0xF0; /* clear OUT_SEL */
			/*
			 * with pp split enabled, it is a requirement that both
			 * panels share equal load, so split-point is center.
			 */
			u32 left_panel_w = left_lm_w_from_mfd(ctl->mfd) / 2;

			mdss_rect_split(&ctl->roi, &l_roi, &r_roi,
				left_panel_w);

			/*
			 * If update is only on left panel then we still send
			 * zeroed out right panel ROIs to DSI driver. Based on
			 * zeroed ROI, DSI driver identifies which panel is not
			 * transmitting.
			 */
			ctl->panel_data->panel_info.roi = l_roi;
			ctl->panel_data->next->panel_info.roi = r_roi;

			/* based on the roi, update ctl topology */
			if (!mdss_rect_cmp(&temp, &l_roi) &&
			    !mdss_rect_cmp(&temp, &r_roi)) {
				/* left + right */
				opmode |= (ctl->intf_num << 4);
				pp_split = true;
			} else if (mdss_rect_cmp(&temp, &l_roi)) {
				/* right only */
				opmode |= (ctl->slave_intf_num << 4);
				pp_split = false;
			} else {
				/* left only */
				opmode |= (ctl->intf_num << 4);
				pp_split = false;
			}

			mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_TOP, opmode);

			mdss_mdp_ctl_pp_split_display_enable(pp_split, ctl);
		} else {
			/*
			 * if single lm update on 3D mux topology, clear it.
			 */
			if ((is_dual_lm_single_display(ctl->mfd)) &&
			    (ctl->opmode & MDSS_MDP_CTL_OP_PACK_3D_ENABLE) &&
			    (!mdss_mdp_is_both_lm_valid(ctl))) {

				u32 opmode = mdss_mdp_ctl_read(ctl,
					MDSS_MDP_REG_CTL_TOP);
			       opmode &= ~(0xF << 19); /* clear 3D Mux */

				mdss_mdp_ctl_write(ctl,
					MDSS_MDP_REG_CTL_TOP, opmode);
			}

			ctl->panel_data->panel_info.roi = ctl->roi;
			if (sctl && sctl->panel_data)
				sctl->panel_data->panel_info.roi = sctl->roi;
		}
	}

	if (commit_cb)
		commit_cb->commit_cb_fnc(MDP_COMMIT_STAGE_READY_FOR_KICKOFF,
			commit_cb->data);

	if (mdss_has_quirk(mdata, MDSS_QUIRK_BWCPANIC) &&
	    !bitmap_empty(mdata->bwc_enable_map, MAX_DRV_SUP_PIPES))
		mdss_mdp_bwcpanic_ctrl(mdata, true);

	ATRACE_BEGIN("flush_kickoff");
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_FLUSH, ctl_flush_bits);
	if (sctl && sctl_flush_bits) {
		mdss_mdp_ctl_write(sctl, MDSS_MDP_REG_CTL_FLUSH,
			sctl_flush_bits);
		sctl->flush_bits = 0;
	}
	MDSS_XLOG(ctl->intf_num, ctl_flush_bits, sctl_flush_bits,
		split_lm_valid);
	wmb();
	ctl->flush_reg_data = ctl_flush_bits;
	ctl->flush_bits = 0;

	mdss_mdp_mixer_update_pipe_map(ctl, MDSS_MDP_MIXER_MUX_LEFT);
	mdss_mdp_mixer_update_pipe_map(ctl, MDSS_MDP_MIXER_MUX_RIGHT);

	/* right-only kickoff */
	if (!ctl->mixer_left->valid_roi &&
	    sctl && sctl->mixer_left->valid_roi) {
		/*
		 * Seperate kickoff on DSI1 is needed only when we have
		 * ONLY right half updating on a dual DSI panel
		 */
		if (sctl->ops.display_fnc)
			ret = sctl->ops.display_fnc(sctl, arg);
	} else {
		if (ctl->ops.display_fnc)
			ret = ctl->ops.display_fnc(ctl, arg); /* DSI0 kickoff */
	}

	if (ret)
		pr_warn("ctl %d error displaying frame\n", ctl->num);

	ctl->play_cnt++;
	ATRACE_END("flush_kickoff");

done:
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	mutex_unlock(&ctl->lock);

	return ret;
}

void mdss_mdp_ctl_notifier_register(struct mdss_mdp_ctl *ctl,
	struct notifier_block *notifier)
{
	struct mdss_mdp_ctl *sctl;

	blocking_notifier_chain_register(&ctl->notifier_head, notifier);

	sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl)
		blocking_notifier_chain_register(&sctl->notifier_head,
						notifier);
}

void mdss_mdp_ctl_notifier_unregister(struct mdss_mdp_ctl *ctl,
	struct notifier_block *notifier)
{
	struct mdss_mdp_ctl *sctl;
	blocking_notifier_chain_unregister(&ctl->notifier_head, notifier);

	sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl)
		blocking_notifier_chain_unregister(&sctl->notifier_head,
						notifier);
}

int mdss_mdp_ctl_notify(struct mdss_mdp_ctl *ctl, int event)
{
	return blocking_notifier_call_chain(&ctl->notifier_head, event, ctl);
}

int mdss_mdp_get_ctl_mixers(u32 fb_num, u32 *mixer_id)
{
	int i;
	struct mdss_mdp_ctl *ctl;
	struct mdss_data_type *mdata;
	u32 mixer_cnt = 0;
	mutex_lock(&mdss_mdp_ctl_lock);
	mdata = mdss_mdp_get_mdata();
	for (i = 0; i < mdata->nctl; i++) {
		ctl = mdata->ctl_off + i;
		if ((mdss_mdp_ctl_is_power_on(ctl)) && (ctl->mfd) &&
			(ctl->mfd->index == fb_num)) {
			if (ctl->mixer_left) {
				mixer_id[mixer_cnt] = ctl->mixer_left->num;
				mixer_cnt++;
			}
			if (mixer_cnt && ctl->mixer_right) {
				mixer_id[mixer_cnt] = ctl->mixer_right->num;
				mixer_cnt++;
			}
			if (mixer_cnt)
				break;
		}
	}
	mutex_unlock(&mdss_mdp_ctl_lock);
	return mixer_cnt;
}

/**
 * @mdss_mdp_ctl_mixer_switch() - return ctl mixer of @return_type
 * @ctl: Pointer to ctl structure to be switched.
 * @return_type: wb_type of the ctl to be switched to.
 *
 * Virtual mixer switch should be performed only when there is no
 * dedicated wfd block and writeback block is shared.
 */
struct mdss_mdp_ctl *mdss_mdp_ctl_mixer_switch(struct mdss_mdp_ctl *ctl,
					       u32 return_type)
{
	int i;
	struct mdss_data_type *mdata = ctl->mdata;

	if (ctl->wb_type == return_type) {
		mdata->mixer_switched = false;
		return ctl;
	}
	for (i = 0; i <= mdata->nctl; i++) {
		if (mdata->ctl_off[i].wb_type == return_type) {
			pr_debug("switching mixer from ctl=%d to ctl=%d\n",
				 ctl->num, mdata->ctl_off[i].num);
			mdata->mixer_switched = true;
			return mdata->ctl_off + i;
		}
	}
	pr_err("unable to switch mixer to type=%d\n", return_type);
	return NULL;
}

static inline int __mdss_mdp_ctl_get_mixer_off(struct mdss_mdp_mixer *mixer)
{
	if (mixer->type == MDSS_MDP_MIXER_TYPE_INTF) {
		if (mixer->num == MDSS_MDP_INTF_LAYERMIXER3)
			return MDSS_MDP_CTL_X_LAYER_5;
		else
			return MDSS_MDP_REG_CTL_LAYER(mixer->num);
	} else {
		return MDSS_MDP_REG_CTL_LAYER(mixer->num +
				MDSS_MDP_INTF_LAYERMIXER3);
	}
}

u32 mdss_mdp_get_mixercfg(struct mdss_mdp_mixer *mixer, bool extn)
{
	u32 mixer_off;

	if (!mixer || !mixer->ctl)
		return 0;

	mixer_off = __mdss_mdp_ctl_get_mixer_off(mixer);
	mixer_off = extn ? (mixer_off + MDSS_MDP_REG_CTL_LAYER_EXTN_OFFSET) :
				mixer_off;

	return mdss_mdp_ctl_read(mixer->ctl, mixer_off);
}

static int __mdss_mdp_mixer_handoff_helper(struct mdss_mdp_mixer *mixer,
	struct mdss_mdp_pipe *pipe)
{
	int rc = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 right_blend = 0;

	if (!mixer) {
		rc = -EINVAL;
		goto error;
	}

	/*
	 * It is possible to have more the one pipe staged on a single
	 * layer mixer at same staging level.
	 */
	if (mixer->stage_pipe[MDSS_MDP_STAGE_UNUSED] != NULL) {
		if (mdata->mdp_rev < MDSS_MDP_HW_REV_103) {
			pr_err("More than one pipe staged on mixer num %d\n",
				mixer->num);
			rc = -EINVAL;
			goto error;
		} else if (mixer->stage_pipe[MDSS_MDP_STAGE_UNUSED + 1] !=
			NULL) {
			pr_err("More than two pipe staged on mixer num %d\n",
				mixer->num);
			rc = -EINVAL;
			goto error;
		} else {
			right_blend = 1;
		}
	}

	pr_debug("Staging pipe num %d on mixer num %d\n",
		pipe->num, mixer->num);
	mixer->stage_pipe[MDSS_MDP_STAGE_UNUSED + right_blend] = pipe;
	pipe->mixer_left = mixer;
	pipe->mixer_stage = MDSS_MDP_STAGE_UNUSED;

error:
	return rc;
}

/**
 * mdss_mdp_mixer_handoff() - Stages a given pipe on the appropriate mixer
 * @ctl:  pointer to the control structure associated with the overlay device.
 * @num:  the mixer number on which the pipe needs to be staged.
 * @pipe: pointer to the pipe to be staged.
 *
 * Function stages a given pipe on either the left mixer or the right mixer
 * for the control structre based on the mixer number. If the input mixer
 * number does not match either of the mixers then an error is returned.
 * This function is called during overlay handoff when certain pipes are
 * already staged by the bootloader.
 */
int mdss_mdp_mixer_handoff(struct mdss_mdp_ctl *ctl, u32 num,
	struct mdss_mdp_pipe *pipe)
{
	int rc = 0;
	struct mdss_mdp_mixer *mx_left = ctl->mixer_left;
	struct mdss_mdp_mixer *mx_right = ctl->mixer_right;

	/*
	 * For performance calculations, stage the handed off pipe
	 * as MDSS_MDP_STAGE_UNUSED
	 */
	if (mx_left && (mx_left->num == num)) {
		rc = __mdss_mdp_mixer_handoff_helper(mx_left, pipe);
	} else if (mx_right && (mx_right->num == num)) {
		rc = __mdss_mdp_mixer_handoff_helper(mx_right, pipe);
	} else {
		pr_err("pipe num %d staged on unallocated mixer num %d\n",
			pipe->num, num);
		rc = -EINVAL;
	}

	return rc;
}

struct mdss_mdp_writeback *mdss_mdp_wb_alloc(u32 caps, u32 reg_index)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_writeback *wb = NULL;
	int i;
	bool wb_virtual_on;

	wb_virtual_on = (mdata->nctl == mdata->nwb_offsets);

	if (wb_virtual_on && reg_index >= mdata->nwb_offsets)
		return NULL;

	mutex_lock(&mdata->wb_lock);

	for (i = 0; i < mdata->nwb; i++) {
		wb = mdata->wb + i;
		if ((wb->caps & caps) &&
			(atomic_read(&wb->kref.refcount) == 0)) {
			kref_init(&wb->kref);
			break;
		}
		wb = NULL;
	}
	mutex_unlock(&mdata->wb_lock);

	if (wb) {
		wb->base = mdata->mdss_io.base;
		if (wb_virtual_on)
			wb->base += mdata->wb_offsets[reg_index];
		else
			wb->base += mdata->wb_offsets[i];
	}

	return wb;
}

bool mdss_mdp_is_wb_mdp_intf(u32 num, u32 reg_index)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_writeback *wb = NULL;
	bool wb_virtual_on;

	wb_virtual_on = (mdata->nctl == mdata->nwb_offsets);

	if (num >= mdata->nwb || (wb_virtual_on && reg_index >=
			mdata->nwb_offsets))
		return false;

	wb = mdata->wb + num;
	if (!wb)
		return false;

	return (wb->caps & MDSS_MDP_WB_INTF) ? true : false;
}

struct mdss_mdp_writeback *mdss_mdp_wb_assign(u32 num, u32 reg_index)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_writeback *wb = NULL;
	bool wb_virtual_on;

	wb_virtual_on = (mdata->nctl == mdata->nwb_offsets);

	if (num >= mdata->nwb)
		return NULL;

	if (wb_virtual_on && reg_index >= mdata->nwb_offsets)
		return NULL;

	mutex_lock(&mdata->wb_lock);
	wb = mdata->wb + num;
	if (atomic_read(&wb->kref.refcount) == 0)
		kref_init(&wb->kref);
	else
		wb = NULL;
	mutex_unlock(&mdata->wb_lock);

	if (!wb)
		return NULL;

	wb->base = mdata->mdss_io.base;
	if (wb_virtual_on)
		wb->base += mdata->wb_offsets[reg_index];
	else
		wb->base += mdata->wb_offsets[num];

	return wb;
}

static void mdss_mdp_wb_release(struct kref *kref)
{
	struct mdss_mdp_writeback *wb =
		container_of(kref, struct mdss_mdp_writeback, kref);

	if (!wb)
		return;

	wb->base = NULL;
}

void mdss_mdp_wb_free(struct mdss_mdp_writeback *wb)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (kref_put_mutex(&wb->kref, mdss_mdp_wb_release,
			&mdata->wb_lock))
		mutex_unlock(&mdata->wb_lock);
}
