#include "sdl_head.h"
#include <stdio.h>
/* Define window size */
/* Define various vision related constants */
#define EyeHeight  10    // Camera height from floor when standing
#define DuckHeight 2.5  // And when crouching
#define HeadMargin 1    // How much room there is above camera before the head hits the ceiling
#define KneeHeight 2    // How tall obstacles the player can simply walk over without jumping
#define hfov (1.0 * 0.73f*H/W)
#define vfov (1.0 * .2f)
#define scale 20

/* Sectors: Floor and ceiling height; list of edge vertices and neighbors */
struct sector *sectors = NULL;
static unsigned NumSectors = 0;

#define Scaler_Init(a,b,c,d,f) \
    { d + (b-1 - a) * (f-d) / (c-a), ((f<d) ^ (c<a)) ? -1 : 1, \
      abs(f-d), abs(c-a), (int)((b-1-a) * abs(f-d)) % abs(c-a) }
/* Player: location */
static struct player
{
    struct xyz { float x,y,z; } where,      // Current position
                                velocity;   // Current motion vector
    float angle, anglesin, anglecos, yaw;   // Looking towards (and sin() and cos() thereof)
    unsigned sector;                        // Which sector the player is currently in
} player;

void    line(SDL_Surface *surface, t_point start, t_point end, int color);

typedef struct  s_point3D
{
    float x;
    float y;
    float z;
}               t_point3D;

typedef struct	s_point2D
{
	float x;
	float y;
}				t_point2D;

typedef struct  s_projectile
{
	float speed;
	float anglecos;
	float anglesin;
	float yaw;
	t_fpoint pos;
	float z;
	t_anim anim;
}				t_projectile;

// Utility functions. Because C doesn't have templates,
// we use the slightly less safe preprocessor macros to
// implement these functions that work with multiple types.
#define min(a,b)             (((a) < (b)) ? (a) : (b)) // min: Choose smaller of two scalars.
#define max(a,b)             (((a) > (b)) ? (a) : (b)) // max: Choose greater of two scalars.
#define clamp(a, mi,ma)      min(max(a,mi),ma)
#define Yaw(y,z) (y + z*player.yaw)
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
#define CeilingFloorScreenCoordinatesToMapCoordinates(mapY, screenX,screenY, X,Z) \
            do { Z = (mapY)*H*vfov / ((H/2 - (screenY)) - player.yaw*H*vfov); \
                 X = (Z) * (W/2 - (screenX)) / (W*hfov); \
                RelativeMapCoordinatesToAbsoluteOnes(X,Z); } while(0)
#define RelativeMapCoordinatesToAbsoluteOnes(X,Z) \
            do { float rtx = (Z) * pcos + (X) * psin; \
                 float rtz = (Z) * psin - (X) * pcos; \
         X = rtx + player.where.x; Z = rtz + player.where.y; \
         } while(0)
char *map;
SDL_Surface *imageSrf;
SDL_Surface *floorTexture;
SDL_Surface *ceilTexture;
SDL_Surface *skybox;
t_enemy demons[5];
int enemyMove = 0;
int sx1 = 0;
int sx2 = 0;

static void ReloadData()
{
    FILE* fp = fopen(map, "rt");
    if(!fp) { perror(map); exit(1); }
    char Buf[256], word[256], *ptr;
    struct vertex* vert = NULL, v;
    int n, m, NumVertices = 0;
    while(fgets(Buf, sizeof Buf, fp))
        switch(sscanf(ptr = Buf, "%32s%n", word, &n) == 1 ? word[0] : '\0')
        {
            case 'v': // vertex
                for(sscanf(ptr += n, "%f%n", &v.y, &n); sscanf(ptr += n, "%f%n", &v.x, &n) == 1;){
                    sscanf(ptr += n, "%f%n", &v.ceilz, &n);
                    sscanf(ptr += n, "%f%n", &v.floorz, &n);
                    vert = realloc(vert, ++NumVertices * sizeof(*vert)); vert[NumVertices-1] = v;
                }
                break;
            case 's': // sector
                sectors = realloc(sectors, ++NumSectors * sizeof(*sectors));
                struct sector* sect = &sectors[NumSectors-1];
                int* num = NULL;
                sscanf(ptr += n, "%f%f%n", &sect->floor,&sect->ceil, &n);
                for(m=0; sscanf(ptr += n, "%32s%n", word, &n) == 1 && word[0] != '#'; )
                    { num = realloc(num, ++m * sizeof(*num)); num[m-1] = word[0]=='x' ? -1 : atoi(word); }
                sect->npoints   = m /= 2;
                sect->neighbors = malloc( (m  ) * sizeof(*sect->neighbors) );
                sect->vertex    = malloc( (m+1) * sizeof(*sect->vertex)    );
                for(n=0; n<m; ++n) sect->neighbors[n] = num[m + n];
                for(n=0; n<m; ++n) sect->vertex[n+1]  = vert[num[n]]; // TODO: Range checking
                sect->vertex[0] = sect->vertex[m]; // Ensure the vertexes form a loop
                free(num);
                break;
        }
    fclose(fp);
    free(vert);
}

static void LoadData()
{
    FILE* fp = fopen(map, "rt");
    if(!fp) { perror(map); exit(1); }
    char Buf[256], word[256], *ptr;
    struct vertex* vert = NULL, v;
    int n, m, NumVertices = 0;
    while(fgets(Buf, sizeof Buf, fp))
        switch(sscanf(ptr = Buf, "%32s%n", word, &n) == 1 ? word[0] : '\0')
        {
            case 'v': // vertex
                //printf("%sn:%d\n", ptr + n, n);
                for(sscanf(ptr += n, "%f%n", &v.y, &n); sscanf(ptr += n, "%f%n", &v.x, &n) == 1;){
                    sscanf(ptr += n, "%f%n", &v.ceilz, &n);
                    sscanf(ptr += n, "%f%n", &v.floorz, &n);
                    vert = realloc(vert, ++NumVertices * sizeof(*vert)); vert[NumVertices-1] = v;
                }
                break;
            case 's': // sector
                sectors = realloc(sectors, ++NumSectors * sizeof(*sectors));
                struct sector* sect = &sectors[NumSectors-1];
                int* num = NULL;
                sscanf(ptr += n, "%f%f%n", &sect->floor,&sect->ceil, &n);
                for(m=0; sscanf(ptr += n, "%32s%n", word, &n) == 1 && word[0] != '#'; )
                {
                        num = realloc(num, ++m * sizeof(*num));
                        num[m-1] = word[0]=='x' ? -1 : atoi(word);
                }
                sect->npoints   = m /= 2;
                sect->neighbors = malloc( (m  ) * sizeof(*sect->neighbors) );
                sect->vertex    = malloc( (m+1) * sizeof(*sect->vertex)    );
                for(n=0; n<m; ++n) sect->neighbors[n] = num[m + n];
                for(n=0; n<m; ++n) sect->vertex[n+1]  = vert[num[n]]; // TODO: Range checking
                sect->vertex[0] = sect->vertex[m]; // Ensure the vertexes form a loop
                free(num);
                break;
            case 'p':; // player
                float angle;
                sscanf(ptr += n, "%f %f %f %d", &v.x, &v.y, &angle,&n);
                player = (struct player) { {v.x, v.y, 0}, {0,0,0}, angle,0,0,0, n }; // TODO: Range checking
                player.where.z = sectors[player.sector].floor + EyeHeight;
        }
    fclose(fp);
    free(vert);
}

float zBuffer[H][W] = {0};

