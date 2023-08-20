/*
 * linux/drivers/video/fbdev/exynos/panel/ea8079/ea8079.c
 *
 * EA8079 Dimming Driver
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#include "../panel.h"
#include "ea8079.h"
#include "ea8079_panel.h"
#ifdef CONFIG_PANEL_AID_DIMMING
#include "../dimming.h"
#include "../panel_dimming.h"
#endif
#include "../panel_drv.h"

#ifdef PANEL_PR_TAG
#undef PANEL_PR_TAG
#define PANEL_PR_TAG	"ddi"
#endif

#ifdef CONFIG_PANEL_AID_DIMMING

static int generate_brt_step_table(struct brightness_table *brt_tbl)
{
	int ret = 0;
	int i = 0, j = 0, k = 0;

	if (unlikely(!brt_tbl || !brt_tbl->brt)) {
		panel_err("invalid parameter\n");
		return -EINVAL;
	}
	if (unlikely(!brt_tbl->step_cnt)) {
		if (likely(brt_tbl->brt_to_step)) {
			panel_info("we use static step table\n");
			return ret;
		} else {
			panel_err("invalid parameter, all table is NULL\n");
			return -EINVAL;
		}
	}

	brt_tbl->sz_brt_to_step = 0;
	for(i = 0; i < brt_tbl->sz_step_cnt; i++)
		brt_tbl->sz_brt_to_step += brt_tbl->step_cnt[i];

	brt_tbl->brt_to_step =
		(u32 *)kmalloc(brt_tbl->sz_brt_to_step * sizeof(u32), GFP_KERNEL);

	if (unlikely(!brt_tbl->brt_to_step)) {
		panel_err("alloc fail\n");
		return -EINVAL;
	}
	brt_tbl->brt_to_step[0] = brt_tbl->brt[0];
	i = 1;
	while (i < brt_tbl->sz_brt_to_step) {
		for (k = 1; k < brt_tbl->sz_brt; k++) {
			for (j = 1; j <= brt_tbl->step_cnt[k]; j++, i++) {
				brt_tbl->brt_to_step[i] = interpolation(brt_tbl->brt[k - 1] * disp_pow(10, 2),
					brt_tbl->brt[k] * disp_pow(10, 2), j, brt_tbl->step_cnt[k]);
				brt_tbl->brt_to_step[i] = disp_pow_round(brt_tbl->brt_to_step[i], 2);
				brt_tbl->brt_to_step[i] = disp_div64(brt_tbl->brt_to_step[i], disp_pow(10, 2));
				if (brt_tbl->brt[brt_tbl->sz_brt - 1] < brt_tbl->brt_to_step[i]) {

					brt_tbl->brt_to_step[i] = disp_pow_round(brt_tbl->brt_to_step[i], 2);
				}
				if (i >= brt_tbl->sz_brt_to_step) {
					panel_err("step cnt over %d %d\n", i, brt_tbl->sz_brt_to_step);
					break;
				}
			}
		}
	}
	return ret;
}

#endif /* CONFIG_PANEL_AID_DIMMING */

int init_common_table(struct maptbl *tbl)
{
	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
static int getidx_common_maptbl(struct maptbl *tbl)
{
	return 0;
}
#endif

#ifdef CONFIG_SUPPORT_DOZE
#ifdef CONFIG_SUPPORT_AOD_BL
static int init_aod_dimming_table(struct maptbl *tbl)
{
	int id = PANEL_BL_SUBDEV_TYPE_AOD;
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;

	if (unlikely(!tbl || !tbl->pdata)) {
		panel_err("panel_bl-%d invalid param (tbl %p, pdata %p)\n",
				id, tbl, tbl ? tbl->pdata : NULL);
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_bl = &panel->panel_bl;

	if (unlikely(!panel->panel_data.panel_dim_info[id])) {
		panel_err("panel_bl-%d panel_dim_info is null\n", id);
		return -EINVAL;
	}

	memcpy(&panel_bl->subdev[id].brt_tbl,
			panel->panel_data.panel_dim_info[id]->brt_tbl,
			sizeof(struct brightness_table));

	return 0;
}
#endif
#endif

static void copy_tset_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;

	if (!tbl || !dst)
		return;

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel))
		return;

	panel_data = &panel->panel_data;

	*dst = (panel_data->props.temperature < 0) ?
		BIT(7) | abs(panel_data->props.temperature) :
		panel_data->props.temperature;
}

#if 0
static void copy_grayspot_cal_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct panel_device *panel;
	struct panel_info *panel_data;
	u8 val;
	int ret = 0;

	if (!tbl || !dst)
		return;

	panel = (struct panel_device *)tbl->pdata;
	if (unlikely(!panel))
		return;

	panel_data = &panel->panel_data;

	ret = resource_copy_by_name(panel_data, &val, "grayspot_cal");
	if (unlikely(ret)) {
		panel_err("grayspot_cal not found in panel resource\n");
		return;
	}

	panel_info("grayspot_cal 0x%02x\n", val);
	*dst = val;
}
#endif

#ifdef CONFIG_SUPPORT_XTALK_MODE
static int getidx_vgh_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data;
	int row = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;

	row = ((panel_data->props.xtalk_mode) ? 1 : 0);
	panel_info("xtalk_mode %d\n", row);

	return maptbl_index(tbl, 0, row, 0);
}
#endif

static int getidx_gm2_elvss_table(struct maptbl *tbl)
{
	int row;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	row = get_actual_brightness_index(panel_bl, panel_bl->props.brightness);

	return maptbl_index(tbl, 0, row, 0);
}

static int getidx_hbm_transition_table(struct maptbl *tbl)
{
	int layer, row;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;
	panel_bl = &panel->panel_bl;

	layer = is_hbm_brightness(panel_bl, panel_bl->props.brightness);
	row = panel_bl->props.smooth_transition;

	return maptbl_index(tbl, layer, row, 0);
}

static int getidx_acl_onoff_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	return maptbl_index(tbl, 0, panel_bl_get_acl_pwrsave(&panel->panel_bl), 0);
}

static int getidx_hbm_onoff_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_bl_device *panel_bl;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_bl = &panel->panel_bl;

	return maptbl_index(tbl, 0,
			is_hbm_brightness(panel_bl, panel_bl->props.brightness), 0);
}

