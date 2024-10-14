#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <getopt.h>
#include <sys/time.h>

extern "C" {
#include "types.h"
#include "hwdefs.h"
#include "emu.h"
#include "state.h"
#include "cpu.h"
#include "mmu.h"
#include "lcd.h"
#include "audio.h"
#include "disassembler.h"
#include "debugger.h"
#include "gui.h"
#include "fileio.h"
}

#include "pax/fb.h"
#include "pax/keypad.h"
#include "pax/touchscreen.h"
#include "pax/sound.h"

#define GUI_WINDOW_TITLE "KoenGB"
#define GUI_ZOOM      4

PAXFramebuffer fb;
PAXKeypad kp;
PAXTouchscreen ts;
PAXSound snd;
uint8_t* audio_sndbuf;
int16_t audio_outbuf[AUDIO_SNDBUF_SIZE * AUDIO_CHANNELS];
int gui_audio_init(int sample_rate, int channels, size_t sndbuf_size,
        uint8_t *sndbuf) {
    snd.setSamplerate(sample_rate);
    audio_sndbuf = sndbuf;
    return 0;
}


int gui_lcd_init(int width, int height, int zoom, char *wintitle) {
    return 0;
}


void gui_lcd_render_frame(char use_colors, uint16_t *pixbuf) {

    if (use_colors) {
        /* The colors stored in pixbuf are two byte each, 5 bits per rgb
         * component: -bbbbbgg gggrrrrr. We need to extract these, scale these
         * values from 0-1f to 0-ff and put them in RGBA format. For the scaling
         * we'd have to multiply by 0xff/0x1f, which is 8.23, approx 8, which is
         * a shift by 3. */
        for (int y_scr = 0; y_scr < 216; y_scr++) {
            for (int x_scr = 0;x_scr < 240;x_scr++) {
                int y = y_scr * 144 / 216;
                int x = x_scr * 160 / 240;
                int idx = x + y * GB_LCD_WIDTH;
                uint16_t rawcol = pixbuf[idx];
                uint32_t r = ((rawcol >>  0) & 0x1f) << 3;
                uint32_t g = ((rawcol >>  5) & 0x1f) << 3;
                uint32_t b = ((rawcol >> 10) & 0x1f) << 3;
                fb.pixel(x_scr, 268 - y_scr) = rgb16(r, g, b);
            }
        }
    } else {
        /* The colors stored in pixbuf already went through the palette
         * translation, but are still 2 bit monochrome. */
        uint32_t palette[] = { 0xffffffff, 0xaaaaaaaa, 0x66666666, 0x11111111 };
        for (int y_scr = 0; y_scr < 216; y_scr++) {
            for (int x_scr = 0;x_scr < 240;x_scr++) {
                int y = y_scr * 144 / 216;
                int x = x_scr * 160 / 240;
                int idx = x + y * GB_LCD_WIDTH;
                fb.pixel(x_scr, 268 - y_scr + 52) = palette[pixbuf[idx]];
            }
        }
    }
}

int gui_input_poll(struct player_input *input) {
    input->special_quit = 0;
    input->special_savestate = 0;
    input->special_dbgbreak = 0;

    //memset(input, 0, sizeof(struct player_input));
    KeyCode key = kp.getKey();
    switch (key) {
    case KEY_ALPHA: input->button_start = 1; break;
    case KEY_FUNC: input->button_select = 1; break;
    case KEY_9: input->button_b = 1; break;
    case KEY_3: input->button_a = 1; break;
    case KEY_8: input->button_down = 1; break;
    case KEY_2: input->button_up = 1; break;
    case KEY_4: input->button_left = 1; break;
    case KEY_6: input->button_right = 1; break;
    case KEY_ESC: input->special_quit = 1; break;
    default: break;
    }
    return 0;
}


__attribute__((constructor))
int app_start() {
    struct gb_state gb_state;

    struct emu_args emu_args;

    emu_args = {
        .rom_filename = "/data/app/MAINAPP/lib/rom.gbc",
        .bios_filename = NULL,
        .state_filename = NULL,
        .save_filename = NULL,
        .break_at_start = 0,
        .print_disas = 0,
        .print_mmu = 0,
        .audio_enable = 1,
    };

    if (emu_init(&gb_state, &emu_args)) {
        fprintf(stderr, "Initialization failed\n");
        return 1;
    }

    /* Initialize frontend-specific GUI */
    if (gui_lcd_init(GB_LCD_WIDTH, GB_LCD_HEIGHT, GUI_ZOOM, GUI_WINDOW_TITLE)) {
        fprintf(stderr, "Couldn't initialize GUI LCD\n");
        return 1;
    }
    if (emu_args.audio_enable) {
        if (gui_audio_init(AUDIO_SAMPLE_RATE, AUDIO_CHANNELS, AUDIO_SNDBUF_SIZE,
                    gb_state.emu_state->audio_sndbuf)) {
            fprintf(stderr, "Couldn't initialize GUI audio\n");
            return 1;
        }
    }

    printf("==========================\n");
    printf("=== Starting execution ===\n");
    printf("==========================\n\n");

    struct timeval starttime, endtime;
    gettimeofday(&starttime, NULL);


    while (!gb_state.emu_state->quit) {
        emu_step_frame(&gb_state);

        struct player_input input_state;
        memset(&input_state, 0, sizeof(struct player_input));
        gui_input_poll(&input_state);
        emu_process_inputs(&gb_state, &input_state);

        gui_lcd_render_frame(gb_state.gb_type == GB_TYPE_CGB,
                gb_state.emu_state->lcd_pixbuf);

        if (gb_state.emu_state->audio_enable) /* TODO */
            audio_update(&gb_state);
        for(int i = 0;i < AUDIO_SNDBUF_SIZE * AUDIO_CHANNELS;i++) {
            audio_outbuf[i] = audio_sndbuf[i] * 16;
        }
        snd.playSound(audio_outbuf, sizeof(int16_t) * AUDIO_SNDBUF_SIZE * AUDIO_CHANNELS);
    }

    if (gb_state.emu_state->extram_dirty)
        emu_save(&gb_state, 1, gb_state.emu_state->save_filename_out);

    gettimeofday(&endtime, NULL);

    printf("\nEmulation ended at instr: ");
    disassemble(&gb_state);
    dbg_print_regs(&gb_state);

    int t_usec = endtime.tv_usec - starttime.tv_usec;
    int t_sec = endtime.tv_sec - starttime.tv_sec;
    double exectime = t_sec + (t_usec / 1000000.);

    double emulated_secs = gb_state.emu_state->time_seconds +
        gb_state.emu_state->time_cycles / 4194304.;

    printf("\nEmulated %f sec in %f sec WCT, %.0f%%.\n", emulated_secs, exectime,
            emulated_secs / exectime * 100);

    return 0;
}