static void LoadDataRaw()
{
    FILE* fp = fopen(map, "rt");
    if(!fp) { perror(map); exit(1); }
    char Buf[256], word[256], *ptr;
    struct vertex* vert = NULL, v;
    int n, m, NumVertices = 0;
    while(fgets(Buf, sizeof Buf, fp))
        switch(sscanf(ptr = Buf, "%32s%n", word, &n) == 1 ? word[0] : '\0')
        {
            case 'v': // vertex
                for(sscanf(ptr += n, "%f%n", &v.y, &n); sscanf(ptr += n, "%f%n", &v.x, &n) == 1; )
                    { vert = realloc(vert, ++NumVertices * sizeof(*vert)); vert[NumVertices-1] = v; }
                break;
            case 's': // sector
                sectors = realloc(sectors, ++NumSectors * sizeof(*sectors));
                struct sector* sect = &sectors[NumSectors-1];
                int* num = NULL;
                sscanf(ptr += n, "%f%f%n", &sect->floor,&sect->ceil, &n);
                for(m=0; sscanf(ptr += n, "%32s%n", word, &n) == 1 && word[0] != '#'; )
                    { num = realloc(num, ++m * sizeof(*num)); num[m-1] = word[0]=='x' ? -1 : atoi(word); }
                sect->npoints   = m /= 2;
                sect->neighbors = malloc( (m  ) * sizeof(*sect->neighbors) );
                sect->vertex    = malloc( (m+1) * sizeof(*sect->vertex)    );
                for(n=0; n<m; ++n) sect->neighbors[n] = num[m + n];
                for(n=0; n<m; ++n) sect->vertex[n+1]  = vert[num[n]]; // TODO: Range checking
                sect->vertex[0] = sect->vertex[m]; // Ensure the vertexes form a loop
                free(num);
                break;
            case 'p':; // player
                float angle;
                sscanf(ptr += n, "%f %f %f %d", &v.x, &v.y, &angle,&n);
                player = (struct player) { {v.x, v.y, 0}, {0,0,0}, angle,0,0,0, n }; // TODO: Range checking
                player.where.z = sectors[player.sector].floor + EyeHeight;
        }
    fclose(fp);
    free(vert);
}

static void UnloadData()
{
    for(unsigned a=0; a<NumSectors; ++a) free(sectors[a].vertex);
    for(unsigned a=0; a<NumSectors; ++a) free(sectors[a].neighbors);
    free(sectors);
    sectors    = NULL;
    NumSectors = 0;
}

static SDL_Surface* surface = NULL;

/* vline: Draw a vertical line on screen, with a different color pixel in top & bottom */
static void vline(int x, int y1, int y2, int top, int middle, int bottom)
{
    int *pix = (int*) surface->pixels;
    y1 = clamp(y1, 0, H-1);
    y2 = clamp(y2, 0, H-1);
    if(y2 == y1)
        pix[y1*W+x] = middle;
    else if(y2 > y1)
    {
        pix[y1*W+x] = top;
        for(int y=y1+1; y<y2; ++y) pix[y*W+x] = middle;
        pix[y2*W+x] = bottom;
    }
}

/* MovePlayer(dx,dy): Moves the player by (dx,dy) in the map, and
 * also updates their anglesin/anglecos/sector properties properly.
 */
static void MovePlayer(float dx, float dy)
{
    float px = player.where.x, py = player.where.y;
    /* Check if this movement crosses one of this sector's edges
     * that have a neighboring sector on the other side.
     * Because the edge vertices of each sector are defined in
     * clockwise order, PointSide will always return -1 for a point
     * that is outside the sector and 0 or 1 for a point that is inside.
     */
    const struct sector* const sect = &sectors[player.sector];
    const struct vertex* const vert = sect->vertex;
    for(unsigned s = 0; s < sect->npoints; ++s)
        if(sect->neighbors[s] >= 0
        && IntersectBox(px,py, px+dx,py+dy, vert[s+0].x, vert[s+0].y, vert[s+1].x, vert[s+1].y)
        && PointSide(px+dx, py+dy, vert[s+0].x, vert[s+0].y, vert[s+1].x, vert[s+1].y) < 0)
        {
            player.sector = sect->neighbors[s];
            break;
        }
    player.where.x += dx;
    player.where.y += dy;
    player.anglesin = sinf(player.angle);
    player.anglecos = cosf(player.angle);
}

void    moveEnemyToPlayer(t_enemy *enemy)
{
    float px = player.where.x, py = player.where.y;
    const struct sector* const sect = &sectors[enemy->sector];
    const struct vertex* const vert = sect->vertex;
    float vx = enemy->pos.x - px;
    float vy = enemy->pos.y - py;
	float dist = sqrtf(vx * vx + vy * vy);
	float dx = (dist - enemy->speed) / dist;
	t_fpoint newpos = {vx * dx + px, vy * dx + py};
    for(unsigned s = 0; s < sect->npoints; ++s)
        if(sect->neighbors[s] >= 0
        && IntersectBox(enemy->pos.x,enemy->pos.y, newpos.x, newpos.y, vert[s+0].x, vert[s+0].y, vert[s+1].x, vert[s+1].y)
        && PointSide(newpos.x, newpos.y, vert[s+0].x, vert[s+0].y, vert[s+1].x, vert[s+1].y) < 0)
        {
            enemy->sector = sect->neighbors[s];
            break;
        }
    enemy->pos.x = newpos.x;
    enemy->pos.y = newpos.y;
}

float anglle = -0.02f;
static int Scaler_Next(struct Scaler* i)
{
    for(i->cache += i->fd; i->cache >= i->ca; i->cache -= i->ca) i->result += i->bop;
    return i->result;
}

float         find_max_ceiling_height(const struct sector *const sector)
{
    float h;
    for (int i = 0; i < sector->npoints - 1; i++)
        h = max(sector->vertex[i].ceilz, sector->vertex[i + 1].ceilz);
    return (h);
}

int scaleH = 34;

void swapf(float *a, float *b)
{
    float tmp;

    tmp = *a;
    *a = *b;
    *b = tmp;
}

void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to set */
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
    case 1:
        *p = pixel;
        break;

    case 2:
        *(Uint16 *)p = pixel;
        break;

    case 3:
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            p[0] = (pixel >> 16) & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = pixel & 0xff;
        } else {
            p[0] = pixel & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = (pixel >> 16) & 0xff;
        }
        break;

    case 4:
        *(Uint32 *)p = pixel;
        break;
    }
}

void    draw_crosshair()
{
    t_point start1;
    t_point start2;
    t_point end1;
    t_point end2;
    start1.x = W / 2 - 30;
    start1.y = H / 2;
    end1.x = W / 2 + 30;
    end1.y = H / 2;
    start2.x = W / 2;
    start2.y = H / 2 - 30;
    end2.x = W / 2;
    end2.y = H / 2 + 30;
    line(surface, start1, end1, 0xffffffff);
    line(surface, start2, end2, 0xffffffff);
}

void	draw_custom_image(SDL_Surface *screen, SDL_Surface *image, int x, int y, int width, int height, int imx, int imy, int imw, int imh)
{
	int i = -1;
    int j;
    float tx;
    float ty = 0;
    int *pix = (int*)screen->pixels;
    unsigned char r, g, b, a;
    while (++i < height && i < H - 100)
    {
        j = -1;
        tx = 0;
        while (++j < width && j < W - 100)
        {
            SDL_GetRGBA(getpixel(image, (int)tx + imx, (int)ty + imy), image->format, &r, &g, &b, &a);
            if (j + x < W && j + x > 0 && i + y < H && i + y > 0 && a != 0)
                putpixel(screen, j + x, i + y, SDL_MapRGBA(screen->format, r, g, b, a));
            tx += (float)imw / width;
        }
        ty += (float)imh / height;
    }
}