static int getidx_acl_dim_onoff_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_bl_device *panel_bl;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_bl = &panel->panel_bl;

#ifdef CONFIG_SUPPORT_MASK_LAYER
	return maptbl_index(tbl, 0,
			 panel_bl->props.mask_layer_br_hook == MASK_LAYER_HOOK_ON ? false : true , 0);
#else
	return maptbl_index(tbl, 0, true : 0 , 0);
#endif
}

static int getidx_acl_opr_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int row;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	if (panel_bl_get_acl_pwrsave(&panel->panel_bl) == ACL_PWRSAVE_OFF)
		row = ACL_OPR_OFF;
	else
		row = panel_bl_get_acl_opr(&panel->panel_bl);

	return maptbl_index(tbl, 0, row, 0);
}

static int init_lpm_brt_table(struct maptbl *tbl)
{
#ifdef CONFIG_SUPPORT_AOD_BL
	return init_aod_dimming_table(tbl);
#else
	return init_common_table(tbl);
#endif
}

static int getidx_lpm_brt_table(struct maptbl *tbl)
{
	int row = 0;
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;
	struct panel_properties *props;

	panel = (struct panel_device *)tbl->pdata;
	panel_bl = &panel->panel_bl;
	props = &panel->panel_data.props;

#ifdef CONFIG_SUPPORT_DOZE
#ifdef CONFIG_SUPPORT_AOD_BL
	panel_bl = &panel->panel_bl;
	row = get_subdev_actual_brightness_index(panel_bl, PANEL_BL_SUBDEV_TYPE_AOD,
			panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness);

	props->lpm_brightness = panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness;
	panel_info("alpm_mode %d, brightness %d, row %d\n", props->cur_alpm_mode,
		panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness, row);

#else
	switch (props->alpm_mode) {
	case ALPM_LOW_BR:
	case HLPM_LOW_BR:
		row = 0;
		break;
	case ALPM_HIGH_BR:
	case HLPM_HIGH_BR:
		row = tbl->nrow - 1;
		break;
	default:
		panel_err("Invalid alpm mode : %d\n", props->alpm_mode);
		break;
	}

	panel_info("alpm_mode %d, row %d\n", props->alpm_mode, row);
#endif
#endif

	return maptbl_index(tbl, 0, row, 0);
}

#if 0
static int getidx_lpm_on_table(struct maptbl *tbl)
{
	int row = 0;
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;
	struct panel_properties *props;

	panel = (struct panel_device *)tbl->pdata;
	panel_bl = &panel->panel_bl;
	props = &panel->panel_data.props;

#ifdef CONFIG_SUPPORT_DOZE
#ifdef CONFIG_SUPPORT_AOD_BL
	panel_bl = &panel->panel_bl;
	row = get_subdev_actual_brightness_index(panel_bl, PANEL_BL_SUBDEV_TYPE_AOD,
			panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness);

	props->lpm_brightness = panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness;
	panel_info("alpm_mode %d, brightness %d, row %d\n", props->cur_alpm_mode,
		panel_bl->subdev[PANEL_BL_SUBDEV_TYPE_AOD].brightness, row);

#else
	switch (props->alpm_mode) {
	case ALPM_LOW_BR:
	case HLPM_LOW_BR:
		row = 0;
		break;
	case ALPM_HIGH_BR:
	case HLPM_HIGH_BR:
		row = tbl->nrow - 1;
		break;
	default:
		panel_err("Invalid alpm mode : %d\n", props->alpm_mode);
		break;
	}

	panel_info("alpm_mode %d, row %d\n", props->alpm_mode, row);
#endif
#endif

	if (row > 0)
		row = 1;

	return maptbl_index(tbl, 0, row, 0);
}
#endif

static int getidx_ea8079_vrr_fps(int vrr_fps)
{
	int fps_index = EA8079_VRR_FPS_60;

	switch (vrr_fps) {
	case 120:
		fps_index = EA8079_VRR_FPS_120;
		break;
	case 96:
		fps_index = EA8079_VRR_FPS_96;
		break;
	case 60:
		fps_index = EA8079_VRR_FPS_60;
		break;
	default:
		fps_index = EA8079_VRR_FPS_60;
		panel_err("undefined FPS:%d\n", vrr_fps);
	}

	return fps_index;
}

static int getidx_ea8079_current_vrr_fps(struct panel_device *panel)
{
	int vrr_fps;

	vrr_fps = get_panel_refresh_rate(panel);
	if (vrr_fps < 0)
		return -EINVAL;

	return getidx_ea8079_vrr_fps(vrr_fps);
}

static int getidx_vrr_fps_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int row = 0, layer = 0, index;

	index = getidx_ea8079_current_vrr_fps(panel);
	if (index < 0)
		row = 0;
	else
		row = index;

	return maptbl_index(tbl, layer, row, 0);
}

#if defined(__PANEL_NOT_USED_VARIABLE__)
static int getidx_vrr_gamma_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	int row = 0, layer = 0, index;
	struct panel_bl_device *panel_bl;

	panel_bl = &panel->panel_bl;

	index = getidx_ea8079_current_vrr_fps(panel);
	if (index < 0)
		layer = 0;
	else
		layer = index;

	switch (panel_bl->bd->props.brightness) {
	case EA8079_R8S_VRR_GAMMA_INDEX_0_BR ... EA8079_R8S_VRR_GAMMA_INDEX_1_BR - 1:
		row = EA8079_GAMMA_BR_INDEX_0;
		break;
	case EA8079_R8S_VRR_GAMMA_INDEX_1_BR ... EA8079_R8S_VRR_GAMMA_INDEX_2_BR - 1:
		row = EA8079_GAMMA_BR_INDEX_1;
		break;
	case EA8079_R8S_VRR_GAMMA_INDEX_2_BR ... EA8079_R8S_VRR_GAMMA_INDEX_3_BR - 1:
		row = EA8079_GAMMA_BR_INDEX_2;
		break;
	case EA8079_R8S_VRR_GAMMA_INDEX_3_BR ... EA8079_R8S_VRR_GAMMA_INDEX_MAX_BR:
		row = EA8079_GAMMA_BR_INDEX_3;
		break;
	}

	return maptbl_index(tbl, layer, row, 0);
}
#endif

