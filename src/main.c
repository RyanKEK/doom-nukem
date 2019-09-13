#include "sdl_head.h"
#include "math.h"

typedef struct	s_line
{
	t_point		start;
	t_point		end;		
}				t_line;

typedef	struct	s_vector
{
	float		x;
	float		y;
}				t_vector;


typedef struct	s_plyer
{
	t_vector	pos;
	float		angle;
	t_vector	dir;
}				t_player;

void				build_player_direction(t_player *p)
{
	float			pov_x;
	float			pov_y;

	pov_x = cos(p->angle) * 15 + p->pos.x;
	pov_y = sin(p->angle) * 15 + p->pos.y;
	p->dir = (t_vector){pov_x, pov_y};
}


void				draw_line_render(SDL_Renderer *ren, t_line line, int r, int g, int b)
{
	SDL_SetRenderDrawColor(ren, r, g, b, 200);
	SDL_RenderDrawLine(ren, line.start.x, line.start.y, line.end.x, line.end.y);
}
void				draw_player_and_dir(SDL_Renderer *ren, t_player pl)
{
	t_line			dir;

	dir = (t_line){{round(pl.pos.x),round(pl.pos.y)}, {round(pl.dir.x), round(pl.dir.y)}};
	draw_line_render(ren, dir, 255, 255, 0);
	SDL_RenderDrawPoint(ren, pl.pos.x - 1, pl.pos.y - 1);
	SDL_RenderDrawPoint(ren, pl.pos.x + 1, pl.pos.y + 1);
	SDL_RenderDrawPoint(ren, pl.pos.x + 1, pl.pos.y - 1);
	SDL_RenderDrawPoint(ren, pl.pos.x - 1, pl.pos.y);
	SDL_RenderDrawPoint(ren, pl.pos.x + 1, pl.pos.y);
	SDL_RenderDrawPoint(ren, pl.pos.x, pl.pos.y - 1);
	SDL_RenderDrawPoint(ren, pl.pos.x, pl.pos.y + 1);

}

void				line_at_center_up(t_point win_size, t_line *l)
{
	int				s_x;
	int				s_y;

	s_x = win_size.x / 4;
	s_y = win_size.y / 4;
	l->start = (t_point){s_x, s_y};
	l->end = (t_point){s_x * 3, s_y};
}

void 				draw_circle(SDL_Renderer * renderer, int centreX, int centreY, int radius)
{
   const int32_t diameter = (radius * 2);

   int x = (radius - 1);
   int y = 0;
   int tx = 1;
   int ty = 1;
   int error = (tx - diameter);

   while (x >= y)
   {
      SDL_RenderDrawPoint(renderer, centreX + x, centreY - y);
      SDL_RenderDrawPoint(renderer, centreX + x, centreY + y);
      SDL_RenderDrawPoint(renderer, centreX - x, centreY - y);
      SDL_RenderDrawPoint(renderer, centreX - x, centreY + y);
      SDL_RenderDrawPoint(renderer, centreX + y, centreY - x);
      SDL_RenderDrawPoint(renderer, centreX + y, centreY + x);
      SDL_RenderDrawPoint(renderer, centreX - y, centreY - x);
      SDL_RenderDrawPoint(renderer, centreX - y, centreY + x);

      if (error <= 0)
      {
         ++y;
         error += ty;
         ty += 2;
      }

      if (error > 0)
      {
         --x;
         tx += 2;
         error += (tx - diameter);
      }

   }
}


float				fn_cross(float x1,float y1, float x2,float y2)
{
	return (x1 * y2 - y1 * x2);
}

void				intersect(float x1,float y1,  float x2,float y2,  float x3,float y3,  float x4,float y4,  float *x,float *y)
{
	float			det;

	*x = fn_cross(x1, y1, x2, y2);
	*y = fn_cross(x3, y3, x4, y4);
	det = fn_cross(x1-x2, y1-y2, x3 - x4, y3 - y4);
	*x = fn_cross(*x, x1 - x2, *y, x3 - x4) / det;
	*y = fn_cross(*x, y1 - y2, *y, y3 - y4) / det;
}