void    draw_image(SDL_Surface *screen, SDL_Surface *image, int x, int y, int width, int height)
{
    int i = -1;
    int j;
    float tx;
    float ty = 0;
    int *pix = (int*)screen->pixels;
    unsigned char r, g, b, a;
    while (++i < height)
    {
        j = -1;
        tx = 0;
        while (++j < width)
        {
            SDL_GetRGBA(getpixel(image, (int)tx, (int)ty), image->format, &r, &g, &b, &a);
            if (j + x < W && j + x > 0 && i + y < H && i + y > 0 && a != 0)
                putpixel(screen, j + x, i + y, SDL_MapRGBA(screen->format, r, g, b, a));
			// else if (j + x < W && j + x > 0 && i + y < H && i + y > 0 && a == 0)
			// 	putpixel(screen, j + x, i + y, SDL_MapRGBA(screen->format, 255, 255, 255, 0));
            tx += (float)image->w / width;
        }
        ty += (float)image->h / height;
    }
}

void    draw_enemy_image(SDL_Surface *screen, SDL_Surface *image, int x, int y, int width, int height, int sx1, int sx2, int ytop[W], int ybottom[W])
{
    int i = -1;
    int j;
    float tx;
    float ty = 0;
    int *pix = (int*)screen->pixels;
    unsigned char r, g, b, a;
    while (++i < height)
    {
        j = -1;
        tx = 0;
        while (++j < width)
        {
            SDL_GetRGBA(getpixel(image, (int)tx, (int)ty), image->format, &r, &g, &b, &a);
            if (j + x < W && j + x > 0 && i + y < H && i + y > 0 && a != 0 && j + x > sx1 && j + x < sx2 && i + y < ybottom[j + x] && i + y > ytop[j + x])
                putpixel(surface, j + x, i + y, getpixel(image, (int)tx, (int)ty));
            tx += (float)image->w / width;
        }
        ty += (float)image->h / height;
    }
}

int		projectPoint(float *x, float *y, float *z)
{
        float vx = *x - player.where.x, vy = *y - player.where.y;
        float pcos = player.anglecos, psin = player.anglesin;
        float tx = vx * psin - vy * pcos,  tz = vx * pcos + vy * psin;
		if (tz <= 0)
			return (0);
        float xscale = (W*hfov) / (tz), yscale = (H*vfov) / (tz);
        *x = W/2 + (int)(-tx * xscale);
        *y = H/2 + (int)(-Yaw(*z - player.where.z, tz) * yscale);
		return (1);
}

void    draw_sprite(float x, float y, SDL_Surface *sprite)
{
        float vx = x - player.where.x, vy = y - player.where.y;
        float pcos = player.anglecos, psin = player.anglesin;
        float tx = vx * psin - vy * pcos,  tz = vx * pcos + vy * psin;
        if (tz <= 0)
            return ;
        float xscale = (W*hfov) / (tz), yscale = (H*vfov) / (tz);
        int x1 = W/2 + (int)(-tx * xscale);
        int y1 = H/2 + (int)(-Yaw(10 - player.where.z, tz) * yscale);
        float dist = sqrtf(vx * vx + vy * vy);
		float imScale = 1000 /dist;
        draw_image(surface, sprite, x1 - imScale / 2, y1 - imScale / 2, imScale, imScale);
}

void    draw_projecitle(t_projectile proj)
{
        float vx = proj.pos.x - player.where.x, vy = proj.pos.y - player.where.y;
        float pcos = player.anglecos, psin = player.anglesin;
        float tx = vx * psin - vy * pcos,  tz = vx * pcos + vy * psin;
        if (tz <= 0)
            return ;
        float xscale = (W*hfov) / (tz), yscale = (H*vfov) / (tz);
        int x1 = W/2 + (int)(-tx * xscale);
        int y1 = H/2 + (int)(-Yaw(proj.z - player.where.z, tz) * yscale);
        float dist = sqrtf(vx * vx + vy * vy);
		float imScale = 1000 /dist;
       	draw_image(surface, proj.anim.frames[(int)proj.anim.frameCount], x1 - imScale / 2, y1 - imScale / 2, imScale, imScale);
}

void    draw_enemy_sprite(t_enemy enemy, int sx1, int sx2, int ytop[W], int ybottom[W])
{
		t_anim anim;
		if (enemy.hp <= 0)
			anim = enemy.dying;
		else if (enemy.damaged)
			anim = enemy.taking_damage;
		else if (enemy.moving)
			anim = enemy.walking;
		else
			anim = enemy.standing;
		float vx = enemy.pos.x - player.where.x, vy = enemy.pos.y - player.where.y;
        float pcos = player.anglecos, psin = player.anglesin;
        float tx = vx * psin - vy * pcos,  tz = vx * pcos + vy * psin;
        if (tz <= 0)
            return ;
        float xscale = (W*hfov) / (tz), yscale = (H*vfov) / (tz);
        int x1 = W/2 + (int)(-tx * xscale);
        int y1 = H/2 + (int)(-Yaw(enemy.pos.z - player.where.z, tz) * yscale);
        float dist = sqrtf(vx * vx + vy * vy);
		float imScale = 1000 / dist * enemy.height / 4.0f;
		//float imScale = (-atan(dist) + 2) * 100 * enemy.height / 4.0f;
        float imYscale = y1 - imScale;
		float imXscale = x1 - imScale / 2;

		//printf("%f\n", dist);
		draw_enemy_image(surface, anim.frames[(int)anim.frameCount], imXscale, imYscale, imScale, imScale, sx1, sx2, ytop, ybottom);
		// t_point start = {imXscale, imYscale};
		// t_point end = {imXscale + imScale, imYscale};
		// line(surface, start, end, 0xffffffff);
		// start = (t_point){imXscale + imScale, imYscale};
		// end = (t_point){imXscale + imScale, imYscale  + imScale};
		// line(surface, start, end, 0xffffffff);
		// start = (t_point){imXscale + imScale, imYscale  + imScale};
		// end = (t_point){imXscale, imYscale + imScale};
		// line(surface, start, end, 0xffffffff);
		// start = (t_point){imXscale, imYscale + imScale};
		// end = (t_point){imXscale, imYscale};
		// line(surface, start, end, 0xffffffff);
}

void    draw_enemy_frame(t_enemy enemy, int sx1, int sx2, int ytop[W], int ybottom[W])
{
    draw_enemy_sprite(enemy, sx1, sx2, ytop, ybottom);
}

int enemy_hit(t_enemy enemy)
{
        double vx = enemy.pos.x - player.where.x, vy = enemy.pos.y - player.where.y;
        double pcos = player.anglecos, psin = player.anglesin;
        double tx = vx * psin - vy * pcos,  tz = vx * pcos + vy * psin;
        if (tz <= 0)
            return 0;
        float xscale = (W*hfov) / (tz), yscale = (H*vfov) / (tz);
        float dist = sqrtf(vx * vx + vy * vy);
		float imScale = 1000 / dist * enemy.height / 4;
        int x1 = W/2 + (int)(-tx * xscale) - imScale / 2;
        int y1 = H/2 + (int)(-Yaw(enemy.pos.z - player.where.z, tz) * yscale) - imScale;
        if (x1 < W / 2 && x1 + imScale > W / 2 && y1 < H / 2 && y1 + imScale > H / 2)
            return (1);
        else
            return (0);
}

float del = 5;

t_enemy enemy;