static int getidx_lpm_mode_table(struct maptbl *tbl)
{
	int row = 0;
	struct panel_device *panel;
	struct panel_bl_device *panel_bl;
	struct panel_properties *props;

	panel = (struct panel_device *)tbl->pdata;
	panel_bl = &panel->panel_bl;
	props = &panel->panel_data.props;

#ifdef CONFIG_SUPPORT_DOZE
	switch (props->alpm_mode) {
	case ALPM_LOW_BR:
	case ALPM_HIGH_BR:
		row = ALPM_MODE;
		break;
	case HLPM_LOW_BR:
	case HLPM_HIGH_BR:
		row = HLPM_MODE;
		break;
	default:
		panel_err("Invalid alpm mode : %d\n", props->alpm_mode);
		break;
	}
	panel_info("alpm_mode %d -> %d\n", props->cur_alpm_mode, props->alpm_mode);
	props->cur_alpm_mode = props->alpm_mode;
#endif

	return maptbl_index(tbl, 0, row, 0);
}

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
static void copy_dummy_maptbl(struct maptbl *tbl, u8 *dst)
{
	return;
}
#endif

static void copy_common_maptbl(struct maptbl *tbl, u8 *dst)
{
	int idx;

	if (!tbl || !dst) {
		panel_err("invalid parameter (tbl %p, dst %p)\n",
				tbl, dst);
		return;
	}

	idx = maptbl_getidx(tbl);
	if (idx < 0) {
		panel_err("failed to getidx %d\n", idx);
		return;
	}

	memcpy(dst, &(tbl->arr)[idx], sizeof(u8) * tbl->sz_copy);
	panel_dbg("copy from %s %d %d\n",
			tbl->name, idx, tbl->sz_copy);
	print_data(dst, tbl->sz_copy);
}

#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_SUPPORT_FAST_DISCHARGE)
static int getidx_fast_discharge_table(struct maptbl *tbl)
{
	struct panel_device *panel = (struct panel_device *)tbl->pdata;
	struct panel_info *panel_data;
	int row = 0;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}

	panel_data = &panel->panel_data;

	row = ((panel_data->props.enable_fd) ? 1 : 0);
	panel_info("fast_discharge %d\n", row);

	return maptbl_index(tbl, 0, row, 0);
}

#endif

#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
static int init_color_blind_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;

	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	if (EA8079_SCR_CR_OFS + mdnie->props.sz_scr > sizeof_maptbl(tbl)) {
		panel_err("invalid size (maptbl_size %d, sz_scr %d)\n",
				sizeof_maptbl(tbl), mdnie->props.sz_scr);
		return -EINVAL;
	}

	memcpy(&tbl->arr[EA8079_SCR_CR_OFS],
			mdnie->props.scr, mdnie->props.sz_scr);

	return 0;
}

static int getidx_mdnie_scenario_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;

	return tbl->ncol * (mdnie->props.mode);
}

static int getidx_mdnie_hdr_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;

	return tbl->ncol * (mdnie->props.hdr);
}

static int getidx_mdnie_night_mode_maptbl(struct maptbl *tbl)
{
	int mode = 0;
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;

	if(mdnie->props.mode != AUTO) mode = 1;

	return maptbl_index(tbl, mode , mdnie->props.night_level, 0);
}

static int init_mdnie_night_mode_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	struct maptbl *night_maptbl;

	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	night_maptbl = mdnie_find_etc_maptbl(mdnie, MDNIE_ETC_NIGHT_MAPTBL);
	if (!night_maptbl) {
		panel_err("NIGHT_MAPTBL not found\n");
		return -EINVAL;
	}

	if (sizeof_maptbl(tbl) < (EA8079_NIGHT_MODE_OFS +
			sizeof_row(night_maptbl))) {
		panel_err("invalid size (maptbl_size %d, night_maptbl_size %d)\n",
				sizeof_maptbl(tbl), sizeof_row(night_maptbl));
		return -EINVAL;
	}

	maptbl_copy(night_maptbl, &tbl->arr[EA8079_NIGHT_MODE_OFS]);

	return 0;
}

static int init_mdnie_color_lens_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	struct maptbl *color_lens_maptbl;

	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	color_lens_maptbl = mdnie_find_etc_maptbl(mdnie, MDNIE_ETC_COLOR_LENS_MAPTBL);
	if (!color_lens_maptbl) {
		panel_err("COLOR_LENS_MAPTBL not found\n");
		return -EINVAL;
	}

	if (sizeof_maptbl(tbl) < (EA8079_COLOR_LENS_OFS +
			sizeof_row(color_lens_maptbl))) {
		panel_err("invalid size (maptbl_size %d, color_lens_maptbl_size %d)\n",
				sizeof_maptbl(tbl), sizeof_row(color_lens_maptbl));
		return -EINVAL;
	}

	if (IS_COLOR_LENS_MODE(mdnie))
		maptbl_copy(color_lens_maptbl, &tbl->arr[EA8079_COLOR_LENS_OFS]);

	return 0;
}

static void update_current_scr_white(struct maptbl *tbl, u8 *dst)
{
	struct mdnie_info *mdnie;

	if (!tbl || !tbl->pdata) {
		panel_err("invalid param\n");
		return;
	}

	mdnie = (struct mdnie_info *)tbl->pdata;
	mdnie->props.cur_wrgb[0] = *dst;
	mdnie->props.cur_wrgb[1] = *(dst + 1);
	mdnie->props.cur_wrgb[2] = *(dst + 2);
}

static int init_color_coordinate_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	int type, color;

	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	if (sizeof_row(tbl) != ARRAY_SIZE(mdnie->props.coord_wrgb[0])) {
		panel_err("invalid maptbl size %d\n", tbl->ncol);
		return -EINVAL;
	}

	for_each_row(tbl, type) {
		for_each_col(tbl, color) {
			tbl->arr[sizeof_row(tbl) * type + color] =
				mdnie->props.coord_wrgb[type][color];
		}
	}

	return 0;
}

