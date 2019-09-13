#include "sdl_head.h"

void    filledLine(SDL_Surface *screen, SDL_Surface *image, int screenX, int xStart, int xEnd, int top, int bottom)
{
    int *screenPix = (int*)screen->pixels;
    int *imagePix = (int*)image->pixels;
    float lineLength = abs(top - bottom);
    if (xEnd - xStart == 0)
        return ;
    int x = (float)(screenX - xStart) / (float)(xEnd - xStart) * (float)image->w;
    float y = 0;
    if (top < 0)
        y = (double)image->h / lineLength * abs(top);
    top = clamp(top, 0, H - 1);
    bottom = clamp(bottom, 0, H - 1);
    while (top < bottom)
    {
        //if (top >= 0)
        screenPix[(int)(screenX + top * screen->w)] = imagePix[(screenX * 10) % image->w + (((int)y * 10) % image->h) * image->w];
        top++;
        // if (lineLength != 0)s
        //     y += (double)image->h / lineLength;
        y++;
    }
}

Uint32 getpixel(SDL_Surface *surface, int x, int y)
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to retrieve */
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
        case 1:
            return *p;
            break;

        case 2:
            return *(Uint16 *)p;
            break;

        case 3:
            if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
                return p[0] << 16 | p[1] << 8 | p[2];
            else
                return p[0] | p[1] << 8 | p[2] << 16;
            break;

        case 4:
            return *(Uint32 *)p;
            break;

        default:
            return 0;       /* shouldn't happen, but avoids warnings */
    }
}

static float Scaler_Next(struct Scaler* i)
{
    for(i->cache += i->fd; i->cache >= i->ca; i->cache -= i->ca) i->result += i->bop;
    return i->result;
}

void textLine(int x, int y1,int y2, struct Scaler ty,unsigned txtx, struct sector *sect, SDL_Surface *surface, SDL_Surface *image)
{
    int *pix = (int*) surface->pixels;
    int *imagePix = (int*)image->pixels;
    y1 = clamp(y1, 0, H-1);
    y2 = clamp(y2, 0, H-1);
    pix += y1 * W + x;
    for(int y = y1; y <= y2; ++y)
    {
        float txty = Scaler_Next(&ty);
        //if (fabsf(sect->floor - sect->ceil) * 10.0f > image->h)
            //txty *= fabsf(sect->floor - sect->ceil) * 1 / (float)image->h;
        //txty *= 200.0f;
        //txty += image->h; 
        *pix = getpixel(image, txtx % image->w, (int)txty % image->h);
        pix += W;
    }
}