void rightTexturedTriangle(	int x1, int y1, float u1, float v1, float w1,
						int x2, int y2, float u2, float v2, float w2,
						int x3, int y3, float u3, float v3, float w3)
{
	if (y2 < y1)
	{
		swap(&y1, &y2);
		swap(&x1, &x2);
		swapf(&u1, &u2);
		swapf(&v1, &v2);
		swapf(&w1, &w2);
	}

	if (y3 < y1)
	{
		swap(&y1, &y3);
		swap(&x1, &x3);
		swapf(&u1, &u3);
		swapf(&v1, &v3);
		swapf(&w1, &w3);
	}

	if (y3 < y2)
	{
		swap(&y2, &y3);
		swap(&x2, &x3);
		swapf(&u2, &u3);
		swapf(&v2, &v3);
		swapf(&w2, &w3);
	}

	int dy1 = y2 - y1;
	int dx1 = x2 - x1;
	float dv1 = v2 - v1;
	float du1 = u2 - u1;
	float dw1 = w2 - w1;

	int dy2 = y3 - y1;
	int dx2 = x3 - x1;
	float dv2 = v3 - v1;
	float du2 = u3 - u1;
	float dw2 = w3 - w1;

	float tex_u, tex_v, tex_w;

	float dax_step = 0, dbx_step = 0,
		du1_step = 0, dv1_step = 0,
		du2_step = 0, dv2_step = 0,
		dw1_step=0, dw2_step=0;

	if (dy1) dax_step = dx1 / (float)abs(dy1);
	if (dy2) dbx_step = dx2 / (float)abs(dy2);

	if (dy1) du1_step = du1 / (float)abs(dy1);
	if (dy1) dv1_step = dv1 / (float)abs(dy1);
	if (dy1) dw1_step = dw1 / (float)abs(dy1);

	if (dy2) du2_step = du2 / (float)abs(dy2);
	if (dy2) dv2_step = dv2 / (float)abs(dy2);
	if (dy2) dw2_step = dw2 / (float)abs(dy2);

	if (dy1)
	{
		for (int i = y1; i <= y2; i++)
		{
			int ax = x1 + (float)(i - y1) * dax_step;
			int bx = x1 + (float)(i - y1) * dbx_step;

			float tex_su = u1 + (float)(i - y1) * du1_step;
			float tex_sv = v1 + (float)(i - y1) * dv1_step;
			float tex_sw = w1 + (float)(i - y1) * dw1_step;

			float tex_eu = u1 + (float)(i - y1) * du2_step;
			float tex_ev = v1 + (float)(i - y1) * dv2_step;
			float tex_ew = w1 + (float)(i - y1) * dw2_step;

			if (ax > bx)
			{
				swap(&ax, &bx);
				swapf(&tex_su, &tex_eu);
				swapf(&tex_sv, &tex_ev);
				swapf(&tex_sw, &tex_ew);
			}

			tex_u = tex_su;
			tex_v = tex_sv;
			tex_w = tex_sw;

			float tstep = 1.0f / ((float)(bx - ax));
			float t = 0.0f;

			for (int j = ax; j < bx; j++)
			{
				tex_u = (1.0f - t) * tex_su + t * tex_eu;
				tex_v = (1.0f - t) * tex_sv + t * tex_ev;
				tex_w = (1.0f - t) * tex_sw + t * tex_ew;
				if (j < W && j > 0 && i > 0 && i < H)
					putpixel(surface, j, i, getpixel(ceilTexture, (int)(tex_u * ceilTexture->w), (int)(tex_v * ceilTexture->h)));
				t += tstep;
			}

		}
	}

	dy1 = y3 - y2;
	dx1 = x3 - x2;
	dv1 = v3 - v2;
	du1 = u3 - u2;
	dw1 = w3 - w2;

	if (dy1) dax_step = dx1 / (float)abs(dy1);
	if (dy2) dbx_step = dx2 / (float)abs(dy2);

	du1_step = 0, dv1_step = 0;
	if (dy1) du1_step = du1 / (float)abs(dy1);
	if (dy1) dv1_step = dv1 / (float)abs(dy1);
	if (dy1) dw1_step = dw1 / (float)abs(dy1);

	if (dy1)
	{
		for (int i = y2; i <= y3; i++)
		{
			int ax = x2 + (float)(i - y2) * dax_step;
			int bx = x1 + (float)(i - y1) * dbx_step;

			float tex_su = u2 + (float)(i - y2) * du1_step;
			float tex_sv = v2 + (float)(i - y2) * dv1_step;
			float tex_sw = w2 + (float)(i - y2) * dw1_step;

			float tex_eu = u1 + (float)(i - y1) * du2_step;
			float tex_ev = v1 + (float)(i - y1) * dv2_step;
			float tex_ew = w1 + (float)(i - y1) * dw2_step;

			if (ax > bx)
			{
				swap(&ax, &bx);
				swapf(&tex_su, &tex_eu);
				swapf(&tex_sv, &tex_ev);
				swapf(&tex_sw, &tex_ew);
			}

			tex_u = tex_su;
			tex_v = tex_sv;
			tex_w = tex_sw;

			float tstep = 1.0f / ((float)(bx - ax));
			float t = 0.0f;

			for (int j = ax; j < bx; j++)
			{
				tex_u = (1.0f - t) * tex_su + t * tex_eu;
				tex_v = (1.0f - t) * tex_sv + t * tex_ev;
				tex_w = (1.0f - t) * tex_sw + t * tex_ew;
				if (j < W && j > 0 && i > 0 && i < H)
					putpixel(surface, j, i, getpixel(ceilTexture, (int)(tex_u * ceilTexture->w), (int)(tex_v * ceilTexture->h)));
				t += tstep;
			}
		}
	}
}

void    draw_vector(SDL_Surface *surface, t_fpoint start, t_fpoint end, int color)
{
    start.x *= 100;
    start.x += W / 2;
    start.y *= 100;
    start.y = H - (start.y + H / 2); 
    end.x *= 100;
    end.x += W / 2;
    end.y *= 100;
    end.y = H - (end.y + H / 2);
    t_point s = {start.x, start.y};
    t_point e = {end.x, end.y};
    line(surface, s, e, color);
}

void    vector_tests()
{
    t_point startX;
    t_point endX;
    t_point startY;
    t_point endY;
    t_fpoint enemy = {2, 2};
    t_fpoint player = {-4, 4};
    t_fpoint coordStart = {0, 0};
    startX.x = 0;
    startX.y = H / 2;
    endX.x = W - 1;
    endX.y = H / 2;
    startY.x = W / 2;
    startY.y = 0;
    endY.x = W / 2;
    endY.y = H - 1;
    line(surface, startX, endX, 0xffffffff);
    vline(W / 2, 0, H - 1, 0xffffffff, 0xffffffff, 0xffffffff);
	float vx = (enemy.x - player.x);
	float vy = (enemy.y - player.y);
	float dx = 10 / distance(player.x, player.y, enemy.x, enemy.y);
    t_fpoint vec = {dx * vx, dx * vy};
    t_fpoint vecs = {player.x, player.y - 2};
    t_fpoint vece = {vec.x + player.x, vec.y + player.y - 2};
    draw_vector(surface, enemy, player, 0x000000ff);
    draw_vector(surface, vecs, vece, 0x0000ff00);

}
int sdvig = 0;
int skyW = 0;

void    draw_quad(SDL_Surface *screen, t_point3D points[4], int color)
{
    t_point start;
    t_point end;
    t_point quad[4];
    int i = -1;
	t_point3D tri1[3] = {points[0], points[1], points[2]};
	t_point3D tri2[3] = {points[0], points[3], points[2]};
    if (projectPoint(&points[0].x, &points[0].y, &points[0].z)
    	&& projectPoint(&points[1].x, &points[1].y, &points[1].z)
    	&& projectPoint(&points[2].x, &points[2].y, &points[2].z)
    	&& projectPoint(&points[3].x, &points[3].y, &points[3].z)){
			t_point3D p1 = points[0];
			t_point3D p2 = points[1];
			t_point3D p3 = points[2];
			t_point3D p4 = points[3];
			rightTexturedTriangle(p1.x, p1.y, 0.0f, 0.0f, 1.0f, p2.x, p2.y, 1.0f, 0.0f, 1.0f, p3.x, p3.y, 1.0f, 1.0f, 1.0f);
			rightTexturedTriangle(p1.x, p1.y, 0.0f, 0.0f, 1.0f, p4.x, p4.y, 0.0f, 1.0f, 1.0f, p3.x, p3.y, 1.0f, 1.0f, 1.0f);
	}
    // while (++i < 3)
    // {
    //     start = projectPoint(points[i].x, points[i].y, points[i].z);
    //     quad[i] = start;
    //     end = projectPoint(points[i + 1].x, points[i + 1].y, points[i + 1].z);
    //     line(screen, start, end, color);
    // }
    // start = projectPoint(points[i].x, points[i].y, points[i].z);
    // quad[i] = start;
    // end = projectPoint(points[0].x, points[0].y, points[0].z);
    // line(screen, start, end, color);
}

