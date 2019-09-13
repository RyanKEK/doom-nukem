#include "sdl_head.h"

// void    draw_image(SDL_Surface *screen, SDL_Surface *image, int x, int y, int width, int height)
// {
//     int i = -1;
//     int j;
//     float tx;
//     float ty = 0;
//     int *pix = (int*)screen->pixels;
//     unsigned char r, g, b, a;
//     while (++i < height)
//     {
//         j = -1;
//         tx = 0;
//         while (++j < width)
//         {
//             SDL_GetRGBA(getpixel(image, (int)tx, (int)ty), image->format, &r, &g, &b, &a);
//             if (j + x < W && j + x > 0 && i + y < H && i + y > 0 && a != 0)
//                 putpixel(screen, j + x, i + y, getpixel(image, (int)tx, (int)ty));
//             tx += (float)image->w / width;
//         }
//         ty += (float)image->h / height;
//     }
// }

// void    draw_sprite(int x, int y, SDL_Surface *sprite)
// {
//         double vx = x - player.where.x, vy = y- player.where.y;
//         double pcos = player.anglecos, psin = player.anglesin;
//         double tx = vx * psin - vy * pcos,  tz = vx * pcos + vy * psin;
//         if (tz <= 0)
//             return ;
//         float xscale = (W*hfov) / (tz), yscale = (H*vfov) / (tz);    int x1 = W/2 + (int)(-tx * xscale);
//         int y1 = H/2 + (int)(-Yaw(1, tz) * yscale);
//         float dist = sqrtf(vx * vx + vy * vy);
//         draw_image(surface, sprite, x1, y1, 1000 / dist, 1000 / dist);
// }