void 				change_line(SDL_Renderer *ren, t_line *l, t_player p, t_line o)
{
	float				x1;
	float				y1a;
	float				y1b;

	float				x2;
	float				y2a;
	float				y2b;
	float				dist;

	float				tx1;
	float				tx2;
	float				ty1;
	float				ty2;
	float				tz1;
	float				tz2;
	double			sin_angl;
	double			cos_angl;

	sin_angl = sin(p.angle);
	cos_angl = cos(p.angle);

	tx1 = o.start.x - (p.pos.x + 120); ty1 = o.start.y - p.pos.y;
	tx2 = o.end.x - (p.pos.x + 120); ty2 = o.end.y - p.pos.y;

	tz1 = tx1 * cos_angl + ty1 * sin_angl;
	tz2 = tx2 * cos_angl + ty2 * sin_angl;

	tx1 = tx1 * sin_angl - ty1 * cos_angl;
	tx2 = tx2 * sin_angl - ty2 * cos_angl;

	l->start = (t_point){(50 + 120) - tx1, 50 - tz1};
	l->end = (t_point){(50 + 120) - tx2, 50 - tz2};

	draw_line_render(ren, *l, 255,225,0);

	dist = 50 + 240;
	
	if (tz1 > 0 || tz2 > 0) 
	{
		float		ix1;
		float		iz1;
		float		ix2;
		float		iz2;
		
		intersect(tx1,tz1, tx2,tz2, -0.0001,0.0001, -20,5, &ix1, &iz1);
		intersect(tx1,tz1, tx2,tz2,  0.0001,0.0001,  20,5, &ix2, &iz2);
		if (tz1 <= 0)
		{
			if (iz1 > 0)
			{
				tx1 = ix1;
				tz1 = iz1;
			}
			else
			{
				tx1 = ix2;
				tz1 = iz2;
			}
		}
		if (tz2 <= 0)
		{
			if (iz1 > 0)
			{
				tx2 = ix1;
				tz2 = iz1;
			}
			else
			{
				tx2 = ix2;
				tz2 = iz2;
			}
		}
		x1 = -tx1 * 16 / tz1; y1a = -50 / tz1; y1b = 50 / tz1;
		x2 = -tx2 * 16 / tz2; y2a = -50 / tz2; y2b = 50 / tz2;


		SDL_SetRenderDrawColor(ren, 100, 220, 255, 255);
		SDL_RenderDrawLine(ren, dist + x1, 50 + y1a, dist + x2, 50 + y2a);
		SDL_RenderDrawLine(ren, dist + x1, 50 + y1b, dist + x2, 50 + y2b);
		SDL_RenderDrawLine(ren, dist + x1, 50 + y1a, dist + x1, 50 + y1b);
		SDL_RenderDrawLine(ren, dist + x2, 50 + y2a, dist + x2, 50 + y2b);
	}
}

void				rotate_by_player(t_line line, t_player player, SDL_Renderer *r)
{
	float			x1;
	float			y1;
	float			z1;
	float			x2;
	float			y2;
	float			z2;
	float			cos_angle;
	float			sin_angle;

	cos_angle = cos(player.angle);
	sin_angle = sin(player.angle);

	x1 = line.start.x - player.pos.x;
	y1 = line.start.y - player.pos.y;
	z1 = x1 * cos_angle + y1 * sin_angle;
	x1 = x1 * sin_angle + y1 * cos_angle;

	x2 = line.end.x - player.pos.x;
	y2 = line.end.y - player.pos.y;
	z2 = x2 * cos_angle + y2 * sin_angle;
	x2 = x2 * sin_angle + y2 * cos_angle;

	line.start = (t_point){round(x1), round(z1)};
	line.end = (t_point){round(x2), round(z2)};
	draw_line_render(r, line, 255,225,0);
}