int			picked_up(t_fpoint	pos)
{
	if (player.where.x > pos.x - 2 && player.where.x < pos.x + 2
		&& player.where.y > pos.y - 2 && player.where.y < pos.y + 2){
			return(1);
	}
	return (0);
}

static void DrawScreen(t_sdl *sdl)
{
    enum { MaxQueue = 32 };  // maximum number of pending portal renders
    struct item { int sectorno,sx1,sx2; } queue[MaxQueue], *head=queue, *tail=queue;
    int ytop[W]={0}, ybottom[W], renderedsectors[NumSectors];
    for(unsigned x=0; x<W; ++x) ybottom[x] = H-1;
    for(unsigned n=0; n<NumSectors; ++n) renderedsectors[n] = 0;

    /* Begin whole-screen rendering from where the player is. */
    *head = (struct item) { player.sector, 0, W-1 };
    if(++head == queue+MaxQueue) head = queue;
    do {
    /* Pick a sector & slice from the queue to draw */
    const struct item now = *tail;
    if(++tail == queue+MaxQueue) tail = queue;

    if(renderedsectors[now.sectorno] & 0x21) continue; // Odd = still rendering, 0x20 = give up
    ++renderedsectors[now.sectorno];
    const struct sector* const sect = &sectors[now.sectorno];
    /* Render each wall of this sector that is facing towards player. */
	SDL_Texture *text;
    for(unsigned s = 0; s < sect->npoints; ++s)
    {
		/* Acquire the x,y coordinates of the two endpoints (vertices) of this edge of the sector */
       // printf("angle%f  cos:%f   sin:%f\n", player.angle, player.anglecos, player.anglesin);
		float ceiloff = find_max_ceiling_height(sect);
        float vx1 = sect->vertex[s].x - player.where.x, vy1 = sect->vertex[s].y - player.where.y;
        float vx2 = sect->vertex[s+1].x - player.where.x, vy2 = sect->vertex[s+1].y - player.where.y;

        /* Rotate them around the player's view */
        float pcos = player.anglecos, psin = player.anglesin;
        float tx1 = vx1 * psin - vy1 * pcos,  tz1 = vx1 * pcos + vy1 * psin;
        float tx2 = vx2 * psin - vy2 * pcos,  tz2 = vx2 * pcos + vy2 * psin;
        /* Is the wall at least partially in front of the player? */
        if(tz1 <= 0 && tz2 <= 0)
			continue;
        struct vertex orig1, orig2;
        /* If it's partially behind the player, clip it against player's view frustrum */
        float scaleL;
        if(fabsf(sect->vertex[s].x - sect->vertex[s + 1].x) > fabsf(sect->vertex[s].y - sect->vertex[s + 1].y))
            scaleL = fabsf(sect->vertex[s].x - sect->vertex[s + 1].x) / del;
        else
            scaleL = fabsf(sect->vertex[s].y - sect->vertex[s + 1].y) / del;
        int u0 = 0, u1 = imageSrf->w * scaleL - 1;
        if(tz1 <= 0 || tz2 <= 0)
        {
            float nearz = 1e-4f, farz = 5, nearside = 1e-5f, farside = 20.f;
            // Find an intersection between the wall and the approximate edges of player's view
            struct vertex i1 = Intersect(tx1,tz1,tx2,tz2, -nearside,nearz, -farside,farz);
            struct vertex i2 = Intersect(tx1,tz1,tx2,tz2,  nearside,nearz,  farside,farz);
            struct vertex org1 = {tx1,tz1}, org2 = {tx2,tz2};
            orig1 = org1;
            orig2 = org2;
            if(tz1 < nearz) { if(i1.y > 0) { tx1 = i1.x; tz1 = i1.y; } else { tx1 = i2.x; tz1 = i2.y; } }
            if(tz2 < nearz) { if(i1.y > 0) { tx2 = i1.x; tz2 = i1.y; } else { tx2 = i2.x; tz2 = i2.y; } }
            if(fabs(tx2-tx1) > fabs(tz2-tz1))
                u0 = (tx1-org1.x) * (imageSrf->w * scaleL - 1) / (org2.x-org1.x), u1 = (tx2-org1.x) * (imageSrf->w * scaleL - 1) / (org2.x-org1.x);
            else
                u0 = (tz1-org1.y) * (imageSrf->w * scaleL - 1) / (org2.y-org1.y), u1 = (tz2-org1.y) * (imageSrf->w * scaleL - 1) / (org2.y-org1.y);
        }
        /* Do perspective transformation */
        float xscale1 = (W*hfov) / (tz1), yscale1 = (H*vfov) / (tz1);    int x1 = W/2 + (int)(-tx1 * xscale1);
        float xscale2 = (W*hfov) / (tz2), yscale2 = (H*vfov) / (tz2);    int x2 = W/2 + (int)(-tx2 * xscale2);
		if(x1 >= x2 || x2 < now.sx1 || x1 > now.sx2)
			continue; // Only render if it's visible
        sx1 = now.sx1;
        sx2 = now.sx2;
        /* Acquire the floor and ceiling heights, relative to where the player's view is */
        float yceil  = sect->ceil  - player.where.z;
        float yfloor = sect->floor - player.where.z;
        /* Check the edge type. neighbor=-1 means wall, other=boundary between two sectors. */
        int neighbor = sect->neighbors[s];
        float nyceil=0, nyfloor=0;
        if(neighbor >= 0) // Is another sector showing through this portal?
        {
            nyceil  = sectors[neighbor].ceil  - player.where.z;
            nyfloor = sectors[neighbor].floor - player.where.z;
        }
        /* Project our ceiling & floor heights into screen coordinates (Y coordinate) */
        int y1a  = H/2 + (-Yaw(yceil + sect->vertex[s].ceilz, tz1) * yscale1),  y1b = H/2 + (-Yaw(yfloor + sect->vertex[s].floorz, tz1) * yscale1);
        int y2a  = H/2 + (-Yaw(yceil + sect->vertex[s + 1].ceilz, tz2) * yscale2),  y2b = H/2 + (-Yaw(yfloor + sect->vertex[s + 1].floorz, tz2) * yscale2);
        /* The same for the neighboring sector */
        int ny1a = H/2 + (-Yaw(nyceil + sect->vertex[s].ceilz, tz1) * yscale1), ny1b = H/2 + (-Yaw(nyfloor + sect->vertex[s].floorz, tz1) * yscale1);
        int ny2a = H/2 + (-Yaw(nyceil + sect->vertex[s + 1].ceilz, tz2) * yscale2), ny2b = H/2 + (-Yaw(nyfloor+ sect->vertex[s + 1].floorz, tz2) * yscale2);
        /* Render the wall. */
        int beginx = max(x1, now.sx1), endx = min(x2, now.sx2);
        struct Scaler ya_int = Scaler_Init(x1, beginx, x2, y1a, y2a);
        struct Scaler yb_int = Scaler_Init(x1, beginx, x2, y1b, y2b);
        struct Scaler nya_int = Scaler_Init(x1, beginx, x2, ny1a, ny2a);
        struct Scaler nyb_int = Scaler_Init(x1, beginx, x2, ny1b, ny2b);
        for(int x = beginx; x <= endx; ++x)
        {
            float txtx = (u0*((x2-x)*tz2) + u1*((x-x1)*tz1)) / ((x2-x)*tz2 + (x-x1)*tz1);
            //if ((distance(sect->vertex[s].x, sect->vertex[s].y, sect->vertex[s + 1].x, sect->vertex[s + 1].y) * 75.0f > imageSrf->w * 2))
             //   txtx  *= (distance(sect->vertex[s].x, sect->vertex[s].y, sect->vertex[s + 1].x, sect->vertex[s + 1].y) * 75.0f / (float)imageSrf->w);
            /* Calculate the Z coordinate for this point. (Only used for lighting.) */
            int z = ((x - x1) * (tz2-tz1) / (x2-x1) + tz1) * 8;
            /* Acquire the Y coordinates for our ceiling & floor for this X coordinate. Clamp them. */
            int ya = Scaler_Next(&ya_int);
            int yb = Scaler_Next(&yb_int);
            /* Clamp the ya & yb */
            int cya = clamp(ya, ytop[x],ybottom[x]);
            int cyb = clamp(yb, ytop[x],ybottom[x]);
			int i =0;
			for(int y=ytop[x]; y<=ybottom[x]; ++y)
			{
               if(y >= cya && y <= cyb) { y = cyb; continue; }
               float hei = y < cya ? yceil : yfloor;
               float mapx, mapz;
               CeilingFloorScreenCoordinatesToMapCoordinates(hei, x, y,mapx, mapz);
               unsigned tx = (mapx * 50), txtz = (mapz * 50);
               //printf("tx:%f\ntxtz:%f\n", tx, txtz);
               int *surfacePix = (int*)surface->pixels;
               if (hei != yceil)
                   surfacePix[y * W + x] = getpixel(ceilTexture, tx % ceilTexture->w, txtz % ceilTexture->h);
               else
                   surfacePix[y * W + x] = getpixel(floorTexture, tx % floorTexture->w, txtz % floorTexture->h);
				i++;
			}
            /* Render ceiling: everything above this sector's ceiling height. */
            //vline(x, ytop[x], cya-1, 0xFF ,0x222222, 0xFF);
            /* Render floor: everything below this sector's floor height. */
            //vline(x, cyb+1, ybottom[x], 0xFF,0x222222, 0xFF);
            /* Is there another sector behind this edge? */
            if(neighbor >= 0)
            {
                /* Same for _their_ floor and ceiling */
                int nya = (x - x1) * (ny2a-ny1a) / (x2-x1) + ny1a, cnya = clamp(nya, ytop[x],ybottom[x]);
                int nyb = (x - x1) * (ny2b-ny1b) / (x2-x1) + ny1b, cnyb = clamp(nyb, ytop[x],ybottom[x]);
                /* If our ceiling is higher than their ceiling, render upper wall */
                unsigned r1 = 0x010101 * (255-z), r2 = 0x040007 * (31-z/8);
                //vline(x, cya, cnya-1, 0, x==x1||x==x2 ? 0 : r1, 0); // Between our and their ceiling
                if (nya - 1 != ya){
                    textLine(x, cya, cnya - 1, (struct Scaler)Scaler_Init(ya,cya,nya - 1, 0,imageSrf->h), txtx, sect, surface, imageSrf);
					//vline(x, cya, cnya-1, 0, x==x1||x==x2 ? 0 : r1, 0);
                }
                ytop[x] = clamp(max(cya, cnya), ytop[x], H-1);   // Shrink the remaining window below these ceilings
                /* If our floor is lower than their floor, render bottom wall */
                //vline(x, cnyb+1, cyb, 0, x==x1||x==x2 ? 0 : r2, 0); // Between their and our floor
                if (yb - 1 !=  nyb){
                    textLine(x, cnyb+1, cyb, (struct Scaler)Scaler_Init(nyb,cnyb,yb - 1, 0,imageSrf->h), txtx, sect, surface, imageSrf);
					//vline(x, cnyb+1, cyb, 0, x==x1||x==x2 ? 0 : r2, 0); // Between their and our floor
                }
                ybottom[x] = clamp(min(cyb, cnyb), 0, ybottom[x]); // Shrink the remaining window above these floors
				// t_point start = {x ,cnya};
				// t_point end = {x, cnyb};
				// if (sect->floor != 0)
				// 	line(surface, start, end, 0x00ff0000);
            }
            else
            {
                /* There's no neighbor. Render wall from top (cya = ceiling level) to bottom (cyb = floor level). */
                unsigned r = 0x010101 * (255-z);
                //vline(x, cya, cyb, 0, x==x1||x==x2 ? 0 : r, 0);
                //if (s != sect->npoints - 1)
                    //draw_texture_line(surface, imageSrf, x, 0, distance(sect->vertex[s].x, sect->vertex[s].y, sect->vertex[s + 1].x, sect->vertex[s + 1].y) , ya, yb);
                    textLine(x, cya, cyb, (struct Scaler)Scaler_Init(ya,cya,yb, 0, fabsf(sect->floor - sect->ceil) * scaleH), txtx, sect, surface, imageSrf);
                    // SDL_Texture *text = SDL_CreateTextureFromSurface(sdl->ren, surface);
                    // SDL_RenderClear(sdl->ren);
                    // SDL_RenderCopy(sdl->ren, text, NULL, NULL);
                    // SDL_RenderPresent(sdl->ren);
                    // SDL_DestroyTexture(text);
                    //filledLine(surface, imageSrf, x, wallx1, wallx2, ya, yb);
                    //if (s == 1)
                    //   printf("vx1:%f\nvx2:%f\nx1:%f\nx2:%f\n", vx1, vx2, wallx1, wallx2);
                //else
                //    draw_texture_line(surface, imageSrf, x, 0, distance(sect->vertex[s].x, sect->vertex[s].y, sect->vertex[0].x, sect->vertex[0].y), ya, yb);
            }
            //  else{
            //      printf("normal yscale1:%f\n yscale2:%f\nya:%d\nyb:%d\n", yscale1, yscale2, ya, yb);
            //  }
            //txtx  /= (distance(sect->vertex[s].x, sect->vertex[s].y, sect->vertex[s + 1].x, sect->vertex[s + 1].y) * 100 / imageSrf->w);
        }
        /* Schedule the neighboring sector for rendering within the window formed by this wall. */
        if(neighbor >= 0 && endx >= beginx && (head+MaxQueue+1-tail)%MaxQueue)
        {
            *head = (struct item) { neighbor, beginx, endx };
            if(++head == queue+MaxQueue) head = queue;
        }
//        	t_point p1 = projectPoint(sect->vertex[0].x, sect->vertex[0].y, sect->ceil);
//			t_point p2 = projectPoint(sect->vertex[1].x, sect->vertex[1].y, sect->ceil);
//			t_point p3 = projectPoint(sect->vertex[2].x, sect->vertex[2].y, sect->ceil);
//			t_point p4 = projectPoint(sect->vertex[3].x, sect->vertex[3].y, sect->ceil);
    } // for s in sector's edges
    ++renderedsectors[now.sectorno];
    for (int i = 0; i < 5; i++){
		if (demons[i].sector == now.sectorno && !demons[i].dead)
        	draw_enemy_frame(demons[i], sx1, sx2, ytop, ybottom);
	}
    //vline(sx1, 0, H - 1, 0xffffffff, 0xffffffff, 0xffffffff);
    //vline(sx2, 0, H - 1, 0xffffffff, 0xffffffff, 0xffffffff);
    } while(head != tail); // render any other queued sectors
}
int tmpp = 0;
int is_shooting = 0;
int enemy_damaged = 0;
int flying = 0;
int plasmaShot = 0;

