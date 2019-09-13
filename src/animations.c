#include "sdl_head.h"

void    draw_weapon_frame(SDL_Surface *screen, t_anim animation)
{
    int framew;
    int frameh;
    framew = animation.frames[(int)animation.frameCount]->w;
    frameh = animation.frames[(int)animation.frameCount]->h;
    int x = W / 2 - framew * 1.5 / 2;
    int y = H - frameh * 1.5;
    draw_image(screen, animation.frames[(int)animation.frameCount], x, y, framew * 1.5, frameh * 1.5);
}
