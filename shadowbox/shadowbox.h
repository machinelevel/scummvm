// ej's shadowbox experiment
// This file is copyright 2021 Eric R. Johnston

#ifndef _SHADOWBOX_H_
#define _SHADOWBOX_H_

#include <stdio.h>
#include <stdint.h>
#undef printf
#include "engines/scumm/gfx.h"
#include "engines/scumm/actor.h"
#include "graphics/surface.h"

#include "common/scummsys.h"
#if defined(SDL_BACKEND)
#include "backends/graphics/surfacesdl/surfacesdl-graphics.h"
#endif

using namespace Scumm;
using namespace Graphics;

namespace ShadowBoxNs {

#define SBOX_MAX_NUM_ACTORS 256

class Foil
{
public:
    Foil()
    :w(0), h(0), actor(NULL), sdl8(NULL), sdl16(NULL)
    {
    }
    ~Foil()
    {
    }
    uint w;
    int h;
    const Actor* actor;
    SDL_Surface* sdl8;
    SDL_Surface* sdl16;
    Surface surface;
};

class ShadowBox
{
public:
    ShadowBox()
    {
        _sdlmgr = NULL;
        depth_image = NULL;
        view_angle_min = -1.0f;
        view_angle_max = 1.0f;
        view_angle = 0.0f;
        for (uint32_t i = 0; i < SBOX_MAX_NUM_ACTORS; ++i)
            actor_foils[i] = NULL;
    }
    ~ShadowBox()
    {
        for (uint32_t i = 0; i < SBOX_MAX_NUM_ACTORS; ++i)
            delete actor_foils[i];
    }

    void actor_draw_done(const Actor* actor, int actorX, int actorY1, int actorY2)
    {
        printf("Actor %d draw done: x:%d w:%d y1:%d y2:%d\n", 
            (int)actor->_number, actorX, (int)actor->_width, (int)actorY1, (int)actorY2);
        Foil* foil = actor_foils[actor->_number];
        int x1a = actorX - (actor->_width >> 1);
        int x1 = x1a;
        int x2 = x1 + actor->_width;
        int y1 = actorY1;
        int y2 = actorY2;
        if (x1 < 0)
            x1 = 0;
        if (x2 > 320)
            x2 = 320;
        int h = y2 - y1;
        int w = x2 - x1;
        SDL_LockSurface(foil->sdl8);
        uint8_t* src = (uint8_t*)foil->surface.getPixels();
        uint8_t* dst = (uint8_t*)foil->sdl8->pixels;
        for (int row = 0; row < h; ++row)
        {
            uint8_t* srow = src + (row + y1) * foil->surface.pitch + x1;
            uint8_t* drow = dst + row * foil->sdl8->pitch + (x1 - x1a);
            for (int col = 0; col < w; ++col)
            {
                drow[col] = srow[col];
                srow[col] = 0;
            }
        }
        SDL_UnlockSurface(foil->sdl8);
//            (int)actor->_number, actorX, (int)actor->_width, (int)actor->_top, (int)actor->_bottom);
    }

    Graphics::Surface& get_actor_surface(const Surface &vs_surface, const Actor *actor)
    {
        Foil* foil = actor_foils[actor->_number];

        if (!foil)
            foil = actor_foils[actor->_number] = new Foil();
        foil->actor = actor;
        uint w = actor->_width;
        int h = 200;
        w = (w + 7) & ~7; // pad to 8 pixels
        if (foil->w < w || foil->h < h)
        {
            foil->w = w;
            foil->h = h;
printf("At %d: create foil for actor %d: %dx%d\n", __LINE__, (int)actor->_number, (int)w, (int)h);
            foil->surface.create(vs_surface.w, vs_surface.h, vs_surface.format);

            if (foil->sdl8)
                SDL_FreeSurface(foil->sdl8);

            foil->sdl8 = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, sdl_screen->format->BitsPerPixel,
                                                                    sdl_screen->format->Rmask,
                                                                    sdl_screen->format->Gmask,
                                                                    sdl_screen->format->Bmask,
                                                                    sdl_screen->format->Amask);
printf("]] at %s:%d foil->sdl8 is %dx%d %d bytes/pixel\n", __FILE__, __LINE__, w, h, foil->sdl8->format->BitsPerPixel);
            if (foil->sdl8 == NULL)
                error("allocating _screen failed");
            SDL_SetPaletteColors(foil->sdl8->format->palette, sdl_currentPalette, 0, 256);
        }
//printf("At %d: return %dx%d foil for actor %d\n", __LINE__, foil->surface.w, foil->surface.h, (int)a->_number);
        return foil->surface;
    }

    void save_depth_image()
    {
        const char* image_file_name = "depth/room_depth_save.bmp";
        printf("Saving BMP image: %s\n", image_file_name);
        SDL_SaveBMP(sdl_tmpscreen, image_file_name);
    }