void	play_animation();

int main(int ac, char **av)
{
    if (ac != 2){
        printf("Bruh, map\n");
        exit(1);
    }
    map = av[1];
    t_sdl *sdl;
    SDL_Texture *text;
    t_anim pistol;
	t_anim shotgun;
	t_projectile plasma;
	plasma.anim.frameCount = 0;
	plasma.anim.frames[0] = load_img("sprites/projectiles/plasma/plasma_1.png");
	plasma.anim.frames[1] = load_img("sprites/projectiles/plasma/plasma_2.png");
	plasma.anim.frames[2] = load_img("sprites/projectiles/plasma/plasma_3.png");
    plasma.speed = 1.0f;
	t_enemy demon;
	demon.pos = (point3D){10, 20};
    demon.sector = 0;
	demon.height = 10;
	demon.pos.z = 0;
	demon.speed = 0.5f;
    demon.hp = 100;
	demon.damaged = 0;
	demon.standing.frames[0] = load_img("sprites/demon/demon_standing_1.png");
	demon.walking.frames[0] = load_img("sprites/demon/demon_walking_1.png");
	demon.walking.frames[1] = load_img("sprites/demon/demon_walking_2.png");
	demon.walking.frames[2] = load_img("sprites/demon/demon_walking_3.png");
	demon.walking.frames[3] = load_img("sprites/demon/demon_walking_4.png");
	demon.walking.frames[2] = load_img("sprites/demon/demon_walking_3.png");
	demon.taking_damage.frames[0] = load_img("sprites/demon/demon_taking_damage.png");
	demon.taking_damage.frames[1] = load_img("sprites/demon/demon_taking_damage.png");
	demon.dying.frames[0] = load_img("sprites/demon/demon_dying_1.png");
	demon.dying.frames[1] = load_img("sprites/demon/demon_dying_2.png");
	demon.dying.frames[2] = load_img("sprites/demon/demon_dying_3.png");
	demon.dying.frames[3] = load_img("sprites/demon/demon_dying_3.png");
	demon.dying.frames[4] = load_img("sprites/demon/demon_dying_3.png");
	demon.dying.frames[5] = load_img("sprites/demon/demon_dying_3.png");
	demon.dying.frames[6] = load_img("sprites/demon/demon_dying_3.png");
	demon.dying.frames[7] = load_img("sprites/demon/demon_dying_3.png");
    demon.standing.frameCount = 0;
	demon.dying.frameCount = 0;
	demon.walking.frameCount = 0;
	demon.taking_damage.frameCount = 0;
    pistol.frameCount = 0;
	shotgun.frameCount = 0;
	shotgun.frames[0] = load_img("sprites/shotgun/shotgun_frame_1.png");
	shotgun.frames[1] = load_img("sprites/shotgun/shotgun_frame_2.png");
	shotgun.frames[2] = load_img("sprites/shotgun/shotgun_frame_3.png");
	shotgun.frames[3] = load_img("sprites/shotgun/shotgun_frame_4.png");
	shotgun.frames[4] = load_img("sprites/shotgun/shotgun_frame_3.png");
	shotgun.frames[5] = load_img("sprites/shotgun/shotgun_frame_2.png");
	demons[0] = demon;
	demons[1] = demon;
	demons[2] = demon;
	demons[3] = demon;
	demons[4] = demon;
	demons[0].pos = (point3D){20, 50};
	demons[1].pos = (point3D){30, 40};
	demons[2].pos = (point3D){10, 80};
	demons[3].pos = (point3D){30, 70};
	demons[4].pos = (point3D){20, 100};
    if (!ft_strcmp(av[1], "map-clear.txt") && !ft_strcmp(av[1], "lol"))
        LoadDataRaw();
    else
        LoadData();
    sdl = new_t_sdl(W, H, "test");
    if (init_sdl(sdl) == ERROR)
        exit(1);
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
	{
		fprintf(stderr, "Unable to initialize SDL: %s\n", SDL_GetError());
		return 1;
	}
	int audio_rate = 22050;
	Uint16 audio_format = AUDIO_S16SYS;
	int audio_channels = 2;
	int audio_buffers = 4096;
	if(Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers) != 0)
	{
		fprintf(stderr, "Unable to initialize audio: %s\n", Mix_GetError());
		exit(1);
	}
	Mix_Chunk* mus = Mix_LoadWAV("sounds/shot.wav");
	if (mus == NULL)
		printf("Error loading music: %s\n", Mix_GetError());
	surface = SDL_CreateRGBSurface(0, W, H, 32, 0, 0, 0, 0);
    imageSrf = load_img("textures/pent.jpg");
    floorTexture = load_img("textures/floor.jpeg");
    ceilTexture = load_img("textures/ceil.jpeg");
	skybox = load_img("textures/skybox.png");
    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    int wsad[4]={0,0,0,0}, ground=0, falling=1, moving=0, ducking=0;
    float yaw = 0;
    //SDL_Texture *imgtxt = SDL_CreateTextureFromSurface(sdl->ren, imageSrf);
    Uint32 startclock = 0;
    Uint32 deltaclock = 0;
    Uint32 currentFPS = 0;
	int damageCount = 0;
	int frame = 0;
    startclock = SDL_GetTicks();
    for(;;)
    {
		frame++;
		plasma.anim.frameCount = rand() % 3;
		deltaclock = SDL_GetTicks() - startclock;
        startclock = SDL_GetTicks();
        //if (deltaclock != 0)
           // printf("FPS: %d\n", 1000 / deltaclock);
		//SDL_FillRect(surface, NULL, 0xff000000);
        //printf("sx1:%f\nsx2:%f\n", sx1, sx2);
        //printf("sector:%d\nx:%f\ny:%f\n", player.sector, player.where.x, player.where.y);
       	//vector_tests();
		// struct Scaler scalH = Scaler_Init(-5, player.yaw, 5, 0, skybox->h);
		// int skyH = scalH.result / 2 + 100;
		//draw_custom_image(surface, skybox, 100, 100, 100, H, skyW + 100, skyH, skybox->w / 2, skybox->h / 2);
		// if (sdvig % 2 != 0)
		// 	draw_custom_image(surface, skybox, 0 + 100, 0 + 100, W, H, skyW , skyH, W, H);
		// else
		// {
		// 	draw_custom_image(surface, skybox, 0 , 0 , W, H,skyW ,skyH, W, H);
		// }
		sx1 = 0;
		sx2 = 0;
    	DrawScreen(sdl);
		for (int i = 0; i < 5; i++)
			demons[i].moving = 0;
		draw_crosshair();
		float degreeAngle = rtod(player.angle);
		float tmp = 360 + (degreeAngle - 360 * ((int)degreeAngle / 360));
		struct Scaler scalW = Scaler_Init(0, tmp - 360 * ((int)tmp / 360), 360, 0, skybox->w - 1);
		//skyW = scalW.result;
		demon.moving = enemyMove;
		int closest_shot = -1;
		float min_dist;
		for (int i = 0; i < 5; i++){
			if (demons[i].damaged)
			{
				damageCount++;
				if (damageCount > 4)
				{
					demons[i].damaged = 0;
					damageCount = 0;
				}
			}
			if (demons[i].hp <= 0)
			{
				demons[i].moving = 0;
				demons[i].dying.frameCount += 0.2;
				if (demons[i].dying.frameCount > 6){
					demons[i].dead = 1;
				}
			}
			if (distance(demons[i].pos.x, demons[i].pos.y, player.where.x, player.where.y) > 10 && !demons[i].damaged && demons[i].hp > 0)
			{
				demons[i].moving = 1;
				demons[i].walking.frameCount += 0.3;
				moveEnemyToPlayer(&demons[i]);
				if (demons[i].walking.frameCount >= 4)
					demons[i].walking.frameCount = 0;
			}
			// if (is_shooting && shotgun.frameCount == 0 && enemy_hit(demons[i]))
			// {
			// 	float curr_dist = distance(player.where.x ,player.where.y, demons[i].pos.x, demons[i].pos.y);
			// 	if (closest_shot == -1 || curr_dist < min_dist){
			// 		closest_shot = i;
			// 		min_dist = curr_dist;
			// 	}
			// }
			if (distance(plasma.pos.x, plasma.pos.y, demons[i].pos.x, demons[i].pos.y) < 2)
			{
				demons[i].damaged = 1;
				demons[i].hp -= 20;
			}
		}
		if (closest_shot > -1)
		{
			demons[closest_shot].damaged = 1;
			demons[closest_shot].hp -= 20;
		}
		if (is_shooting || shotgun.frameCount != 0){
			if (shotgun.frameCount == 0){
				plasma.pos.x = player.where.x + 1 * player.anglecos;
				plasma.pos.y = player.where.y + 1 * player.anglesin;
				plasma.anglecos = player.anglecos;
				plasma.anglesin = player.anglesin;
				plasma.yaw = player.yaw;
				plasma.z = player.where.z - plasma.yaw;
				Mix_PlayChannel(-1, mus, 0);
				plasmaShot = 1;
			}
			shotgun.frameCount += 0.4;
		}
		if (shotgun.frameCount >= 6.0){
			//is_shooting = 0;
			shotgun.frameCount = 0;
		}
		if (plasmaShot)
		{
			plasma.pos.x += plasma.speed * plasma.anglecos;
			plasma.pos.y += plasma.speed * plasma.anglesin;
			plasma.z -= plasma.yaw;
			draw_projecitle(plasma);
		}
		draw_weapon_frame(surface, shotgun);
		t_fpoint pos;
        text = SDL_CreateTextureFromSurface(sdl->ren, surface);
        SDL_RenderClear(sdl->ren);
        SDL_RenderCopy(sdl->ren, text, NULL, NULL);
        SDL_RenderPresent(sdl->ren);
        SDL_DestroyTexture(text);
        /* Vertical collision detection */
        float eyeheight = ducking ? DuckHeight : EyeHeight;
        ground = !falling;
        if(falling)
        {
            player.velocity.z -= 0.05f; /* Add gravity */
            float nextz = player.where.z + player.velocity.z;
            if(player.velocity.z < 0 && nextz  < sectors[player.sector].floor + eyeheight) // When going down
            {
                /* Fix to ground */
                player.where.z    = sectors[player.sector].floor + eyeheight;
                player.velocity.z = 0;
                falling = 0;
                ground  = 1;
            }
            else if(player.velocity.z > 0 && nextz > sectors[player.sector].ceil) // When going up
            {
                /* Prevent jumping above ceiling */
                player.velocity.z = 0;
                falling = 1;
            }
            if(falling)
            {
                player.where.z += player.velocity.z;
                moving = 1;
            }
        }
        /* Horizontal collision detection */
        if(moving)
        {
            float px = player.where.x,    py = player.where.y;
            float dx = player.velocity.x, dy = player.velocity.y;

            const struct sector* const sect = &sectors[player.sector];
            const struct vertex* const vert = sect->vertex;
            /* Check if the player is about to cross one of the sector's edges */
            for(unsigned s = 0; s < sect->npoints; ++s)
                if(IntersectBox(px,py, px+dx,py+dy, vert[s+0].x, vert[s+0].y, vert[s+1].x, vert[s+1].y)
                && PointSide(px+dx, py+dy, vert[s+0].x, vert[s+0].y, vert[s+1].x, vert[s+1].y) < 0)
                {
                    /* Check where the hole is. */
                    float hole_low  = sect->neighbors[s] < 0 ?  9e9 : max(sect->floor, sectors[sect->neighbors[s]].floor);
                    float hole_high = sect->neighbors[s] < 0 ? -9e9 : min(sect->ceil,  sectors[sect->neighbors[s]].ceil );
                    /* Check whether we're bumping into a wall. */
                    if(hole_high < player.where.z+HeadMargin
                    || hole_low  > player.where.z-eyeheight+KneeHeight)
                    {
                        /* Bumps into a wall! Slide along the wall. */
                        /* This formula is from Wikipedia article "vector projection". */
                        float xd = vert[s+1].x - vert[s+0].x, yd = vert[s+1].y - vert[s+0].y;
                        dx = xd * (dx*xd + yd*dy) / (xd*xd + yd*yd);
                        dy = yd * (dx*xd + yd*dy) / (xd*xd + yd*yd);
                        moving = 0;
                    }
                }
            MovePlayer(dx, dy);
            falling = 1;
        }

        SDL_Event ev;
        while(SDL_PollEvent(&ev))
            switch(ev.type)
            {
                case SDL_KEYDOWN:
				switch(ev.key.keysym.sym)
                    {
						case 'm': enemyMove = 1; break;
						case 'w': wsad[0] = 1; break;
                        case 's': wsad[1] = 1; break;
                        case 'a': wsad[2] = 1; break;
                        case 'd': wsad[3] = 1; break;
                        default: break;
                    }
				break;
                case SDL_KEYUP:
                    switch(ev.key.keysym.sym)
                    {
                        case 'w': wsad[0] = 0; break;
                        case 's': wsad[1] = 0; break;
                        case 'a': wsad[2] = 0; break;
                        case 'd': wsad[3] = 0; break;
                        case 'g': anglle += 0.01f; break;
                        case 'h': sectors[player.sector].ceil += 2; break;
                        case 'j': sectors[player.sector].ceil -= 2; break;
                        case 'c': scaleH++; break;
                        case 'v': scaleH--; break;
                        case 'z': del += 0.2; break;
                        case 'x': del /= 1.2; break;
                        case 'm': enemyMove = 0; break;
                        case SDLK_ESCAPE: goto done;
                        case 'q': flying = ev.type==SDL_KEYDOWN; break;
                        case 'r':     UnloadData(); ReloadData(); break;
                        case ' ': /* jump */
                            if(ground) { player.velocity.z += 0.5f; falling = 1; }
                            break;
                        case SDLK_RCTRL: ducking = ev.type==SDL_KEYDOWN; falling=1; break;
                        default: break;
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
					is_shooting = 1; break ;
				case SDL_MOUSEBUTTONUP:
					is_shooting = 0; break ;
                case SDL_QUIT: goto done;
            }

        /* mouse aiming */
        int x,y;
        SDL_GetRelativeMouseState(&x,&y);
        y = -y;
		skyW += x * (skybox->w / 360.0f);
        player.angle += x * 0.01;
        yaw          = clamp(yaw - y * 0.05f, -5, 5);
        player.yaw   = yaw /*- player.velocity.z*0.5f*/;
        MovePlayer(0,0);

        float move_vec[2] = {0.f, 0.f};
        if(wsad[0]) { move_vec[0] += player.anglecos*0.5f; move_vec[1] += player.anglesin*0.5f; }
        if(wsad[1]) { move_vec[0] -= player.anglecos*0.5f; move_vec[1] -= player.anglesin*0.5f; }
        if(wsad[2]) { move_vec[0] += player.anglesin*0.5f; move_vec[1] -= player.anglecos*0.5f; }
        if(wsad[3]) { move_vec[0] -= player.anglesin*0.5f; move_vec[1] += player.anglecos*0.5f; }
        if (flying)
            if (player.velocity.z < 3)
                player.velocity.z += 0.3;
        int pushing = wsad[0] || wsad[1] || wsad[2] || wsad[3];
        float acceleration = pushing ? 0.4 : 0.2;

        player.velocity.x = player.velocity.x * (1-acceleration) + move_vec[0] * acceleration;
        player.velocity.y = player.velocity.y * (1-acceleration) + move_vec[1] * acceleration;

        if(pushing) moving = 1;
    }
done:
    UnloadData();
    SDL_DestroyTexture(text);
    free_t_sdl(sdl);
	system("leaks -q doom-nukem");
    return 0;
}