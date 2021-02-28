// ej's shadowbox experiment
// This file is copyright 2021 Eric R. Johnston

#ifndef _SHADOWBOX_H_
#define _SHADOWBOX_H_

#include <stdio.h>
#include <stdint.h>
#include <vector>
#include "engines/scumm/gfx.h"
#include "engines/scumm/actor.h"
#include "graphics/surface.h"
#include "common/config-manager.h"

#include "common/scummsys.h"
#if defined(SDL_BACKEND)
#include "backends/graphics/surfacesdl/surfacesdl-graphics.h"
#endif

using namespace Scumm;
using namespace Graphics;

namespace ShadowBoxNs {

extern void TIC();
extern float TOC();

#define SBOX_MAX_NUM_ACTORS 256

enum {
    SHADOWBOX_MODE_OFF,
    SHADOWBOX_MODE_BASIC,
    SHADOWBOX_MODE_GPU,
    SHADOWBOX_MODE_VIEWMASTER,
    SHADOWBOX_MODE_HOWMANY // just to count and terminate
};
#define NUM_MODES_TO_CYCLE 3

class Foil
{
public:
    Foil()
    :w(0), h(0), actor(NULL), sdl8(NULL), sdl16(NULL), sdltex16(NULL)
    {
    }
    ~Foil()
    {
    }
    uint w;
    int h;
    int x;
    int y1;
    int y2;
    const Actor* actor;
    SDL_Surface* sdl8;
    SDL_Surface* sdl16;
    SDL_Texture* sdltex16;
    uint16_t pal565[256];
};

class ShadowBox
{
public:
    ShadowBox()
    {
        mode = SHADOWBOX_MODE_OFF;
        vm = NULL;
        _sdlmgr = NULL;
        sdl_renderer = NULL;
        main_virt_screen = NULL;
        depth_image = NULL;
        selection_map = NULL;
        view_angle_min = -5.0f;
        view_angle_max = 0.0f;
        view_angle = 0.0f;
        room_number = 0;
        room_width = 0;
        mouse_x = mouse_y = 0;
        sel_x = sel_y = 0;
        middle_mouse_down = false;
        highlight_selection = true;
        for (uint32_t i = 0; i < SBOX_MAX_NUM_ACTORS; ++i)
            actor_foils[i] = NULL;
    }
    ~ShadowBox()
    {
        for (uint32_t i = 0; i < SBOX_MAX_NUM_ACTORS; ++i)
            delete actor_foils[i];
    }

