#ifndef __SDL_HEAD_H
#define __SDL_HEAD_H

#include "libft.h"
#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#define W 1600
#define H 900
#define OK 1
#define ERROR 0
#define min(a,b)             (((a) < (b)) ? (a) : (b)) // min: Choose smaller of two scalars.
#define max(a,b)             (((a) > (b)) ? (a) : (b)) // max: Choose greater of two scalars.
#define clamp(a, mi,ma)      min(max(a,mi),ma)
#define vxs(x0,y0, x1,y1)    ((x0)*(y1) - (x1)*(y0))   // vxs: Vector cross product
// Overlap:  Determine whether the two number ranges overlap.
#define Overlap(a0,a1,b0,b1) (min(a0,a1) <= max(b0,b1) && min(b0,b1) <= max(a0,a1))
// IntersectBox: Determine whether two 2D-boxes intersect.
#define IntersectBox(x0,y0, x1,y1, x2,y2, x3,y3) (Overlap(x0,x1,x2,x3) && Overlap(y0,y1,y2,y3))
// PointSide: Determine which side of a line the point is on. Return value: <0, =0 or >0.
#define PointSide(px,py, x0,y0, x1,y1) vxs((x1)-(x0), (y1)-(y0), (px)-(x0), (py)-(y0))
// Intersect: Calculate the point of intersection between two lines.
#define Intersect(x1,y1, x2,y2, x3,y3, x4,y4) ((struct vertex) { \
    vxs(vxs(x1,y1, x2,y2), (x1)-(x2), vxs(x3,y3, x4,y4), (x3)-(x4)) / vxs((x1)-(x2), (y1)-(y2), (x3)-(x4), (y3)-(y4)), \
    vxs(vxs(x1,y1, x2,y2), (y1)-(y2), vxs(x3,y3, x4,y4), (y3)-(y4)) / vxs((x1)-(x2), (y1)-(y2), (x3)-(x4), (y3)-(y4)) })
#define dtor(a)         (a * 0.0174533)
#define rtod(a)         (a * 57.2958)
#define distance(x1, y1 ,x2, y2) (sqrtf((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)))

typedef struct		s_point
{
	int				x;
	int				y;
}					t_point;

typedef struct		s_fpoint
{
	float			x;
	float			y;
}					t_fpoint;

typedef struct		point3D 
{
	float			x;
	float			y;
	float			z;
}					point3D;


typedef	struct		s_sdl
{
	t_point			win_size;
	char			*title;
	SDL_Window		*win;
	SDL_Renderer	*ren;
}					t_sdl;

struct sector
{
    float floor, ceil;
    struct vertex { float x,y,ceilz, floorz; } *vertex; // Each vertex has an x and y coordinate
    signed char *neighbors;           // Each edge may have a corresponding neighboring sector
    unsigned npoints;                 // How many vertexes there are
};

typedef struct      s_anim
{
    SDL_Surface *frames[10];
    float frameCount;
}                   t_anim;

typedef struct      s_enemy
{
    point3D pos;
    float speed;
	float height;
    float hp;
    int sector;
	Uint8 damaged;
	Uint8 moving;
	Uint8 dead;
	t_anim standing;
	t_anim walking;
	t_anim dying;
	t_anim taking_damage;
}                   t_enemy;

struct Scaler { int result, bop, fd, ca, cache; };

//					CREATE
t_sdl				*new_t_sdl(int win_size_x, int win_size_y, const char *title);
int					init_sdl(t_sdl *sdl);

//					INFO
int					error_message(const char *error);
void				*error_message_null(const char *error);

//					IMAGES
SDL_Surface			*load_jpg_png(const char *file_name);
SDL_Surface			*load_bmp(const char *file_name);
SDL_Surface			*load_optimize_bmp(const char *file_name);
SDL_Surface			*load_img(const char *file_name);

//					TEXTURE
SDL_Texture			*load_texture(const char *file_name, SDL_Renderer *ren);
SDL_Texture			*texture_from_surf(SDL_Surface *surf, SDL_Renderer *ren);
SDL_Texture			*new_fresh_texture(SDL_Renderer *ren, int width, int height);
void				textLine(int x, int y1,int y2, struct Scaler ty,unsigned txtx, struct sector *sect, SDL_Surface *surface, SDL_Surface *image);
Uint32              getpixel(SDL_Surface *surface, int x, int y);
void				filledLine(SDL_Surface *screen, SDL_Surface *image, int screenX, int xStart, int xEnd, int top, int bottom);

//					RENDER
void				sdl_render(SDL_Renderer *ren, SDL_Texture *tex, SDL_Rect *src, SDL_Rect *dst);

//					FONT AND TEXT

SDL_Texture			*make_color_text(SDL_Renderer *ren, const char *str, const char *file_font, int size, SDL_Color col);
SDL_Texture			*make_black_text(SDL_Renderer *ren, const char *str, const char *path_to_font, int size);

//					DESTROY
void				quit_sdl();
void				free_t_sdl(t_sdl *s);
void				close_t_sdl(t_sdl *s);
void				line(SDL_Surface *surface, t_point start, t_point end, int color);
void    			quad(SDL_Surface *surface, int x, int y, int w, int h, int color);
void                swap(int *a, int *b);

//                  SPRITES
void                draw_image(SDL_Surface *screen, SDL_Surface *image, int x, int y, int width, int height);
void                draw_sprite(float x, float y, SDL_Surface *sprite);
void                draw_weapon_frame(SDL_Surface *screen, t_anim animation);

#endif	