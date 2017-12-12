#include <assert.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

#include "gui.h"

static SDL_Renderer *renderer;
static SDL_Texture *texture;

int gui_init(void) {
    SDL_Window *window;

    if (SDL_Init(SDL_INIT_VIDEO)) {
        printf("LCD: SDL failed to initialize: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow(GUI_WINDOW_TITLE,
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            GUI_PX_WIDTH * GUI_ZOOM, GUI_PX_HEIGHT * GUI_ZOOM, 0);
    if (!window){
        printf("LCD: SDL could not create window: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, 0);
    if (!window){
        printf("LCD: SDL could not create renderer: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, GUI_PX_WIDTH, GUI_PX_HEIGHT);
    if (!texture){
        printf("LCD: SDL could not create screen texture: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    return 0;
}

void gui_render_frame(struct gb_state *gb_state) {
    u32 *pixels = NULL;
    int pitch;
    if (SDL_LockTexture(texture, NULL, (void*)&pixels, &pitch)) {
        printf("LCD: SDL could not lock screen texture: %s\n", SDL_GetError());
        exit(1);
    }

    /*
     * Tile Data @ 8000-8FFF or 8800-97FF defines the pixels per Tile, which can
     * be used for the BG, window or sprite/object. 192 tiles max, 8x8px, 4
     * colors. Foreground tiles (sprites/objects) may only have 3 colors (0
     * being transparent). Each tile thus is 16 byte, 2 byte per line (2 bit per
     * px), first byte is lsb of color, second byte msb of color.
     *
     *
     * BG Map @ 9800-9BFF or 9C00-9FFF. 32 rows of 32 bytes each, each byte is
     * number of tile to be displayed (index into Tile Data, see below
     *
     * Window works similarly to BG
     *
     * Sprites or objects come from the Sprite Attribute table (OAM: Object
     * Attribute Memory) @ FE00-FE9F, 40 entries of 4 byte (max 10 per hline).
     *  byte 0: Y pos - 16
     *  byte 1: X pos - 8
     *  byte 2: Tile number, index into Tile data (see above)
     *
     */
    for (int y = 0; y < GUI_PX_HEIGHT; y++)
        SDL_memset(&pixels[y * GUI_PX_WIDTH], y, GUI_PX_WIDTH * 4);

    u32 palette[] = { 0xffffffff, 0xaaaaaaaa, 0x66666666, 0x11111111 };

    /* TODO for all things: palette? */

    /* First render background tiles. */
    /* TODO: selecting other tile data area */
    u8 scroll_x = gb_state->io_lcd_SCX;
    u8 scroll_y = gb_state->io_lcd_SCY;

    u8 tilemap_low_unsiged = (gb_state->io_lcd_LCDC & (1<<4)) ? 1 : 0;
    u8 bgmap_high = (gb_state->io_lcd_LCDC & (1<<3)) ? 1 : 0;

    u8 *tiledata;
    if (tilemap_low_unsiged)
        tiledata = &gb_state->mem_VRAM[0x8000-0x8000];
    else /* Spans from 8800-97FF, tileidx signed */
        tiledata = &gb_state->mem_VRAM[0x9000-0x8000];
    u8 *bgmap;
    if (bgmap_high)
        bgmap = &gb_state->mem_VRAM[0x9c00-0x8000];
    else
        bgmap = &gb_state->mem_VRAM[0x9800-0x8000];
    for (int tiley = 0; tiley < 32; tiley++) {
        for (int tilex = 0; tilex < 32; tilex++) {
            u8 tileidxraw = bgmap[tilex + tiley*32];
            s16 tileidx = tilemap_low_unsiged ? (s16)(u16)tileidxraw :
                                                (s16)(s8)tileidxraw;
            for (int y = 0; y < 8; y++) {
                int screen_y = (tiley * 8 + y - scroll_y) % 256;
                if (screen_y < 0) screen_y += 256;
                if (screen_y >= GUI_PX_HEIGHT) continue;
                for (int x = 0; x < 8; x++) {
                    int screen_x = (tilex * 8 + x - scroll_x) % 256;
                    if (screen_x < 0) screen_x += 256;
                    if (screen_x >= GUI_PX_WIDTH) continue;

                    /* We have packed 2-bit color indices here, so the bits look
                     * like: (each bit denoted by the pixel index in tile)
                     *  01234567 01234567 89abcdef 89abcdef ...
                     * So for the 9th pixel (which is px 1,1) we need bytes 2+3
                     * (9/8*2 [+1]) and then shift both by 7 (8-9%8)
                     */
                    int i = x + y * 8;
                    int shift = 7 - i % 8;
                    u8 b1 = tiledata[tileidx * 16 + i/8*2];
                    u8 b2 = tiledata[tileidx * 16 + i/8*2 + 1];
                    u8 colidx = ((b1 >> shift) & 1) |
                               (((b2 >> shift) & 1) << 1);

                    u32 col = palette[colidx];
                    pixels[screen_x + screen_y * GUI_PX_WIDTH] = col;

                }
            }
        }
    }

    SDL_UnlockTexture(texture);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    SDL_Delay(1000/60);
}

int gui_handleinputs(struct gb_state *gb_state) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    return 1;
                break;
            case SDL_QUIT:
                return 1;
        }
    }
    return 0;
}