static int init_sensor_rgb_table(struct maptbl *tbl)
{
	struct mdnie_info *mdnie;
	int i;

	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	mdnie = tbl->pdata;

	if (tbl->ncol != ARRAY_SIZE(mdnie->props.ssr_wrgb)) {
		panel_err("invalid maptbl size %d\n", tbl->ncol);
		return -EINVAL;
	}

	for (i = 0; i < tbl->ncol; i++)
		tbl->arr[i] = mdnie->props.ssr_wrgb[i];

	return 0;
}

static int getidx_color_coordinate_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;
	static int wcrd_type[MODE_MAX] = {
		WCRD_TYPE_D65, WCRD_TYPE_D65, WCRD_TYPE_D65,
		WCRD_TYPE_ADAPTIVE, WCRD_TYPE_ADAPTIVE,
	};
	if ((mdnie->props.mode < 0) || (mdnie->props.mode >= MODE_MAX)) {
		panel_err("out of mode range %d\n", mdnie->props.mode);
		return -EINVAL;
	}
	return maptbl_index(tbl, 0, wcrd_type[mdnie->props.mode], 0);
}

static int getidx_adjust_ldu_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;
	static int wcrd_type[MODE_MAX] = {
		WCRD_TYPE_D65, WCRD_TYPE_D65, WCRD_TYPE_D65,
		WCRD_TYPE_ADAPTIVE, WCRD_TYPE_ADAPTIVE,
	};

	if (!IS_LDU_MODE(mdnie))
		return -EINVAL;

	if ((mdnie->props.mode < 0) || (mdnie->props.mode >= MODE_MAX)) {
		panel_err("out of mode range %d\n", mdnie->props.mode);
		return -EINVAL;
	}
	if ((mdnie->props.ldu < 0) || (mdnie->props.ldu >= MAX_LDU_MODE)) {
		panel_err("out of ldu mode range %d\n", mdnie->props.ldu);
		return -EINVAL;
	}
	return maptbl_index(tbl, wcrd_type[mdnie->props.mode], mdnie->props.ldu, 0);
}

static int getidx_color_lens_maptbl(struct maptbl *tbl)
{
	struct mdnie_info *mdnie = (struct mdnie_info *)tbl->pdata;

	if (!IS_COLOR_LENS_MODE(mdnie))
		return -EINVAL;

	if ((mdnie->props.color_lens_color < 0) || (mdnie->props.color_lens_color >= COLOR_LENS_COLOR_MAX)) {
		panel_err("out of color lens color range %d\n", mdnie->props.color_lens_color);
		return -EINVAL;
	}
	if ((mdnie->props.color_lens_level < 0) || (mdnie->props.color_lens_level >= COLOR_LENS_LEVEL_MAX)) {
		panel_err("out of color lens level range %d\n", mdnie->props.color_lens_level);
		return -EINVAL;
	}
	return maptbl_index(tbl, mdnie->props.color_lens_color, mdnie->props.color_lens_level, 0);
}

static void copy_color_coordinate_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct mdnie_info *mdnie;
	int i, idx;
	u8 value;

	if (unlikely(!tbl || !dst))
		return;

	mdnie = (struct mdnie_info *)tbl->pdata;
	idx = maptbl_getidx(tbl);
	if (idx < 0 || (idx + MAX_COLOR > sizeof_maptbl(tbl))) {
		panel_err("invalid index %d\n", idx);
		return;
	}

	if (tbl->ncol != MAX_COLOR) {
		panel_err("invalid maptbl size %d\n", tbl->ncol);
		return;
	}

	for (i = 0; i < tbl->ncol; i++) {
		mdnie->props.def_wrgb[i] = tbl->arr[idx + i];
		value = mdnie->props.def_wrgb[i] +
			(char)((mdnie->props.mode == AUTO) ?
				mdnie->props.def_wrgb_ofs[i] : 0);
		mdnie->props.cur_wrgb[i] = value;
		dst[i] = value;
		if (mdnie->props.mode == AUTO)
			panel_dbg("cur_wrgb[%d] %d(%02X) def_wrgb[%d] %d(%02X), def_wrgb_ofs[%d] %d\n",
					i, mdnie->props.cur_wrgb[i], mdnie->props.cur_wrgb[i],
					i, mdnie->props.def_wrgb[i], mdnie->props.def_wrgb[i],
					i, mdnie->props.def_wrgb_ofs[i]);
		else
			panel_dbg("cur_wrgb[%d] %d(%02X) def_wrgb[%d] %d(%02X), def_wrgb_ofs[%d] none\n",
					i, mdnie->props.cur_wrgb[i], mdnie->props.cur_wrgb[i],
					i, mdnie->props.def_wrgb[i], mdnie->props.def_wrgb[i], i);
	}
}

static void copy_scr_white_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct mdnie_info *mdnie;
	int i, idx;

	if (unlikely(!tbl || !dst))
		return;

	mdnie = (struct mdnie_info *)tbl->pdata;
	idx = maptbl_getidx(tbl);
	if (idx < 0 || (idx + MAX_COLOR > sizeof_maptbl(tbl))) {
		panel_err("invalid index %d\n", idx);
		return;
	}

	if (tbl->ncol != MAX_COLOR) {
		panel_err("invalid maptbl size %d\n", tbl->ncol);
		return;
	}

	for (i = 0; i < tbl->ncol; i++) {
		mdnie->props.cur_wrgb[i] = tbl->arr[idx + i];
		dst[i] = tbl->arr[idx + i];
		panel_dbg("cur_wrgb[%d] %d(%02X)\n",
				i, mdnie->props.cur_wrgb[i], mdnie->props.cur_wrgb[i]);
	}
}

static void copy_adjust_ldu_maptbl(struct maptbl *tbl, u8 *dst)
{
	struct mdnie_info *mdnie;
	int i, idx;
	u8 value;

	if (unlikely(!tbl || !dst))
		return;

	mdnie = (struct mdnie_info *)tbl->pdata;
	idx = maptbl_getidx(tbl);
	if (idx < 0 || (idx + MAX_COLOR > sizeof_maptbl(tbl))) {
		panel_err("invalid index %d\n", idx);
		return;
	}

	if (tbl->ncol != MAX_COLOR) {
		panel_err("invalid maptbl size %d\n", tbl->ncol);
		return;
	}

	for (i = 0; i < tbl->ncol; i++) {
		value = tbl->arr[idx + i] +
			(((mdnie->props.mode == AUTO) && (mdnie->props.scenario != EBOOK_MODE)) ?
				mdnie->props.def_wrgb_ofs[i] : 0);
		mdnie->props.cur_wrgb[i] = value;
		dst[i] = value;
		panel_dbg("cur_wrgb[%d] %d(%02X) (orig:0x%02X offset:%d)\n",
				i, mdnie->props.cur_wrgb[i], mdnie->props.cur_wrgb[i],
				tbl->arr[idx + i], mdnie->props.def_wrgb_ofs[i]);
	}
}