int					game_loop3(t_sdl *s, t_line *line, int n_lines, t_player player)
{
	SDL_Event		e;
	int				run;
//	t_line			original_line;
	SDL_Texture		*tex;

	tex = new_fresh_texture(s->ren, s->win_size.x, s->win_size.y);
	if (!tex)
		return (SDL_FALSE);
//	original_line = line;
	run = 1;
	while (run)
	{
		SDL_SetRenderDrawColor(s->ren, 5, 5, 5, 255);
		SDL_RenderClear(s->ren);
		SDL_RenderCopy(s->ren, tex, NULL, NULL);
		for(int i = 0; i < n_lines; i++)
		{
			rotate_by_player(line[i], player, s->ren);
	//		draw_line_render(s->ren, line[i], 255,255,255);
		}
		SDL_RenderPresent(s->ren);
		while(SDL_PollEvent(&e))
		{
			if (e.type == SDL_QUIT)
				run = 0;
			else if (e.type == SDL_KEYDOWN)
			{
				if (e.key.keysym.sym == SDLK_w || e.key.keysym.sym == SDLK_UP)
					player.pos = (t_vector){player.pos.x + cos(player.angle), player.pos.y + sin(player.angle)};
				else if (e.key.keysym.sym == SDLK_s || e.key.keysym.sym == SDLK_DOWN)
					player.pos = (t_vector){player.pos.x - cos(player.angle), player.pos.y - sin(player.angle)};
				else if (e.key.keysym.sym == SDLK_LEFT)
					player.angle -= 0.1;
				else if (e.key.keysym.sym == SDLK_RIGHT)
					player.angle += 0.1;
				else if (e.key.keysym.sym == SDLK_a)
					player.pos = (t_vector){player.pos.x + sin(player.angle), player.pos.y - cos(player.angle)};
				else if (e.key.keysym.sym == SDLK_d)
					player.pos = (t_vector){player.pos.x - sin(player.angle), player.pos.y + cos(player.angle)};
				else if (e.key.keysym.sym == SDLK_ESCAPE)
				{
					run = 0;
					break ;
				}
				else if(e.key.keysym.sym == SDLK_i)
				{
				//	printf("moving line: start x = %d, y = %d\nend x = %d, y = %d\n", move_line.start.x, move_line.start.y, move_line.end.x, move_line.end.y);
					break ;
				}
			}
			break ;
		}
	}
	SDL_DestroyTexture(tex);
	return (SDL_TRUE);
}

int					game_loop2(t_sdl *s)
{
	SDL_Event		e;
	int				run;
	SDL_Texture		*tex;
	SDL_Rect		area;
	t_line			line;
	t_line			p_line;
	t_player		player;
	
	SDL_Rect		area2;
	t_line			move_line;
	t_line			st_dir;
	t_line			origin_line;

	SDL_Rect		area3;

	area = (SDL_Rect){0, 2,100,150};
	line = (t_line){{.x = 70, .y = 20},{.x = 70, .y = 70}};
	player.pos = (t_vector){.x = 50, .y = 50};
	player.angle = 0;
	p_line.start = (t_point){player.pos.x, player.pos.y};
	p_line.end = (t_point){cos(player.angle) * 5 + player.pos.x, sin(player.angle) * 5 + player.pos.y};

	area2 = (SDL_Rect){120, 2, 100, 150};
	st_dir = (t_line){{.x = 120+50,.y =  50},{.x =120+50, .y = 45}};
	move_line = line;
	move_line.start.x += 120;
	move_line.end.x += 120;

	area3 = area2;
	area3.x += 120;
	tex = new_fresh_texture(s->ren, s->win_size.x, s->win_size.y);
	if (!tex)
		return (SDL_FALSE);
	run = 1;
	while (run)
	{

		
		SDL_SetRenderDrawColor(s->ren, 5, 5, 5, 255);
		SDL_RenderClear(s->ren);
		SDL_RenderCopy(s->ren, tex, NULL, NULL);

		p_line.start = (t_point){player.pos.x, player.pos.y};
		p_line.end = (t_point){cos(player.angle) * 5 + player.pos.x, sin(player.angle) * 5 + player.pos.y};
		change_line(s->ren, &move_line, player, origin_line);

		SDL_SetRenderDrawColor(s->ren, 255,255,255,255);
		SDL_RenderDrawRect(s->ren, &area);
		SDL_RenderDrawRect(s->ren, &area2);
		SDL_RenderDrawRect(s->ren, &area3);

		draw_line_render(s->ren, line, 100, 75, 190);
		draw_line_render(s->ren, p_line, 255, 255, 0);
		draw_circle(s->ren, player.pos.x, player.pos.y, 2);

		draw_line_render(s->ren, st_dir, 130, 200, 100);
	//	draw_line_render(s->ren, move_line, 220, 255, 35);
		draw_circle(s->ren, 50 + 120, 50, 2);

		SDL_RenderPresent(s->ren);
		while(SDL_PollEvent(&e))
		{
			if (e.type == SDL_QUIT)
				run = 0;
			else if (e.type == SDL_KEYDOWN)
			{
				if (e.key.keysym.sym == SDLK_w || e.key.keysym.sym == SDLK_UP)
					player.pos = (t_vector){player.pos.x + cos(player.angle), player.pos.y + sin(player.angle)};
				else if (e.key.keysym.sym == SDLK_s || e.key.keysym.sym == SDLK_DOWN)
					player.pos = (t_vector){player.pos.x - cos(player.angle), player.pos.y - sin(player.angle)};
				else if (e.key.keysym.sym == SDLK_LEFT)
					player.angle -= 0.05;
				else if (e.key.keysym.sym == SDLK_RIGHT)
					player.angle += 0.05;
				else if (e.key.keysym.sym == SDLK_a)
					player.pos = (t_vector){player.pos.x + sin(player.angle), player.pos.y - cos(player.angle)};
				else if (e.key.keysym.sym == SDLK_d)
					player.pos = (t_vector){player.pos.x - sin(player.angle), player.pos.y + cos(player.angle)};
				else if (e.key.keysym.sym == SDLK_ESCAPE)
				{
					run = 0;
					break ;
				}
				else if(e.key.keysym.sym == SDLK_i)
				{
					printf("moving line: start x = %d, y = %d\nend x = %d, y = %d\n", move_line.start.x, move_line.start.y, move_line.end.x, move_line.end.y);
					break ;
				}
			}
			break ;
		}
	}
	SDL_DestroyTexture(tex);
	return (SDL_TRUE);
}