    void actor_draw_done(const Actor* actor, int actorX, int actorY1, int actorY2,
                        VirtScreen* mainVS)
    {
        // printf("Actor %d draw done: x:%d w:%d y1:%d y2:%d\n", 
        //     (int)actor->_number, actorX, (int)actor->_width, (int)actorY1, (int)actorY2);
//printf("--> at %d\n", __LINE__);
        if (scratch_surface.w == 0)
            return;
//printf("--> at %d\n", __LINE__);

        Foil* foil = actor_foils[actor->_number];

        if (!foil)
            foil = actor_foils[actor->_number] = new Foil();
//printf("--> at %d\n", __LINE__);
        foil->actor = actor;
        main_virt_screen = mainVS;

        if (foil->sdl8)
        {
            SDL_SetPaletteColors(foil->sdl8->format->palette, sdl_currentPalette, 0, 256);
            make_actor_pal565(foil);
        }

        int x1a = actorX - (actor->_width >> 1);
        int x1 = x1a;
        int x2 = x1 + actor->_width;
        int y1 = actorY1;
        int y2 = actorY2;
        if (x1 < 0)
            x1 = 0;
        if (x2 > 320)
            x2 = 320;
//printf("]] at %d: \n", __LINE__);
        int h = y2 - y1;
        int w = x2 - x1;
//printf("--> at %d\n", __LINE__);

        if (!foil->sdl8 || !foil->sdltex16 || foil->sdl8->w < w || foil->sdl8->h < h)
        {
            if (foil->sdl8)
                SDL_FreeSurface(foil->sdl8);
            if (foil->sdltex16)
                SDL_DestroyTexture(foil->sdltex16);
            foil->w = w;
            foil->h = h;
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
            foil->sdltex16 = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_RGB565,
                                               SDL_TEXTUREACCESS_STREAMING, w, h);
            foil->sdl8 = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, sdl_screen->format->BitsPerPixel,
                                                                    sdl_screen->format->Rmask,
                                                                    sdl_screen->format->Gmask,
                                                                    sdl_screen->format->Bmask,
                                                                    sdl_screen->format->Amask);
            // foil->sdl16 = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h,
            //             16,
            //             sdl_hwScreen->format->Rmask,
            //             sdl_hwScreen->format->Gmask,
            //             sdl_hwScreen->format->Bmask,
            //             sdl_hwScreen->format->Amask);
printf("]] at %s:%d foil->sdl8 is %dx%d %d bits/pixel\n", __FILE__, __LINE__, w, h, foil->sdl8->format->BitsPerPixel);
            if (foil->sdl8 == NULL)
                error("allocating 8-bit foil failed");
            // if (foil->sdl16 == NULL)
            //     error("allocating 16-bit foil failed");
            if (foil->sdltex16 == NULL)
                error("allocating 16-bit-texture foil failed");
        }
//printf("--> at %d\n", __LINE__);


//printf("]] at %d: \n", __LINE__);
        uint16_t* dsttex16_pixels = NULL;
        int dsttex16_pitch = 0;
        SDL_LockTexture(foil->sdltex16, NULL, (void**)&dsttex16_pixels, &dsttex16_pitch);
        SDL_LockSurface(foil->sdl8);
//        SDL_LockSurface(foil->sdl16);
//printf("]] at %d: \n", __LINE__);
        uint8_t* src = (uint8_t*)scratch_surface.getPixels();
        uint8_t* dst8 = (uint8_t*)foil->sdl8->pixels;
//        uint16_t* dst16 = (uint16_t*)foil->sdl16->pixels;
//printf("]] at %d: \n", __LINE__);
        for (int row = 0; row < h; ++row)
        {
            uint8_t* srow = src + (row + y1) * scratch_surface.pitch + x1;
            uint8_t* drow8 = dst8 + row * foil->sdl8->pitch + (x1 - x1a);
//            uint16_t* drow16 = dst16 + row * (foil->sdl16->pitch >> 1) + (x1 - x1a);
            uint16_t* dsttex16 = dsttex16_pixels + row * (dsttex16_pitch >> 1) + (x1 - x1a);
            for (int col = 0; col < w; ++col)
            {
                dsttex16[col] = foil->pal565[srow[col]];
//                drow16[col] = foil->pal565[srow[col]];
                drow8[col] = srow[col];
                srow[col] = 0;
            }
        }
//printf("]] at %d: \n", __LINE__);
        SDL_UnlockTexture(foil->sdltex16);
        SDL_UnlockSurface(foil->sdl8);
//        SDL_UnlockSurface(foil->sdl16);
        foil->x = actorX + main_virt_screen->xstart;
        foil->y1 = actorY1 - main_virt_screen->topline;
        foil->y2 = actorY2 - main_virt_screen->topline;
//        transfer_actor_to_texture(foil);
//            (int)actor->_number, actorX, (int)actor->_width, (int)actor->_top, (int)actor->_bottom);
//printf("--> at %d\n", __LINE__);
    }

    Graphics::Surface& get_actor_surface(Surface &vs_surface, ScummEngine *_vm)
    {
        vm = _vm;
//printf("--> at %d\n", __LINE__);
        if (mode == SHADOWBOX_MODE_OFF)
            return vs_surface;

        if (scratch_surface.w < vs_surface.w || scratch_surface.h < vs_surface.h)
        {
//printf("--> at %d\n", __LINE__);
            scratch_surface.create(vs_surface.w, vs_surface.h, vs_surface.format);
        }
//printf("At %d: return %dx%d foil for actor %d\n", __LINE__, scratch_surface.w, scratch_surface.h, (int)a->_number);
//printf("--> at %d ssw %d ssh %d \n", __LINE__, (int)scratch_surface.w, (int)scratch_surface.h);
        return scratch_surface;
    }

    void room_depth_filename(char* dest, size_t dest_len)
    {
        const char* gameid = ConfMan.get("gameid").c_str();
        snprintf(dest, dest_len, "depth/%s__room_depth_%d.bmp", gameid, (int)room_number);
    }

    void save_depth_image()
    {
        char image_file_name[1024];
        room_depth_filename(image_file_name, sizeof(image_file_name));
        printf("Saving BMP image: %s\n", image_file_name);
        SDL_SaveBMP(depth_image, image_file_name);
    }

    void make_grayscale_clut(SDL_Surface* img)
    {
        // build the grayscale palette
        SDL_LockSurface(img);
        SDL_Palette* pal = img->format->palette;
        for (uint32_t i = 0; i < 256; ++i)
        {
            pal->colors[i].r = pal->colors[i].g = pal->colors[i].b = i;
            pal->colors[i].a = 255;
        }
        SDL_UnlockSurface(img);
    }

    void convert_to_heightmap(SDL_Surface* img)
    {
        // Convert a palette grayscale image to a no-palette-needed heightmap
        SDL_LockSurface(img);
        uint32_t num_pixels = img->h * img->pitch;
        uint8_t* pix = (uint8_t*)img->pixels;
        SDL_Palette* pal = img->format->palette;
        if (!pal)
        {
            printf("Error: depth images should be 8-bit grayscale BMP.\n");
            return;
        }
// printf("    ]] at %d img %p w %d h %d pitch %d pal %p\n", __LINE__, img, (int)img->w, (int)img->h, (int)img->pitch);
// printf("    ]] at %d npix %d pix %p pal %p\n", __LINE__, (int)num_pixels, pix, pal);
        for (uint32_t i = 0; i < num_pixels; ++i)
        {
            uint8_t h = pal->colors[pix[i]].r;
            pix[i] = h ? h : 1; // min height of 1, so we know what's covered when rendering
        }
        make_grayscale_clut(img);
        SDL_UnlockSurface(img);
    }

    void fill_heightmap(SDL_Surface* img, uint8_t value)
    {
        SDL_LockSurface(img);
        uint32_t num_pixels = img->h * img->pitch;
        uint8_t* pix = (uint8_t*)img->pixels;
        for (uint32_t i = 0; i < num_pixels; ++i)
        {
            pix[i] = value;
        }
        SDL_UnlockSurface(img);
    }

    void load_depth_image()
    {
        char image_file_name[1024];
        room_depth_filename(image_file_name, sizeof(image_file_name));
        printf("Loading BMP image: %s\n", image_file_name);
        if (depth_image)
            SDL_FreeSurface(depth_image);
        depth_image = SDL_LoadBMP(image_file_name);
//        depth_image = NULL;
        if (depth_image)
        {
            convert_to_heightmap(depth_image);
        }
        else if (room_width > 0)
        {
            int w = room_width;
            int h = 200;
            printf("CREATING HEIGHTMAP: %dx%d\n", w, h);
            depth_image = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 8,
                                        sdl_screen->format->Rmask,
                                        sdl_screen->format->Gmask,
                                        sdl_screen->format->Bmask,
                                        sdl_screen->format->Amask);
            make_grayscale_clut(depth_image);
            fill_heightmap(depth_image, 1);
        }
    }

    void make_selection_map(int w, int h)
    {
        if (selection_map)
        {
            if (selection_map->w < w || selection_map->h < h)
            {
                SDL_FreeSurface(selection_map);
                selection_map = NULL;
            }
        }
        if (!selection_map)
        {
            selection_map = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 8,
                                        sdl_screen->format->Rmask,
                                        sdl_screen->format->Gmask,
                                        sdl_screen->format->Bmask,
                                        sdl_screen->format->Amask);
            make_grayscale_clut(selection_map);
        }
        fill_heightmap(selection_map, 0);
    }

    void clear_selection()
    {
        fill_heightmap(selection_map, 0);
    }

    void gather_selection_info(std::vector<uint32_t>& sel_checks, uint8_t* colors_used, uint8_t* depths_used)
    {
        sel_checks.clear();
        for (uint32_t i = 0; i < 256; ++i)
        {
            colors_used[i] = 0;
            depths_used[i] = 0;
        }
        for (int32_t y = 0; y < selection_map->h; ++y)
        {
            for (int32_t x = 0; x < selection_map->w; ++x)
            {
                uint8_t sel = ((uint8_t*)selection_map->pixels)[x + y * selection_map->pitch];
                if (sel)
                {
                    sel_checks.push_back((y << 16) | x);
                    uint8_t depth = ((uint8_t*)depth_image->pixels)[x + y * selection_map->pitch];
                    depths_used[depth] = 1;
                    int32_t screen_x = (int)x - (int)main_virt_screen->xstart;
                    if (screen_x >= 0 && screen_x < 320)
                    {
                        uint8_t color = ((uint8_t*)sdl_screen->pixels)[screen_x + y * sdl_screen->pitch];
                        colors_used[color] = 1;
                    }
                }
            }
        }
    }

    void expand_selection_by_color()
    {
        uint8_t colors_used[256];
        uint8_t depths_used[256];
        std::vector<uint32_t> sel_checks;
        gather_selection_info(sel_checks, colors_used, depths_used);
        for (uint32_t si = 0; si < sel_checks.size(); ++si)
        {
            int32_t sx = sel_checks[si] & 0x0000ffff;
            int32_t sy = (sel_checks[si] >> 16) & 0x0000ffff;
            int32_t screen_sx = (int)sx - (int)main_virt_screen->xstart;
            if (screen_sx >= 0 && screen_sx < 320)
            {
//                uint8_t scolor = ((uint8_t*)sdl_screen->pixels)[screen_sx + sy * sdl_screen->pitch];
                for (int32_t dy = -1; dy <=1; ++dy)
                {
                    for (int32_t dx = -1; dx <= 1; ++dx)
                    {
                        if (dx || dy)
                        {
                            int32_t px = sx + dx;
                            int32_t py = sy + dy;
                            uint8_t& psel = ((uint8_t*)selection_map->pixels)[px + py * selection_map->pitch];
                            if (!psel)
                            {
                                int32_t screen_px = (int)px - (int)main_virt_screen->xstart;
                                if (screen_px >= 0 && screen_px < 320)
                                {
                                    uint8_t pcolor = ((uint8_t*)sdl_screen->pixels)[screen_px + py * sdl_screen->pitch];
                                    if (colors_used[pcolor])
                                    {
                                        // Expand!
                                        psel = 1;
                                        uint8_t pdepth = ((uint8_t*)depth_image->pixels)[px + py * selection_map->pitch];
                                        depths_used[pdepth] = 1;
                                        sel_checks.push_back((py << 16) | px);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    void expand_selection_by_depth()
    {
    }

    void set_edit_depth(int depth10)
    {
        if (depth10 < 0)
        {
            edit_depth = depth10;
        }
        else
        {
            uint8_t depth = (255 * depth10) / 10;
            if (depth == 0)
                depth = 1;
            if (depth10 == 0)
                depth = 0;
            if (edit_depth == depth)
                edit_depth = -1;
            else
                edit_depth = depth;
        }
    }

    void push_selection_to_depth(int depth10)
    {
        uint8_t depth = (255 * depth10) / 10;
        if (depth == 0)
            depth = 1;
        if (depth10 == 0)
            depth = 0;

        SDL_LockSurface(selection_map);
        SDL_LockSurface(depth_image);
        uint32_t num_pixels = selection_map->h * selection_map->pitch;
        uint8_t* spix = (uint8_t*)selection_map->pixels;
        uint8_t* dpix = (uint8_t*)depth_image->pixels;
        for (uint32_t i = 0; i < num_pixels; ++i)
        {
            if (spix[i])
                dpix[i] = depth;
        }
        SDL_UnlockSurface(selection_map);
        SDL_UnlockSurface(depth_image);
    }

    void load_room_done(int roomNumber, int roomWidth)
    {
        room_number = roomNumber;
        room_width = roomWidth;
        load_depth_image();
        make_selection_map(room_width, 200);
    }

    float adjust_view_angle(float amount)
    {
        view_angle += amount;
        if (view_angle > view_angle_max)
            view_angle = view_angle_max;
        if (view_angle < view_angle_min)
            view_angle = view_angle_min;
//        printf("-> shadowbox view %.2f\n", view_angle);
        return view_angle;
    }

    void setup_sdl(SurfaceSdlGraphicsManager* sdlmgr, SDL_Surface* _screen, SDL_Renderer* _renderer,
                   SDL_Surface* _tmpscreen, SDL_Surface* _hwScreen, SDL_Color* _currentPalette)
    {
        _sdlmgr = sdlmgr;
        sdl_screen = _screen;
        sdl_renderer = _renderer;
        sdl_tmpscreen = _tmpscreen;
        sdl_hwScreen = _hwScreen;
        sdl_currentPalette = _currentPalette;

        // set up the heightmap
        zbuf = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 200, 8,
                                        sdl_screen->format->Rmask,
                                        sdl_screen->format->Gmask,
                                        sdl_screen->format->Bmask,
                                        sdl_screen->format->Amask);
        make_grayscale_clut(zbuf);
    }

    void clear_zbuf()
    {
//        SDL_LockSurface(zbuf);
        uint32_t num_uint64 = zbuf->h * (zbuf->pitch >> 3);
        uint64_t* pix = (uint64_t*) zbuf->pixels;
        for (uint32_t i = 0; i < num_uint64; ++i)
            pix[i] = 0;
//        SDL_UnlockSurface(zbuf);
    }

    void make_parallax_table(float vangle)
    {
        float near_parallax = vangle * 10.0;
        float far_parallax = vangle * -10.0;
        for (uint32_t i = 0; i < 256; ++i)
        {
            float t = (float)i / 255.0;
            float parallax = near_parallax + t * (far_parallax - near_parallax);
            parallax_table[i] = int32_t(parallax);
        }
    }

    void clear_to_black()
    {
        if (!(sdl_tmpscreen && sdl_hwScreen && depth_image && zbuf))
            return;
        SDL_LockSurface(sdl_hwScreen);
        uint32_t num_uint64 = sdl_hwScreen->h * (sdl_hwScreen->pitch >> 3);
        uint64_t* pix = (uint64_t*) sdl_hwScreen->pixels;
        for (uint32_t i = 0; i < num_uint64; ++i)
            pix[i] = 0;
        SDL_UnlockSurface(sdl_hwScreen);
    }

    void make_actor_pal565(Foil* foil)
    {
        if (foil && foil->sdl8 && foil->y2 > foil->y1)
        {
            SDL_LockSurface(foil->sdl8);
            for (uint32_t i = 0; i < 256; ++i)
            {
                SDL_Color& cp = sdl_currentPalette[i];
                foil->pal565[i] = (((uint16_t)cp.r & 0xf8) << 8) | (((uint16_t)cp.g & 0xf8) << 3) | (((uint16_t)cp.b & 0xf8) >> 3);
            }
            SDL_UnlockSurface(foil->sdl8);
        }
    }

    void compose_actor_foil(Foil* foil, int offset_x, int offset_y, int scale, float viewport_scale)
    {
        make_actor_pal565(foil);
        int32_t w = foil->w;
        int32_t h = foil->y2 - foil->y1;
        if (mode == SHADOWBOX_MODE_GPU)
        {
            int character_parallax = 0;
            float fx = viewport_scale * (offset_x + character_parallax + scale * (foil->x - main_virt_screen->xstart - (foil->w >> 1)));
            float fy = viewport_scale * (offset_y + scale * (foil->y2 + main_virt_screen->topline));
            SDL_Rect dst_rect;
            dst_rect.x = (int)fx;
            dst_rect.y = (int)fy;
            dst_rect.w = (int)(w * scale * viewport_scale);
            dst_rect.h = (int)(h * scale * viewport_scale);
            SDL_RenderCopy(sdl_renderer, foil->sdltex16, NULL, &dst_rect);
            return;
        }
        if (foil && foil->sdl8 && foil->y2 > foil->y1)
        {
            SDL_LockSurface(foil->sdl8);
            uint8_t* csrc = (uint8_t*)foil->sdl8->pixels;
            uint16_t* cdst = (uint16_t*)sdl_hwScreen->pixels;
            uint8_t* zdst = (uint8_t*)zbuf->pixels;
            uint8_t* zsrc = (uint8_t*)depth_image->pixels;

            cdst += offset_x + offset_y * (sdl_hwScreen->pitch >> 1);
            cdst += scale * (foil->y1 + main_virt_screen->topline) * (sdl_hwScreen->pitch >> 1);
            int32_t xmax = scale * 320;

            const uint8_t* pz = zsrc;
            pz += (foil->y2 + main_virt_screen->topline) * depth_image->pitch;
            pz += foil->x - (foil->w >> 1);
            uint8_t character_z = 0;
            if (pz >= zsrc && pz < zsrc + depth_image->h * depth_image->pitch)
                character_z = *pz;
            int32_t character_parallax = parallax_table[character_z];
//printf("actor %d depth is %d\n", (int)foil->actor->_number, (int)character_z);

            for (int32_t row = 0; row < h; ++row)
            {
                for (int32_t srow = 0; srow < scale; ++srow)
                {
                    int32_t xdst = character_parallax + scale * (foil->x - main_virt_screen->xstart - (foil->w >> 1));
                    for (int32_t col = 0; col < w; ++col)
                    {
                        uint8_t c8 = csrc[col];
                        if (c8)
                        {
                            uint16_t cc = foil->pal565[c8];
                            for (int s = 0; s < scale; ++s)
                            {
                                if (xdst >= 0 && xdst < xmax)
                                {
                                    cdst[xdst] = cc;
    //                                zdst[xdst] = z;
                                    xdst++;
                                }
                            }
                        }
                        else
                        {
                            xdst += scale;
                        }
                    }
                    cdst += sdl_hwScreen->pitch >> 1;
                }
                // for (int copy_row = 1; copy_row < scale; ++copy_row)
                // {
                //     memcpy(((uint8_t*)cdst) + sdl_hwScreen->pitch, cdst, sdl_hwScreen->pitch);
                //     cdst += sdl_hwScreen->pitch >> 1;
                // }
                csrc += foil->sdl8->pitch;
                zdst += zbuf->pitch;
            }
            SDL_UnlockSurface(foil->sdl8);
        }
    }

    // void transfer_actor_to_texture(Foil* foil)
    // {
    //     if (foil && foil->sdl16 && foil->sdltex16 && foil->y2 > foil->y1)
    //     {
    //         SDL_LockSurface(foil->sdl16);
    //         SDL_UpdateTexture(foil->sdltex16, nullptr, foil->sdl16->pixels, foil->sdl16->pitch);
    //         SDL_UnlockSurface(foil->sdl16);
    //     }
    // }

    void compose_actors(int offset_x, int offset_y, int scale, float viewport_scale)
    {
        for (uint32_t actor_num = 0; actor_num < SBOX_MAX_NUM_ACTORS; ++actor_num)
        {
            Foil* foil = actor_foils[actor_num];
            if (foil)
                compose_actor_foil(foil, offset_x, offset_y, scale, viewport_scale);
        }
    }

    void compose_viewmaster_eye(int offset_x, int offset_y, int scale)
    {
        uint16_t* csrc = (uint16_t*)sdl_tmpscreen->pixels;
        csrc += sdl_tmpscreen->pitch >> 1;  // this addreses an off-by-1
        uint16_t* cdst = (uint16_t*)sdl_hwScreen->pixels;
        uint8_t* sel = (uint8_t*)selection_map->pixels;
        uint8_t* zsrc = (uint8_t*)depth_image->pixels;
        uint8_t* zdst = (uint8_t*)zbuf->pixels;
        int32_t w = 320;
        int32_t h = 200;
        sel += main_virt_screen->xstart;
        zsrc += main_virt_screen->xstart;
        cdst += offset_x + offset_y * (sdl_hwScreen->pitch >> 1);

        for (int32_t row = 0; row < h; ++row)
        {
            uint16_t cprev = csrc[0];
            uint8_t zprev = zsrc[0];
            int32_t plxprev = parallax_table[zprev];
            int32_t xdst = plxprev;
            for (int32_t x = 0; x < xdst; ++x)
            {
                cdst[x] = 0;
                zdst[x] = 0;
            }
            for (int32_t col = 0; col < w; ++col)
            {
                uint16_t c = csrc[col+1];  // this addreses an off-by-1
                uint8_t z = zsrc[col];
                int32_t plx = plxprev;
                if (z != zprev)
                    plx = parallax_table[z];
                int32_t new_xdst = (scale * col) + plx;
                uint16_t cc = c;
                if (edit_depth >= 0 && (int)z == edit_depth)
                    cc = 0x00ff;
                if (col == sel_x && row == sel_y)
                    cc = 0xffff;
                if (sel[col])
                    cc = 0xffe0; // bright for selection
                if (xdst < new_xdst)
                {
                    // Fill in any empty space with the lower z value
                    uint16_t cfill = c;
                    uint8_t zfill = z;
                    if (zprev < z)
                    {
                        cfill = cprev;
                        zfill = zprev;
                    }
                    while (xdst < new_xdst)
                    {
                        if (xdst >= 0 && xdst < (w * scale))
                        {
                            if (zfill > zdst[xdst])
                            {
                                cdst[xdst] = cfill;
                                zdst[xdst] = zfill;
                            }
                        }
                        xdst++;
                    }
                }
                xdst = new_xdst;
                for (int s = 0; s < scale; ++s)
                {
                    if (xdst >= 0 && xdst < (w * scale))
                    {
                        if (z >= zdst[xdst])
                        {
                            cdst[xdst] = cc;
                            zdst[xdst] = z;
                            xdst++;
                        }
                    }
                }
                cprev = c;
                zprev = z;
                plxprev = plx;
            }
            while (xdst < (w * scale))
            {
                if (xdst >= 0)
                {
                    if (!zdst[xdst])
                        cdst[xdst] = 0;
                }
                xdst++;
            }
            // replicate scaled rows
            for (int copy_row = 1; copy_row < scale; ++copy_row)
            {
                memcpy(((uint8_t*)cdst) + sdl_hwScreen->pitch, cdst, sdl_hwScreen->pitch);
                cdst += sdl_hwScreen->pitch >> 1;
                // memcpy(((uint8_t*)zdst) + zbuf->pitch, zdst, zbuf->pitch);
                // zdst += zbuf->pitch;
            }
            csrc += sdl_tmpscreen->pitch >> 1;
            cdst += sdl_hwScreen->pitch >> 1;
            sel += selection_map->pitch;
            zsrc += depth_image->pitch;
            zdst += zbuf->pitch;
        }
        compose_actors(offset_x, offset_y, scale, 1.0f);
    }

    void compose_viewmaster(int config)
    {
        // depth_image is 320x200 8bit
        // zbuf is 640x200 8bit
        // sdl_tmpscreen is 323x203 16bit
        // sdl_hwScreen is 640x480 16bit
        if (!(main_virt_screen && sdl_tmpscreen && sdl_hwScreen && depth_image && zbuf))
            return;
        SDL_LockSurface(selection_map);
        SDL_LockSurface(depth_image);
        SDL_LockSurface(zbuf);
        SDL_LockSurface(sdl_tmpscreen);
        SDL_LockSurface(sdl_hwScreen);

        if (config == 1)
        {
            // left eye
            make_parallax_table(view_angle);
            clear_zbuf();
            compose_viewmaster_eye(0, 0, 2);
        }
        else if (config == 2)
        {
            // left eye
            make_parallax_table(view_angle);
            clear_zbuf();
            compose_viewmaster_eye(0+0, 200, 1);
            // right eye
            make_parallax_table(-view_angle);
            clear_zbuf();
            compose_viewmaster_eye(0+320, 200, 1);
        }

        SDL_UnlockSurface(selection_map);
        SDL_UnlockSurface(depth_image);
        SDL_UnlockSurface(zbuf);
        SDL_UnlockSurface(sdl_tmpscreen);
        SDL_UnlockSurface(sdl_hwScreen);
    }

    void update_depth_editor()
    {
        if (middle_mouse_down)
        {
            SDL_LockSurface(selection_map);
            uint8_t* sel = (uint8_t*)selection_map->pixels;
            int x = main_virt_screen->xstart + sel_x;
            int y = sel_y;
            sel[x + y * selection_map->pitch] = 1;
            SDL_UnlockSurface(selection_map);
        }
    }

    bool compose_gpu(SDL_Texture* screen_texture,
                     const SDL_Rect* viewport)
    {
        static uint32_t report_skip = 0;
        TIC();

        SDL_RenderClear(sdl_renderer);
        SDL_Rect dst_rect = *viewport;
        SDL_RenderCopy(sdl_renderer, screen_texture, NULL, &dst_rect);
        int offset_x = 0;
        int offset_y = 0;
        int scale = 2;
        compose_actors(offset_x, offset_y, scale, (float)viewport->w / 640.0f);

        if (report_skip++ > 20)
        {
            report_skip = 0;
            printf("gpu compose time = %f ms\n",  0.001 * (float)TOC());
        }
        SDL_RenderPresent(sdl_renderer);
        return true;
    }

    bool compose(int mouseX, int mouseY, SDL_Texture* screen_texture, const SDL_Rect* viewport)
    {
        if (mode == SHADOWBOX_MODE_OFF)
            return false;
        if (mode == SHADOWBOX_MODE_GPU)
            return compose_gpu(screen_texture, viewport);

        static uint32_t report_skip = 0;
        TIC();
        SDL_RenderClear(sdl_renderer);
        mouse_x = mouseX;
        mouse_y = mouseY;
        sel_x = mouse_x - 2;
        sel_y = mouse_y - 2;
        if (sel_x < 0)
            sel_x = 0;
        if (sel_y < 0)
            sel_y = 0;
        update_depth_editor();
        bool report = false;
        if (mode == SHADOWBOX_MODE_BASIC)
        {
            compose_viewmaster(1);
            report = true;
        }
        else if (mode == SHADOWBOX_MODE_VIEWMASTER)
        {
            compose_viewmaster(2);
            report = true;
        }
        SDL_RenderCopy(sdl_renderer, screen_texture, NULL, viewport);
        if (report)
        {
            if (report_skip++ > 20)
            {
                report_skip = 0;
                printf("cpu compose time = %f ms\n",  0.001 * (float)TOC());
            }
        }
        SDL_RenderPresent(sdl_renderer);
        return true;
    }

    int get_mode() const
    {
        return mode;
    }

    int get_mode_hw_width(int proposed_width) const
    {
        return proposed_width;
    }

    int get_mode_hw_height(int proposed_height) const
    {
        return proposed_height;
        return proposed_height;
    }

    void set_mode(int to_mode)
    {
        mode = to_mode;
        clear_to_black();
        if (vm)
            vm->_fullRedraw = true;
        if (mode == SHADOWBOX_MODE_OFF)
        {
//            _sdlmgr->shadowbox_set_hw_screen(640, 400);
            _sdlmgr->notifyVideoExpose();
        }
        if (mode == SHADOWBOX_MODE_VIEWMASTER || mode == SHADOWBOX_MODE_BASIC)
        {
//            _sdlmgr->shadowbox_set_hw_screen(1280, 400);
        }
    }

    void next_mode()
    {
        set_mode((mode + 1) % NUM_MODES_TO_CYCLE);
    }

    void do_key(char key_ascii)
    {
        switch (key_ascii)
        {
            case 's':  save_depth_image(); break;
            case 'l':  load_depth_image(); break;
            case '\\': next_mode(); break;
            case '|':  set_mode(SHADOWBOX_MODE_VIEWMASTER); break;
            case 'x':  clear_selection(); break;
            case 'c':  expand_selection_by_color(); break;
            case 'd':  expand_selection_by_depth(); break;
            case '1':  push_selection_to_depth(1); break;
            case '2':  push_selection_to_depth(2); break;
            case '3':  push_selection_to_depth(3); break;
            case '4':  push_selection_to_depth(4); break;
            case '5':  push_selection_to_depth(5); break;
            case '6':  push_selection_to_depth(6); break;
            case '7':  push_selection_to_depth(7); break;
            case '8':  push_selection_to_depth(8); break;
            case '9':  push_selection_to_depth(9); break;
            case '0':  push_selection_to_depth(0); break;
            case '~':  set_edit_depth(-1); break;
            case '!':  set_edit_depth(1); break;
            case '@':  set_edit_depth(2); break;
            case '#':  set_edit_depth(3); break;
            case '$':  set_edit_depth(4); break;
            case '%':  set_edit_depth(5); break;
            case '^':  set_edit_depth(6); break;
            case '&':  set_edit_depth(7); break;
            case '*':  set_edit_depth(8); break;
            case '(':  set_edit_depth(9); break;
            case ')':  set_edit_depth(0); break;
            default:   break;
        }
    }

    void do_middle_mouse_button(bool down)
    {
        printf("MM = %d\n", (int)down);
        middle_mouse_down = down;
    }

protected:
    float view_angle_min;
    float view_angle_max;
    float view_angle;
    Foil* actor_foils[SBOX_MAX_NUM_ACTORS];
    SurfaceSdlGraphicsManager* _sdlmgr;
    ScummEngine* vm;
    SDL_Surface* sdl_screen;
    SDL_Renderer* sdl_renderer;
    SDL_Surface* sdl_tmpscreen;
    SDL_Surface* sdl_hwScreen;
    SDL_Surface* depth_image;
    SDL_Surface* selection_map;
    SDL_Surface* zbuf;
    SDL_Color*   sdl_currentPalette;
    VirtScreen*  main_virt_screen;
    int room_number;
    int room_width;
    int32_t parallax_table[256];
    int mode;
    int edit_depth;
    int mouse_x, mouse_y;
    int sel_x, sel_y;
    bool middle_mouse_down;
    bool highlight_selection;
    Surface scratch_surface;
};

} // namespace ShadowBox

extern ShadowBoxNs::ShadowBox* shadowbox;

#endif // _SHADOWBOX_H_