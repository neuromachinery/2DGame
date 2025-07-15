#ifndef SPRITESHEET_H
#define SPRITESHEET_H

#include <stdio.h>
#include <SDL3/SDL.h>
#define XML_MAX_NAME_LEN 64
#define MAX_SPRITES_PER_ANIMATION 32
#define MAX_ANIMATIONS 32

typedef struct {
    SDL_FRect FRect;
    char name[XML_MAX_NAME_LEN];
} Sprite;

typedef struct {
    char name[XML_MAX_NAME_LEN];
    Sprite sprites[MAX_SPRITES_PER_ANIMATION];
    int sprite_count;
} Animation;

typedef struct {
    char image_name[XML_MAX_NAME_LEN];
    int width;
    int height;
    Animation animations[MAX_ANIMATIONS];
    int animation_count;
} SpriteSheet;

// Parses the XML file pointer, fills the SpriteSheet struct, returns 1 on success, 0 on failure
int parse_spritesheet(FILE *f, SpriteSheet *sheet);

#endif // SPRITESHEET_H
