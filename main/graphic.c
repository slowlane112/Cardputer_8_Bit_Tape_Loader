/*
 * SPDX-FileCopyrightText: 2026 slowlane112
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
#include <stdio.h>
#include <string.h>
#include "pixel_font.h"
#include "display.h"
#include "graphic.h"

static void display_font_pixel(int x_pos, int y_pos, uint16_t color) 
{
	
	for (int i = 0; i < 1; i++) {
		for (int j = 0; j < 2; j++) {
			framebuffer[x_pos + i + y_pos + (j * DISPLAY_WIDTH)] = color;
		}
	}
	
}

void graphic_display_text(const char *text, int y_start, int x_start, uint16_t text_fg_color, uint16_t text_bg_color) 
{
	
	uint8_t pixel_font_data = 0;
    uint8_t pixel_font_pos = 0;
    int x_pos = 0;
    int y_pos = 0;
    int text_pos = 0;
	
	for (int i = 0; text[i] != '\0'; i++) {
			
		pixel_font_pos = (int)text[i];
	
		for (int j = 0; j < 8; j++) {
		
			pixel_font_data = pixel_font_get_data((pixel_font_pos * 8) + j);
		
			for (int k = 7; k >= 0; k--) {

				x_pos = x_start + (text_pos * 8) + (7 - k);
				y_pos = (y_start * DISPLAY_WIDTH) + ((j * 2) * DISPLAY_WIDTH);
					
				display_font_pixel(x_pos, y_pos, ((pixel_font_data >> k) & 1) ? text_fg_color : text_bg_color);
				
			}
		
		}
		
		text_pos++;
	}
	
}

void graphic_display_loading_screen() 
{
	
	for (int i = 0; i < DISPLAY_WIDTH * DISPLAY_HEIGHT; i++) {
		framebuffer[i] = BG_COLOR;
	}
	
	graphic_display_text("Loading...", (DISPLAY_HEIGHT / 2) - 8, (DISPLAY_WIDTH / 2) - (5 * 8), LABEL_COLOR, BG_COLOR);
	
	display_draw();
}

void graphic_display_invalid_file_screen(const char *text) 
{
	for (int y = DISPLAY_HEIGHT - FOOTER_HEIGHT; y < DISPLAY_HEIGHT; y++) {
		
		for (int x = 0; x < DISPLAY_WIDTH; x++) {
		
			framebuffer[(y * DISPLAY_WIDTH) + x] = BG_COLOR;
		}
		
	}
	
	int pos_x = 4;
	int pos_y = 30;
		
	graphic_display_text(text, pos_y, pos_x, LABEL_COLOR, BG_COLOR);
	
	pos_y = pos_y + 20;
	
	graphic_display_text("The selected file is invalid.", pos_y, pos_x, LABEL_COLOR, BG_COLOR);
	
}

void draw_header(const char *text) {
	
	for (int y = 0; y < HEADER_HEIGHT; y++) {
		
		for (int x = 0; x < DISPLAY_WIDTH; x++) {
	
			framebuffer[(y * DISPLAY_WIDTH) + x] = HEADER_BG_COLOR;
	
		}

	}
	
	int text_y_start = 2;
	int text_x_start = 4;
	
	graphic_display_text(text, text_y_start, text_x_start, HEADER_LABEL_COLOR, HEADER_BG_COLOR);
	
}

void draw_footer(graphic_footer_button_t *btn1, graphic_footer_button_t *btn2, graphic_footer_button_t *btn3) {
	
	int unit = DISPLAY_WIDTH / 3;
	int half_unit = unit / 2;
	
    for (int y = DISPLAY_HEIGHT - FOOTER_HEIGHT; y < DISPLAY_HEIGHT; y++) {
		
		for (int x = 0; x < DISPLAY_WIDTH; x++) {
			
			if (x == unit || x == (2 * unit)) {
				framebuffer[(y * DISPLAY_WIDTH) + x] = FOOTER_LABEL_COLOR;
			}
			else if (x < unit) {
				framebuffer[(y * DISPLAY_WIDTH) + x] = btn1->bg_color;
			}
			else if (x < (2 * unit)) {
				framebuffer[(y * DISPLAY_WIDTH) + x] = btn2->bg_color;
			}
			else {
				framebuffer[(y * DISPLAY_WIDTH) + x] = btn3->bg_color;
			}
			
		}
		
	}
	
	int text_y_start = DISPLAY_HEIGHT - 18;

	int text_x_start = half_unit - ((strlen(btn1->text) * 8) / 2);
	graphic_display_text(btn1->text, text_y_start, text_x_start, btn1->fg_color, btn1->bg_color);
	
	text_x_start = unit + half_unit - ((strlen(btn2->text) * 8) / 2);
	graphic_display_text(btn2->text, text_y_start, text_x_start, btn2->fg_color, btn2->bg_color);
	
	text_x_start = (2 * unit) + half_unit - ((strlen(btn3->text) * 8) / 2);
	graphic_display_text(btn3->text, text_y_start, text_x_start, btn3->fg_color, btn3->bg_color);

}

void graphic_draw_status_indicator(const char *text, bool status, int x_pos, int y_pos, uint16_t on_color, uint16_t off_color) {
	
	int indicator_width = 16;
	int indicator_height = 16;
	
	for (int y = y_pos; y < y_pos + indicator_height; y++) {
		
		for (int x = x_pos; x < x_pos + indicator_width; x++) {
			
			if (y == y_pos 
				|| y == (y_pos + indicator_height - 1)
				|| x == x_pos
				|| x == (x_pos + indicator_width - 1)
				
				)
			{
				// border
				framebuffer[(y * DISPLAY_WIDTH) + x] = LABEL_COLOR;

			}
			else {
		
				if (status) {
				
					framebuffer[(y * DISPLAY_WIDTH) + x] = on_color;
				}
				else {
					framebuffer[(y * DISPLAY_WIDTH) + x] = off_color;
				}
				
			}
		}
		
	}
	
	graphic_display_text(text, y_pos, x_pos + 20, LABEL_COLOR, BG_COLOR);
	
}

void graphic_draw_progress_bar(size_t pos, size_t total, int x_pos, int y_pos, uint16_t on_color, uint16_t off_color) {
	
	int progress_bar_height = 16;
	
	char buf_percentage[32]; 

	snprintf(buf_percentage, sizeof(buf_percentage), "%u%%", total > 0 ? (unsigned)((pos * 100ULL) / total) : 0);
	
	size_t percentage_len = strlen(buf_percentage);

	int progress_bar_width = DISPLAY_WIDTH - 4 - (8 * (percentage_len + 1));
	int progress_bar_size = pos == 0 ? 0 : (pos * progress_bar_width) / total;
	
	for (int y = y_pos; y < y_pos + progress_bar_height; y++) {
		
		for (int x = x_pos; x < x_pos + progress_bar_width; x++) {
			
			if (y == y_pos 
				|| y == (y_pos + progress_bar_height - 1)
				|| x == x_pos
				|| x == (x_pos + progress_bar_width - 1)
				
				)
			{
				// border
				framebuffer[(y * DISPLAY_WIDTH) + x] = LABEL_COLOR;

			}
			else {
				
				if (x < x_pos + progress_bar_size) {
			
					framebuffer[(y * DISPLAY_WIDTH) + x] = on_color;
				}
				else {
					framebuffer[(y * DISPLAY_WIDTH) + x] = off_color;
				}
				
			}
			
		}

	}
	
	int pos_x = DISPLAY_WIDTH - 4 - (8 * percentage_len);
	
	graphic_display_text(buf_percentage, y_pos, pos_x, LABEL_COLOR, BG_COLOR);
	
}