static int getidx_mdnie_maptbl(struct pkt_update_info *pktui, int offset)
{
	struct panel_device *panel = pktui->pdata;
	struct mdnie_info *mdnie = &panel->mdnie;
	int row = mdnie_get_maptbl_index(mdnie);
	int index;

	if (row < 0) {
		panel_err("invalid row %d\n", row);
		return -EINVAL;
	}

	index = row * mdnie->nr_reg + offset;
	if (index >= mdnie->nr_maptbl) {
		panel_err("exceeded index %d row %d offset %d\n",
				index, row, offset);
		return -EINVAL;
	}
	return index;
}

static int getidx_mdnie_0_maptbl(struct pkt_update_info *pktui)
{
	return getidx_mdnie_maptbl(pktui, 0);
}

static int getidx_mdnie_1_maptbl(struct pkt_update_info *pktui)
{
	return getidx_mdnie_maptbl(pktui, 1);
}

static int getidx_mdnie_2_maptbl(struct pkt_update_info *pktui)
{
	return getidx_mdnie_maptbl(pktui, 2);
}

static int getidx_mdnie_3_maptbl(struct pkt_update_info *pktui)
{
	return getidx_mdnie_maptbl(pktui, 3);
}

static int getidx_mdnie_4_maptbl(struct pkt_update_info *pktui)
{
	return getidx_mdnie_maptbl(pktui, 4);
}

static int getidx_mdnie_5_maptbl(struct pkt_update_info *pktui)
{
	return getidx_mdnie_maptbl(pktui, 5);
}

static int getidx_mdnie_scr_white_maptbl(struct pkt_update_info *pktui)
{
	struct panel_device *panel = pktui->pdata;
	struct mdnie_info *mdnie = &panel->mdnie;
	int index;

	if (mdnie->props.scr_white_mode < 0 ||
			mdnie->props.scr_white_mode >= MAX_SCR_WHITE_MODE) {
		panel_warn("out of range %d\n",
				mdnie->props.scr_white_mode);
		return -1;
	}

	if (mdnie->props.scr_white_mode == SCR_WHITE_MODE_COLOR_COORDINATE) {
		panel_dbg("coordinate maptbl\n");
		index = MDNIE_COLOR_COORDINATE_MAPTBL;
	} else if (mdnie->props.scr_white_mode == SCR_WHITE_MODE_ADJUST_LDU) {
		panel_dbg("adjust ldu maptbl\n");
		index = MDNIE_ADJUST_LDU_MAPTBL;
	} else if (mdnie->props.scr_white_mode == SCR_WHITE_MODE_SENSOR_RGB) {
		panel_dbg("sensor rgb maptbl\n");
		index = MDNIE_SENSOR_RGB_MAPTBL;
	} else {
		panel_dbg("empty maptbl\n");
		index = MDNIE_SCR_WHITE_NONE_MAPTBL;
	}

	return index;
}
#endif /* CONFIG_EXYNOS_DECON_MDNIE_LITE */

