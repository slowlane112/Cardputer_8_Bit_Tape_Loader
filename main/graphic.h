/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#ifndef GRAPHIC_H
#define GRAPHIC_H

#define HEADER_HEIGHT	20
#define FOOTER_HEIGHT	20

typedef struct {
    const char *text;
    uint16_t fg_color;
    uint16_t bg_color;
} graphic_footer_button_t;

static const uint16_t LABEL_COLOR = 0xFFFF;
static const uint16_t BG_COLOR = 0x1F54;
static const uint16_t HEADER_BG_COLOR = 0xDF01;
static const uint16_t HEADER_LABEL_COLOR = 0xFFFF;
static const uint16_t FOOTER_BG_COLOR = 0xDF01;
static const uint16_t FOOTER_LABEL_COLOR = 0xFFFF;
static const uint16_t FOLDER_COLOR = 0xC0FF;
static const uint16_t DISABLED_LABEL_COLOR = 0x1800;

static const uint16_t BUTTON_PLAY_ON_BG_COLOR = 0xE846;
static const uint16_t BUTTON_DISABLED_LABEL_COLOR = 0x1800;
static const uint16_t BUTTON_STOP_STOPPING_LABEL_COLOR = 0x00F8;

static const uint16_t INDICATOR_OFF_COLOR = 0x1F54;
static const uint16_t INDICATOR_DATA_COLOR = 0xE846;
static const uint16_t INDICATOR_MOTOR_COLOR = 0x00F8;
static const uint16_t INDICATOR_REMOTE_COLOR = 0x00F8;

static const uint16_t PROGRESS_BAR_ON_COLOR = 0xE846;
static const uint16_t PROGRESS_BAR_OFF_COLOR = 0x00F8;

void graphic_display_text(const char *text, int y_start, int x_start, uint16_t text_fg_color, uint16_t text_bg_color);
void graphic_display_loading_screen();
void draw_header(const char *text);
void draw_footer(graphic_footer_button_t *btn1, graphic_footer_button_t *btn2, graphic_footer_button_t *btn3);
void graphic_draw_status_indicator(const char *text, bool status, int x_pos, int y_pos, uint16_t on_color, uint16_t off_color);
void graphic_draw_progress_bar(size_t pos, size_t total, int x_pos, int y_pos, uint16_t on_color, uint16_t off_color);
void graphic_display_invalid_file_screen(const char *text);

#endif