    void convert_to_heightmap(SDL_Surface* img)
    {
        // Convert a palette grayscale image to a no-palette-needed heightmap
        SDL_LockSurface(img);
        uint32_t num_pixels = img->h * img->pitch;
        uint8_t* pix = (uint8_t*)img->pixels;
        SDL_Palette* pal = img->format->palette;
        for (uint32_t i = 0; i < num_pixels; ++i)
            pix[i] = pal->colors[pix[i]].r;

        for (uint32_t i = 0; i < 256; ++i)
        {
            pal->colors[i].r = pal->colors[i].g = pal->colors[i].b = i;
            pal->colors[i].a = 255;
        }
        SDL_UnlockSurface(img);
    }

    void load_depth_image()
    {
        const char* image_file_name = "depth/room_depth_test1.bmp";
        printf("Loading BMP image: %s\n", image_file_name);
        if (depth_image)
            SDL_FreeSurface(depth_image);
        depth_image = SDL_LoadBMP(image_file_name);
        if (depth_image)
        {
            // printf("  depth bpp = %d\n", (int)depth_image->format->BitsPerPixel);
            // printf("  depth palette entries = %d\n", (int)depth_image->format->palette->ncolors);
            // for (int i = 0; i < 256; ++i)
            // {
            //     printf("  depth palette[%d] = %d,%d,%d,%d\n", i, (int)depth_image->format->palette->colors[i].r,
            //                                                      (int)depth_image->format->palette->colors[i].g,
            //                                                      (int)depth_image->format->palette->colors[i].b,
            //                                                      (int)depth_image->format->palette->colors[i].a);
            // }
            convert_to_heightmap(depth_image);
            // for (int i = 0; i < 256; ++i)
            // {
            //     printf("  depth palette[%d] = %d,%d,%d,%d\n", i, (int)depth_image->format->palette->colors[i].r,
            //                                                      (int)depth_image->format->palette->colors[i].g,
            //                                                      (int)depth_image->format->palette->colors[i].b,
            //                                                      (int)depth_image->format->palette->colors[i].a);
            // }
        }
    }

    float adjust_view_angle(float amount)
    {
        view_angle += amount;
        if (view_angle > view_angle_max)
            view_angle = view_angle_max;
        if (view_angle < view_angle_min)
            view_angle = view_angle_min;
        printf("-> shadowbox view %.2f\n", view_angle);
        return view_angle;
    }

    void setup_sdl(SurfaceSdlGraphicsManager* sdlmgr, SDL_Surface* _screen,
                   SDL_Surface* _tmpscreen, SDL_Surface* _hwScreen, SDL_Color* _currentPalette)
    {
        _sdlmgr = sdlmgr;
        sdl_screen = _screen;
        sdl_tmpscreen = _tmpscreen;
        sdl_hwScreen = _hwScreen;
        sdl_currentPalette = _currentPalette;
    }

    void compose()
    {
//        SDL_LockSurface(sdl_hwScreen);
        for (uint32_t actor_num = 0; actor_num < SBOX_MAX_NUM_ACTORS; ++actor_num)
        {
            Foil* foil = actor_foils[actor_num];
            if (foil)
            {
                SDL_Rect sr;
                SDL_Rect dr;
                sr.x = 0;
                dr.x = 32;
                sr.y = dr.y = 0;
                sr.w = dr.w = foil->w;
                sr.h = dr.h = foil->h;
//                SDL_LockSurface(foil->sdl8);
                if (SDL_BlitSurface(foil->sdl8, &sr, sdl_hwScreen, &dr) != 0)
                    error("SDL_BlitSurface failed: %s", SDL_GetError());
//                SDL_UnlockSurface(foil->sdl8);
            }
        }
//        SDL_UnlockSurface(sdl_hwScreen);
        if (1)
        {
            if (!depth_image)
                load_depth_image();
            SDL_Rect sr;
            SDL_Rect dr;
            sr.x = 0;
            dr.x = 128;
            sr.y = dr.y = 0;
            sr.w = dr.w = 320;
            sr.h = dr.h = 200;
            if (SDL_BlitSurface(depth_image, &sr, sdl_hwScreen, &dr) != 0)
                error("SDL_BlitSurface failed: %s", SDL_GetError());
        }
    }

    // void copyRectToScreen(const void *buf, int pitch, int x, int y, int w, int h)
    // {
    //     _system->copyRectToScreen(buf, pitch, x, y, w, h);
    // }

protected:
    float view_angle_min;
    float view_angle_max;
    float view_angle;
    Foil* actor_foils[SBOX_MAX_NUM_ACTORS];
    SurfaceSdlGraphicsManager* _sdlmgr;
    SDL_Surface* sdl_screen;
    SDL_Surface* sdl_tmpscreen;
    SDL_Surface* sdl_hwScreen;
    SDL_Surface* depth_image;
    SDL_Color*   sdl_currentPalette;
};

} // namespace ShadowBox

extern ShadowBoxNs::ShadowBox* shadowbox;

#endif // _SHADOWBOX_H_