static void show_rddpm(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 rddpm[EA8079_RDDPM_LEN] = { 0, };
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	extern unsigned int g_rddpm;
#endif

	if (!res || ARRAY_SIZE(rddpm) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(rddpm, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddpm resource\n");
		return;
	}

	panel_info("========== SHOW PANEL [0Ah:RDDPM] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			rddpm[0], ((rddpm[0] & 0x9C) == 0x9C) ? "GOOD" : "NG");
	panel_info("* Bootster Mode : %s\n", rddpm[0] & 0x80 ? "ON (GD)" : "OFF (NG)");
	panel_info("* Idle Mode     : %s\n", rddpm[0] & 0x40 ? "ON (NG)" : "OFF (GD)");
	panel_info("* Partial Mode  : %s\n", rddpm[0] & 0x20 ? "ON" : "OFF");
	panel_info("* Sleep Mode    : %s\n", rddpm[0] & 0x10 ? "OUT (GD)" : "IN (NG)");
	panel_info("* Normal Mode   : %s\n", rddpm[0] & 0x08 ? "OK (GD)" : "SLEEP (NG)");
	panel_info("* Display ON    : %s\n", rddpm[0] & 0x04 ? "ON (GD)" : "OFF (NG)");
	panel_info("=================================================\n");
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	g_rddpm = (unsigned int)rddpm[0];
#endif
}

static void show_rddsm(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 rddsm[EA8079_RDDSM_LEN] = { 0, };
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	extern unsigned int g_rddsm;
#endif

	if (!res || ARRAY_SIZE(rddsm) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(rddsm, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy rddsm resource\n");
		return;
	}

	panel_info("========== SHOW PANEL [0Eh:RDDSM] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			rddsm[0], (rddsm[0] == 0x80) ? "GOOD" : "NG");
	panel_info("* TE Mode : %s\n", rddsm[0] & 0x80 ? "ON(GD)" : "OFF(NG)");
	panel_info("=================================================\n");
#ifdef CONFIG_LOGGING_BIGDATA_BUG
	g_rddsm = (unsigned int)rddsm[0];
#endif
}

static void show_err(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 err[EA8079_ERR_LEN] = { 0, }, err_15_8, err_7_0;

	if (!res || ARRAY_SIZE(err) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(err, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy err resource\n");
		return;
	}

	err_15_8 = err[0];
	err_7_0 = err[1];

	panel_info("========== SHOW PANEL [EAh:DSIERR] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x%02x, Result : %s\n", err_15_8, err_7_0,
			(err[0] || err[1] || err[2] || err[3] || err[4]) ? "NG" : "GOOD");

	if (err_15_8 & 0x80)
		panel_info("* DSI Protocol Violation\n");

	if (err_15_8 & 0x40)
		panel_info("* Data P Lane Contention Detetion\n");

	if (err_15_8 & 0x20)
		panel_info("* Invalid Transmission Length\n");

	if (err_15_8 & 0x10)
		panel_info("* DSI VC ID Invalid\n");

	if (err_15_8 & 0x08)
		panel_info("* DSI Data Type Not Recognized\n");

	if (err_15_8 & 0x04)
		panel_info("* Checksum Error\n");

	if (err_15_8 & 0x02)
		panel_info("* ECC Error, multi-bit (detected, not corrected)\n");

	if (err_15_8 & 0x01)
		panel_info("* ECC Error, single-bit (detected and corrected)\n");

	if (err_7_0 & 0x80)
		panel_info("* Data Lane Contention Detection\n");

	if (err_7_0 & 0x40)
		panel_info("* False Control Error\n");

	if (err_7_0 & 0x20)
		panel_info("* HS RX Timeout\n");

	if (err_7_0 & 0x10)
		panel_info("* Low-Power Transmit Sync Error\n");

	if (err_7_0 & 0x08)
		panel_info("* Escape Mode Entry Command Error");

	if (err_7_0 & 0x04)
		panel_info("* EoT Sync Error\n");

	if (err_7_0 & 0x02)
		panel_info("* SoT Sync Error\n");

	if (err_7_0 & 0x01)
		panel_info("* SoT Error\n");

	panel_info("* CRC Error Count : %d\n", err[2]);
	panel_info("* ECC1 Error Count : %d\n", err[3]);
	panel_info("* ECC2 Error Count : %d\n", err[4]);

	panel_info("==================================================\n");
}

static void show_err_fg(struct dumpinfo *info)
{
	int ret;
	u8 err_fg[EA8079_ERR_FG_LEN] = { 0, };
	struct resinfo *res = info->res;

	if (!res || ARRAY_SIZE(err_fg) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(err_fg, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy err_fg resource\n");
		return;
	}

	panel_info("========== SHOW PANEL [EEh:ERR_FG] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			err_fg[0], (err_fg[0] & 0x4C) ? "NG" : "GOOD");

	if (err_fg[0] & 0x04) {
		panel_info("* VLOUT3 Error\n");
		inc_dpui_u32_field(DPUI_KEY_PNVLO3E, 1);
	}

	if (err_fg[0] & 0x08) {
		panel_info("* ELVDD Error\n");
		inc_dpui_u32_field(DPUI_KEY_PNELVDE, 1);
	}

	if (err_fg[0] & 0x40) {
		panel_info("* VLIN1 Error\n");
		inc_dpui_u32_field(DPUI_KEY_PNVLI1E, 1);
	}

	panel_info("==================================================\n");
}

static void show_dsi_err(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 dsi_err[EA8079_DSI_ERR_LEN] = { 0, };

	if (!res || ARRAY_SIZE(dsi_err) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(dsi_err, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy dsi_err resource\n");
		return;
	}

	panel_info("========== SHOW PANEL [05h:DSIE_CNT] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			dsi_err[0], (dsi_err[0]) ? "NG" : "GOOD");
	if (dsi_err[0])
		panel_info("* DSI Error Count : %d\n", dsi_err[0]);
	panel_info("====================================================\n");

	inc_dpui_u32_field(DPUI_KEY_PNDSIE, dsi_err[0]);
}

static void show_self_diag(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 self_diag[EA8079_SELF_DIAG_LEN] = { 0, };

	ret = resource_copy(self_diag, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy self_diag resource\n");
		return;
	}

	panel_info("========== SHOW PANEL [0Fh:SELF_DIAG] INFO ==========\n");
	panel_info("* Reg Value : 0x%02x, Result : %s\n",
			self_diag[0], (self_diag[0] & 0x40) ? "GOOD" : "NG");
	if ((self_diag[0] & 0x80) == 0)
		panel_info("* OTP value is changed\n");
	if ((self_diag[0] & 0x40) == 0)
		panel_info("* Panel Boosting Error\n");

	panel_info("=====================================================\n");

	inc_dpui_u32_field(DPUI_KEY_PNSDRE, (self_diag[0] & 0x40) ? 0 : 1);
}

#ifdef CONFIG_SUPPORT_DDI_CMDLOG
static void show_cmdlog(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 cmdlog[EA8079_CMDLOG_LEN];

	memset(cmdlog, 0, sizeof(cmdlog));
	ret = resource_copy(cmdlog, res);
	if (unlikely(ret < 0)) {
		panel_err("failed to copy cmdlog resource\n");
		return;
	}

	panel_info("dump:cmdlog\n");
	print_data(cmdlog, ARRAY_SIZE(cmdlog));
}
#endif

static void show_self_mask_crc(struct dumpinfo *info)
{
	int ret;
	struct resinfo *res = info->res;
	u8 crc[EA8079_SELF_MASK_CRC_LEN] = {0, };

	if (!res || ARRAY_SIZE(crc) != res->dlen) {
		panel_err("invalid resource\n");
		return;
	}

	ret = resource_copy(crc, info->res);
	if (unlikely(ret < 0)) {
		panel_err("failed to self mask crc resource\n");
		return;
	}

	panel_info("======= SHOW PANEL [7Fh:SELF_MASK_CRC] INFO =======\n");
	panel_info("* Reg Value : 0x%02x, 0x%02x, 0x%02x, 0x%02x\n",
			crc[0], crc[1], crc[2], crc[3]);
	panel_info("====================================================\n");
}

static int init_gamma_mode2_brt_table(struct maptbl *tbl)
{
	struct panel_info *panel_data;
	struct panel_device *panel;
	struct panel_dimming_info *panel_dim_info;
	//todo:remove
	panel_info("++\n");
	if (tbl == NULL) {
		panel_err("maptbl is null\n");
		return -EINVAL;
	}

	if (tbl->pdata == NULL) {
		panel_err("pdata is null\n");
		return -EINVAL;
	}

	panel = tbl->pdata;
	panel_data = &panel->panel_data;

	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_DISP];

	if (panel_dim_info == NULL) {
		panel_err("panel_dim_info is null\n");
		return -EINVAL;
	}

	if (panel_dim_info->brt_tbl == NULL) {
		panel_err("panel_dim_info->brt_tbl is null\n");
		return -EINVAL;
	}

	generate_brt_step_table(panel_dim_info->brt_tbl);

	/* initialize brightness_table */
	memcpy(&panel->panel_bl.subdev[PANEL_BL_SUBDEV_TYPE_DISP].brt_tbl,
			panel_dim_info->brt_tbl, sizeof(struct brightness_table));

	return 0;
}

static int getidx_gamma_mode2_brt_table(struct maptbl *tbl)
{
	int row = 0;
	struct panel_info *panel_data;
	struct panel_bl_device *panel_bl;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	panel_bl = &panel->panel_bl;
	panel_data = &panel->panel_data;

	row = get_brightness_pac_step(panel_bl, panel_bl->props.brightness);

	return maptbl_index(tbl, 0, row, 0);
}

#ifdef CONFIG_DYNAMIC_FREQ

static int getidx_dyn_ffc_table(struct maptbl *tbl)
{
	int row = 0;
	struct df_status_info *status;
	struct panel_device *panel = (struct panel_device *)tbl->pdata;

	if (panel == NULL) {
		panel_err("panel is null\n");
		return -EINVAL;
	}
	status = &panel->df_status;

	row = status->ffc_df;
	if (row >= EA8079_MAX_MIPI_FREQ) {
		panel_warn("ffc out of range %d %d, set to %d\n", row,
			EA8079_MAX_MIPI_FREQ, EA8079_DEFAULT_MIPI_FREQ);
		row = status->ffc_df = EA8079_DEFAULT_MIPI_FREQ;
	}

	panel_info("ffc idx: %d, ddi_osc: %d, row: %d\n",
			status->ffc_df, status->current_ddi_osc, row);

	return maptbl_index(tbl, 0, row, 0);
}

#endif

static bool ea8079_is_120hz(struct panel_device *panel)
{
	return (getidx_ea8079_current_vrr_fps(panel) == EA8079_VRR_FPS_120);
}
/*
 * all_gamma_to_each_addr_table
 *
 * EA8079 gamma mtp register map is mess up.
 * Frist, read all register values, Make and sort to one big gamma table.
 * Do Gamma offset cal operation as one ALL big gamma table.
 * And then Split the table into mess up register map again.
 */

#if defined(__PANEL_NOT_USED_VARIABLE__)
static void gamma_to_reg_map_table(struct panel_device *panel,
			u8 *new_gamma, int fps_index, int br_index)
{

	int pos = 0;
	struct maptbl *target_maptbl = NULL;
	struct maptbl_pos target_pos = {0, };
	u8 *gamma_table_temp = NULL;
	struct panel_info *panel_data = NULL;

	panel_data = &panel->panel_data;
	gamma_table_temp = kzalloc(EA8079_GAMMA_MTP_ALL_LEN * 2, GFP_KERNEL);


	if (fps_index == EA8079_VRR_FPS_120) {
		/* 1. copy 120offset cal gamma */
		memcpy(gamma_table_temp + EA8079_GAMMA_MTP_ALL_LEN, new_gamma, EA8079_GAMMA_MTP_ALL_LEN);
		/* 2. copy orig 60 gamma */
		resource_copy_by_name(panel_data, gamma_table_temp, "gamma_mtp_60");
	} else {
		/* 1. copy orig 120 gamma */
		resource_copy_by_name(panel_data,
				gamma_table_temp + EA8079_GAMMA_MTP_ALL_LEN, "gamma_mtp_120");
		/* 2. copy 96/60/48 offset cal gamma */
		memcpy(gamma_table_temp, new_gamma, EA8079_GAMMA_MTP_ALL_LEN);
	}


	target_pos.index[NDARR_3D] = fps_index;
	target_pos.index[NDARR_2D] = br_index;
	pos = 0;

	target_maptbl = find_panel_maptbl_by_index(panel_data, GAMMA_MTP_0_HS_MAPTBL);
	maptbl_fill(target_maptbl, &target_pos, gamma_table_temp + pos, EA8079_GAMMA_MTP_0_LEN);
	pos += EA8079_GAMMA_MTP_0_LEN;
	target_maptbl = find_panel_maptbl_by_index(panel_data, GAMMA_MTP_1_HS_MAPTBL);
	maptbl_fill(target_maptbl, &target_pos, gamma_table_temp + pos, EA8079_GAMMA_MTP_1_LEN);
	pos += EA8079_GAMMA_MTP_1_LEN;
	target_maptbl = find_panel_maptbl_by_index(panel_data, GAMMA_MTP_2_HS_MAPTBL);
	maptbl_fill(target_maptbl, &target_pos, gamma_table_temp + pos, EA8079_GAMMA_MTP_2_LEN);
	pos += EA8079_GAMMA_MTP_2_LEN;
	target_maptbl = find_panel_maptbl_by_index(panel_data, GAMMA_MTP_3_HS_MAPTBL);
	maptbl_fill(target_maptbl, &target_pos, gamma_table_temp + pos, EA8079_GAMMA_MTP_3_LEN);
	pos += EA8079_GAMMA_MTP_3_LEN;


	kfree(gamma_table_temp);
}

static void gamma_itoc(u8 *dst, s32(*src)[MAX_COLOR], int nr_tp)
{
	unsigned int i, dst_pos = 0;

	if (nr_tp != EA8079_NR_TP) {
		panel_err("invalid nr_tp(%d)\n", nr_tp);
		return;
	}

	//11th
	dst[0] = src[0][RED] & 0xFF;
	dst[1] = src[0][GREEN] & 0xFF;
	dst[2] = src[0][BLUE] & 0xFF;

	dst_pos = 3;

	//10th ~ 1st
	for (i = 1; i < nr_tp; i++) {
		dst_pos = (i - 1) * 4 + 3;
		dst[dst_pos] = (src[i][RED]>>4 & 0x3F);
		dst[dst_pos + 1] = (src[i][RED]<<4 & 0xF0) | (src[i][GREEN]>>6  & 0x0F);
		dst[dst_pos + 2] = (src[i][GREEN]<<2 & 0xFC) | (src[i][BLUE]>>8  & 0x03);
		dst[dst_pos + 3] = (src[i][BLUE]<<0 & 0xFF);
	}
}


static int gamma_ctoi(s32 (*dst)[MAX_COLOR], u8 *src, int nr_tp)
{
	unsigned int i, pos = 0;

	//11th
	dst[0][RED] = src[pos];
	dst[0][GREEN] = src[pos + 1];
	dst[0][BLUE] = src[pos + 2];

	//10th ~ 1st
	for (i = 1 ; i < nr_tp ; i++) {
		pos = (i - 1) * 4 + 3;
		dst[i][RED] = (src[pos] & 0x3F)<<4 | (src[pos+1] & 0xF0)>>4;
		dst[i][GREEN] = (src[pos+1] & 0x0F)<<6 | (src[pos+2] & 0xFC)>>2;
		dst[i][BLUE] = (src[pos+2] & 0x03)<<8 | (src[pos+3] & 0xFF)>>0;
	}

	return 0;
}

static int gamma_sum(s32 (*dst)[MAX_COLOR], s32 (*src)[MAX_COLOR],
		s32 (*offset)[MAX_COLOR], int nr_tp)
{
	unsigned int i, c;
	int upper_limit[EA8079_NR_TP] = {
		0xFF, 0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3FF
	};

	if (nr_tp != EA8079_NR_TP) {
		panel_err("invalid nr_tp(%d)\n", nr_tp);
		return -EINVAL;
	}

	for (i = 0; i < nr_tp; i++) {
		for_each_color(c)
			dst[i][c] =
			min(max(src[i][c] + offset[i][c], 0), upper_limit[i]);
	}

	return 0;
}

static int  do_gamma_update(struct panel_device *panel, int fps_index, int br_index)
{
	int src_gamma = 0;
	int ret;
	struct panel_info *panel_data;
	struct gm2_dimming_lut *dim_lut;
	struct panel_dimming_info *panel_dim_info;
	u8 gamma_8_org[EA8079_GAMMA_MTP_1SET_LEN];
	u8 gamma_8_new[EA8079_GAMMA_MTP_1SET_LEN];
	s32 gamma_32_org[EA8079_NR_TP][MAX_COLOR];
	s32 gamma_32_new[EA8079_NR_TP][MAX_COLOR];
	u8 gamma_all_org[EA8079_GAMMA_MTP_ALL_LEN];
	s32(*rgb_color_offset)[MAX_COLOR];
	int gamma_index = 0;
	u8 gamma_8_new_set[EA8079_GAMMA_MTP_1SET_LEN * EA8079_GAMMA_MTP_SET_NUM];

	panel_data = &panel->panel_data;
	panel_dim_info = panel_data->panel_dim_info[PANEL_BL_SUBDEV_TYPE_DISP];

	if (unlikely(!panel_dim_info)) {
		panel_err("panel_bl-%d panel_dim_info is null\n",
				PANEL_BL_SUBDEV_TYPE_DISP);
		return -EINVAL;
	}

	if (panel_dim_info->nr_gm2_dim_init_info == 0) {
		panel_err("panel_bl-%d gammd mode 2 init info is null\n",
				PANEL_BL_SUBDEV_TYPE_DISP);
		return -EINVAL;
	}

	dim_lut = panel_dim_info->gm2_dim_init_info[0].dim_lut;

	for (gamma_index = 0 ; gamma_index < EA8079_GAMMA_MTP_SET_NUM; gamma_index++) {
		/* select source */
		src_gamma =
			dim_lut[MAX_EA8079_GAMMA_MTP * MAX_EA8079_GAMMA_BR_INDEX * fps_index
				+ MAX_EA8079_GAMMA_MTP * br_index
				+ (EA8079_GAMMA_MTP_SET_NUM - gamma_index - 1)].source_gamma;

		if (!dim_lut)
			panel_err("no dim_lut\n");

		ret = resource_copy_by_name(panel_data,
				gamma_all_org, src_gamma == EA8079_VRR_FPS_120 ? "gamma_mtp_120" : "gamma_mtp_60");


		/* copy 1 row gamma from all gamma table */
		memcpy(gamma_8_org, gamma_all_org + EA8079_GAMMA_MTP_1SET_LEN * gamma_index,
				EA8079_GAMMA_MTP_1SET_LEN);

		/* byte to int */
		gamma_ctoi(gamma_32_org, gamma_8_org, EA8079_NR_TP);

		/* add offset */
		rgb_color_offset =
			dim_lut[MAX_EA8079_GAMMA_MTP * MAX_EA8079_GAMMA_BR_INDEX * fps_index
				+ MAX_EA8079_GAMMA_MTP * br_index
				+ (EA8079_GAMMA_MTP_SET_NUM - gamma_index - 1)].rgb_color_offset;
		gamma_sum(gamma_32_new, gamma_32_org,
				rgb_color_offset, EA8079_NR_TP);
		/* int to byte */
		gamma_itoc(gamma_8_new, gamma_32_new, EA8079_NR_TP);

		memcpy(gamma_8_new_set + EA8079_GAMMA_MTP_1SET_LEN * gamma_index, gamma_8_new,
					EA8079_GAMMA_MTP_1SET_LEN);
	}

	gamma_to_reg_map_table(panel, gamma_8_new_set, fps_index, br_index);

	return ret;
}

static int init_gamma_mtp_table(struct maptbl *tbl)
{
	struct panel_device *panel;
	int ret = 0;
	int br_index = 0;
	int vrr_fps_index = 0;

	if (unlikely(!tbl || !tbl->pdata)) {
		panel_err("panel_bl-%d invalid param (tbl %p, pdata %p)\n",
				PANEL_BL_SUBDEV_TYPE_DISP, tbl, tbl ? tbl->pdata : NULL);
		return -EINVAL;
	}

	panel = tbl->pdata;

	for (vrr_fps_index = 0 ; vrr_fps_index < MAX_EA8079_VRR_FPS; vrr_fps_index++) {
		for (br_index = 0 ; br_index < MAX_EA8079_GAMMA_BR_INDEX; br_index++) {
				do_gamma_update(panel, vrr_fps_index, br_index);
		}
	}

	panel_dbg("%s initialize\n", tbl->name);

	return ret;
}


static int init_gamma_mtp_all_table(struct maptbl *tbl)
{
	return init_gamma_mtp_table(tbl);
}
#endif

static bool is_panel_state_not_lpm(struct panel_device *panel)
{
	if (panel->state.cur_state != PANEL_STATE_ALPM)
		return true;

	return false;
}