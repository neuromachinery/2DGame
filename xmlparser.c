#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "xmlparser.h"

static int parse_int_tag(const char *line, const char *tag) {
    char open_tag[32], close_tag[32];
    snprintf(open_tag, sizeof(open_tag), "<%s>", tag);
    snprintf(close_tag, sizeof(close_tag), "</%s>", tag);

    const char *start = strstr(line, open_tag);
    if (!start) return 0;
    start += strlen(open_tag);
    const char *end = strstr(start, close_tag);
    if (!end) return 0;

    char buf[32];
    size_t len = end - start;
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    strncpy(buf, start, len);
    buf[len] = '\0';

    return atoi(buf);
}

static int parse_attr(const char *line, const char *attr, char *out, size_t out_size) {
    const char *pos = strstr(line, attr);
    if (!pos) return 0;
    pos = strchr(pos, '=');
    if (!pos) return 0;
    pos++; // skip '='
    while (*pos == ' ' || *pos == '\"') pos++; // skip spaces and quotes

    const char *end = pos;
    while (*end && *end != '\"' && *end != ' ' && *end != '>') end++;

    size_t len = end - pos;
    if (len >= out_size) len = out_size - 1;

    strncpy(out, pos, len);
    out[len] = '\0';

    return 1;
}

int parse_spritesheet(FILE *f, SpriteSheet *sheet) {
    char line[256];
    int in_animation = 0;
    int in_sprite = 0;
    int animation_count = 0;
    int sprite_count = 0;

    Animation *current_animation = NULL;
    Sprite current_sprite = {0};

    memset(sheet, 0, sizeof(SpriteSheet));

    // Parse image_name, width, height from <spritesheet ...>
    rewind(f);
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "<spritesheet")) {
            parse_attr(line, "image_name", sheet->image_name, XML_MAX_NAME_LEN);

            // Parse width and height attributes
            char width_buf[16], height_buf[16];
            if (parse_attr(line, "width", width_buf, sizeof(width_buf)))
                sheet->width = atoi(width_buf);
            if (parse_attr(line, "height", height_buf, sizeof(height_buf)))
                sheet->height = atoi(height_buf);
            break;
        }
    }

    rewind(f);
    while (fgets(line, sizeof(line), f)) {
        // Detect <animation name="...">
        if (!in_animation && strstr(line, "<animation")) {
            if (animation_count >= MAX_ANIMATIONS) break;
            current_animation = &sheet->animations[animation_count];
            if (!parse_attr(line, "name", current_animation->name, XML_MAX_NAME_LEN)) {
                strcpy(current_animation->name, "unknown");
            }
            current_animation->sprite_count = 0;
            sprite_count = 0;
            in_animation = 1;
            continue;
        }
        if (in_animation && strstr(line, "</animation>")) {
            current_animation->sprite_count = sprite_count;
            animation_count++;
            in_animation = 0;
            continue;
        }
        if (!in_animation) continue;

        // Detect <sprite name="...">
        if (!in_sprite && strstr(line, "<sprite")) {
            if (sprite_count >= MAX_SPRITES_PER_ANIMATION) continue;
            memset(&current_sprite, 0, sizeof(Sprite));
            if (!parse_attr(line, "name", current_sprite.name, XML_MAX_NAME_LEN)) {
                strcpy(current_sprite.name, "unknown");
            }
            in_sprite = 1;
            continue;
        }

        if (in_sprite) {
            if (strstr(line, "</sprite>")) {
                current_animation->sprites[sprite_count++] = current_sprite;
                in_sprite = 0;
                continue;
            }
            if (strstr(line, "<offset_x>")) {
                current_sprite.FRect.x = parse_int_tag(line, "offset_x");
            } else if (strstr(line, "<offset_y>")) {
                current_sprite.FRect.y = parse_int_tag(line, "offset_y");
            } else if (strstr(line, "<width>")) {
                current_sprite.FRect.w = parse_int_tag(line, "width");
            } else if (strstr(line, "<height>")) {
                current_sprite.FRect.h = parse_int_tag(line, "height");
            }
        }
    }
    sheet->animation_count = animation_count;
    return 1;
}
