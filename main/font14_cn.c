/*******************************************************************************
 * Size: 14 px
 * Bpp: 1
 * Opts: --bpp 1 --size 14 --lcd --use-color-info --stride 1 --align 1 --font 文泉驿正黑字体.ttf --symbols 主人小狗小西几剩余时间开锁蓝牙锁上解锁： --format lvgl -o font14_cn.c
 ******************************************************************************/

#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif



#ifndef FONT14_CN
#define FONT14_CN 1
#endif

#if FONT14_CN

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+4E0A "上" */
    0x4, 0x0, 0x20, 0x1, 0x0, 0x8, 0x0, 0x7e,
    0x2, 0x0, 0x10, 0x0, 0x80, 0x4, 0x0, 0x20,
    0x1, 0x0, 0x8, 0xf, 0xff, 0x80,

    /* U+4E3B "主" */
    0x4, 0x0, 0x10, 0x0, 0x0, 0xff, 0xe0, 0x20,
    0x1, 0x0, 0x8, 0x7, 0xfc, 0x2, 0x0, 0x10,
    0x0, 0x80, 0x4, 0xf, 0xff, 0x80,

    /* U+4EBA "人" */
    0x4, 0x0, 0x20, 0x1, 0x0, 0x8, 0x0, 0x40,
    0x2, 0x0, 0x28, 0x1, 0x40, 0x11, 0x0, 0x84,
    0x8, 0x10, 0x80, 0x78, 0x1, 0x0,

    /* U+4F59 "余" */
    0x2, 0x0, 0x28, 0x2, 0x20, 0x20, 0x82, 0xfa,
    0x61, 0xc, 0x8, 0xf, 0xfe, 0x2, 0x0, 0x92,
    0x8, 0x88, 0x94, 0x20, 0x40, 0x0,

    /* U+51E0 "几" */
    0x1f, 0x80, 0x84, 0x4, 0x20, 0x21, 0x1, 0x8,
    0x8, 0x40, 0x42, 0x2, 0x10, 0x20, 0x89, 0x4,
    0x50, 0x23, 0x0, 0xf0,

    /* U+5269 "剩" */
    0x7, 0xb, 0xc0, 0x42, 0x13, 0xfe, 0x92, 0xa4,
    0xf5, 0xa4, 0xa9, 0x3d, 0x69, 0x1c, 0x49, 0x52,
    0x52, 0x43, 0x11, 0x50, 0x81, 0x0,

    /* U+5C0F "小" */
    0x2, 0x0, 0x10, 0x0, 0x80, 0x4, 0x2, 0x24,
    0x11, 0x10, 0x88, 0x48, 0x41, 0x82, 0x8, 0x10,
    0x0, 0x80, 0x14, 0x0, 0x40, 0x0,

    /* U+5F00 "开" */
    0x3f, 0xe0, 0x44, 0x2, 0x20, 0x11, 0x0, 0x88,
    0x7f, 0xfc, 0x22, 0x1, 0x10, 0x10, 0x80, 0x84,
    0x8, 0x20, 0x81, 0x0,

    /* U+65F6 "时" */
    0x0, 0x27, 0x81, 0x24, 0x9, 0x3f, 0xf9, 0x2,
    0x79, 0x12, 0x44, 0x92, 0x24, 0x90, 0x27, 0x81,
    0x24, 0x8, 0x1, 0x40, 0x4, 0x0,

    /* U+7259 "牙" */
    0xff, 0xf0, 0x10, 0x21, 0x4, 0x10, 0x7f, 0xf0,
    0x10, 0x5, 0x0, 0x90, 0x11, 0x2, 0x10, 0xc1,
    0x0, 0x50, 0x2, 0x0,

    /* U+72D7 "狗" */
    0x92, 0x2, 0x90, 0x8, 0xfe, 0xa8, 0x19, 0x80,
    0x89, 0xe4, 0xc9, 0x2a, 0x49, 0x93, 0xc8, 0x80,
    0x44, 0x2, 0xa0, 0x92, 0x3, 0x0,

    /* U+84DD "蓝" */
    0x10, 0x87, 0xff, 0xc4, 0x20, 0x12, 0x4, 0x9f,
    0x25, 0x41, 0x31, 0x81, 0x0, 0x7f, 0xf2, 0x48,
    0x92, 0x44, 0x92, 0x2f, 0xff, 0x80,

    /* U+897F "西" */
    0xff, 0xf8, 0x48, 0x2, 0x40, 0xff, 0xe4, 0x91,
    0x24, 0x89, 0x44, 0x4c, 0x1e, 0x40, 0x12, 0x0,
    0x9f, 0xfc, 0x80, 0x20,

    /* U+89E3 "解" */
    0x20, 0x3, 0xef, 0xa2, 0x24, 0x21, 0x27, 0xd1,
    0x2b, 0x19, 0xf5, 0xa, 0xbe, 0x7e, 0x42, 0xa2,
    0x15, 0xff, 0x8, 0x88, 0xc4, 0x0,

    /* U+9501 "锁" */
    0x42, 0x4a, 0xa, 0x9e, 0x11, 0x7, 0xff, 0xa0,
    0x91, 0x27, 0xe9, 0x24, 0x49, 0x22, 0x49, 0x45,
    0xc, 0x44, 0x44, 0x10, 0xc0, 0x80,

    /* U+95F4 "间" */
    0x4f, 0xf2, 0x1, 0x80, 0x19, 0xf9, 0x90, 0x99,
    0x9, 0x9f, 0x99, 0x9, 0x90, 0x99, 0xf9, 0x80,
    0x18, 0x5, 0x80, 0x20,

    /* U+FF1A "：" */
    0xf0, 0x3c
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 224, .box_w = 13, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 22, .adv_w = 224, .box_w = 13, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 44, .adv_w = 224, .box_w = 13, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 66, .adv_w = 224, .box_w = 13, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 88, .adv_w = 224, .box_w = 13, .box_h = 12, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 108, .adv_w = 224, .box_w = 13, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 130, .adv_w = 224, .box_w = 13, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 152, .adv_w = 224, .box_w = 13, .box_h = 12, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 172, .adv_w = 224, .box_w = 13, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 194, .adv_w = 224, .box_w = 12, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 214, .adv_w = 224, .box_w = 13, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 236, .adv_w = 224, .box_w = 13, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 258, .adv_w = 224, .box_w = 13, .box_h = 12, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 278, .adv_w = 224, .box_w = 13, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 300, .adv_w = 224, .box_w = 13, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 322, .adv_w = 224, .box_w = 12, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 342, .adv_w = 224, .box_w = 2, .box_h = 7, .ofs_x = 6, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0x31, 0xb0, 0x14f, 0x3d6, 0x45f, 0xe05, 0x10f6,
    0x17ec, 0x244f, 0x24cd, 0x36d3, 0x3b75, 0x3bd9, 0x46f7, 0x47ea,
    0xb110
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 19978, .range_length = 45329, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 17, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif

};

extern const lv_font_t font14_cn;


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t font14_cn = {
#else
lv_font_t font14_cn = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 13,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_HOR,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -3,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc           /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
};



#endif /*#if FONT14_CN*/
