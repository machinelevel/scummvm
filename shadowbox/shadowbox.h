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
    :w(0), h(0), actor(NULL)
    {
    }
    ~Foil()
    {
    }
    uint w;
    int h;
    const Actor* actor;
    Surface surface;
    SDL_Surface* sdl8;
    SDL_Surface* sdl16;
};

class ShadowBox
{
public:
    ShadowBox()
    {
        _sdlmgr = NULL;
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
            // TODO: delete the old buffers
printf("At %d: create foil for actor %d: %dx%d\n", __LINE__, (int)actor->_number, (int)w, (int)h);
            foil->surface.create(vs_surface.w, vs_surface.h, vs_surface.format);

            // const Graphics::PixelFormat &format = sdl_screen->format;
            // const Uint32 rMask = ((0xFF >> format.rLoss) << format.rShift);
            // const Uint32 gMask = ((0xFF >> format.gLoss) << format.gShift);
            // const Uint32 bMask = ((0xFF >> format.bLoss) << format.bShift);
            // const Uint32 aMask = ((0xFF >> format.aLoss) << format.aShift);
            // foil->sdl8 = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, format.bytesPerPixel * 8, rMask, gMask, bMask, aMask);
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
        SDL_LockSurface(sdl_hwScreen);
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
                SDL_LockSurface(foil->sdl8);
                if (SDL_BlitSurface(foil->sdl8, &sr, sdl_hwScreen, &dr) != 0)
                    error("SDL_BlitSurface failed: %s", SDL_GetError());
                SDL_UnlockSurface(foil->sdl8);
            }
        }
        SDL_UnlockSurface(sdl_hwScreen);
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
    SDL_Color*   sdl_currentPalette;
};

} // namespace ShadowBox

extern ShadowBoxNs::ShadowBox* shadowbox;

#endif // _SHADOWBOX_H_