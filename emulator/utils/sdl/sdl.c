
#include "sdl.h"
#include <signal.h>


 SDL_Surface *screen;


static void sdl_update(DisplayState *ds, int x, int y, int w, int h)
{
    SDL_UpdateRect(screen, x, y, w, h);
}

static void sdl_resize(DisplayState *ds, int w, int h)
{
    int flags;

   printf("resizing to %d %d\n", w, h);

    flags = SDL_HWSURFACE|SDL_ASYNCBLIT|SDL_HWACCEL;
 again:
    screen = SDL_SetVideoMode(w, h, ds->depth, flags);
    if (!screen) {
        fprintf(stderr, "Could not open SDL display\n");
        exit(1);
    }
    if (!screen->pixels && (flags & SDL_HWSURFACE) && (flags & SDL_FULLSCREEN)) {
        flags &= ~SDL_HWSURFACE;
        goto again;
    }

    if (!screen->pixels) {
        fprintf(stderr, "Could not open SDL display\n");
        exit(1);
    }
    ds->data = screen->pixels;
    ds->linesize = screen->pitch;
    //ds->depth = screen->format->BitsPerPixel;
    if (ds->depth == 32 && screen->format->Rshift == 0) {
        ds->bgr = 1;
    } else {
        ds->bgr = 0;
    }
}
static void sdl_refresh(DisplayState *ds)
{

}

static void sdl_update_caption(void)
{
    char buf[1024];
    strcpy(buf, "VirtualMIPS");
    SDL_WM_SetCaption(buf, "");
}

static void sdl_cleanup(void) 
{
    SDL_Quit();
}
void sdl_display_init(DisplayState *ds, int full_screen)
{
    int flags;
    uint8_t data = 0;

    flags = SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE;
    if (SDL_Init (flags)) {
        fprintf(stderr, "Could not initialize SDL - exiting\n");
        exit(1);
    }
#ifndef _WIN32
    /* NOTE: we still want Ctrl-C to work, so we undo the SDL redirections */
    //signal(SIGINT, SIG_DFL);
    //signal(SIGQUIT, SIG_DFL);
#endif

    ds->dpy_update = sdl_update;
    ds->dpy_resize = sdl_resize;
    ds->dpy_refresh = sdl_refresh;

    sdl_resize(ds, ds->width , ds->height);
    sdl_update_caption();
    SDL_EnableUNICODE(1);

    atexit(sdl_cleanup);
}