int					game_loop(t_sdl *s, t_player player)
{
	SDL_Event		e;
	int				run;
	SDL_Texture		*tex;
	t_line			line;
	t_player		stat_pl;

	line = (t_line){};
	build_player_direction(&player);
	line_at_center_up(s->win_size, &line);
	stat_pl = player;
	stat_pl.dir.x = player.pos.x;
	stat_pl.dir.y = player.pos.y - 15;
	tex = new_fresh_texture(s->ren, s->win_size.x, s->win_size.y);
	if (!tex)
		return (SDL_FALSE);
	run = 1;
	while (run)
	{
		SDL_SetRenderDrawColor(s->ren, 5, 5, 5, 255);
		SDL_RenderClear(s->ren);
		SDL_RenderCopy(s->ren, tex, NULL, NULL);
		draw_line_render(s->ren, line, 100, 75, 190);
		draw_player_and_dir(s->ren, player);
		SDL_RenderPresent(s->ren);
		while(SDL_PollEvent(&e))
		{
			if (e.type == SDL_QUIT)
				run = 0;
			else if (e.type == SDL_KEYDOWN)
			{
				if (e.key.keysym.sym == SDLK_RIGHT)
					player.angle += 0.1;
				else if (e.key.keysym.sym == SDLK_LEFT)
					player.angle -= 0.1;
				else if (e.key.keysym.sym == SDLK_UP)
				{
					player.pos.x += (player.dir.x - player.pos.x) / 4;
					player.pos.y += (player.dir.y - player.pos.y) / 4;
				}
				else if (e.key.keysym.sym == SDLK_DOWN)
				{
					player.pos.x += (player.pos.x - player.dir.x) / 4;
					player.pos.y += (player.pos.y - player.dir.y) / 4;
				}
				else if (e.key.keysym.sym == SDLK_ESCAPE)
				{
					run = 0;
					break ;
				}
				build_player_direction(&player);
			}
			break ;
		}
	}
	SDL_DestroyTexture(tex);
	return (SDL_TRUE);
}

int					main()
{
	t_sdl			*sdl;
	t_player		player;
	t_line			lines[3];

	sdl = new_t_sdl(620, 480, "Lines");
	player = (t_player){};
	player.angle = 90;
	player.pos = (t_vector){sdl->win_size.x / 2, sdl->win_size.y / 2};
	lines[0] = (t_line){{50, 50}, {300, 50}};
	lines[1] = (t_line){{73, 330}, {563, 330}};
	if (init_sdl(sdl))
		game_loop2(sdl);
	else
		ft_putendl("Failde to init sdl");
	free_t_sdl(sdl);
}