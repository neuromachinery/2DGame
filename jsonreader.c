#include <stdio.h>
#include <cJSON.h>
#include "jsonreader.h"
struct Actor Actors[MAX_ACTORS];
int ReadJSON(char *path,cJSON **json){
    // open the file
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        printf("Error: Unable to open the file.\n");
        return 1;
    }

    // read the file contents into a string
    char buffer[2048];
    int len = fread(buffer, 1, sizeof(buffer), fp);
    fclose(fp);

    // parse the JSON data
    *json = cJSON_Parse(buffer);
    if (*json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            printf("Error: %s\n", error_ptr);
        }
        return 1;
    }
    return 0;
}
void parse_inventory_array(cJSON *array, char dest[MAX_ITEMS][JSON_MAX_NAME_LEN], int *count) {
    int size = cJSON_GetArraySize(array);
    *count = size > MAX_ITEMS ? MAX_ITEMS : size;
    for (int i = 0; i < *count; i++) {
        cJSON *item = cJSON_GetArrayItem(array, i);
        if (item->valuestring != NULL) {
            strncpy(dest[i], item->valuestring, JSON_MAX_NAME_LEN);
            dest[i][JSON_MAX_NAME_LEN-1] = '\0';
        } else {
            dest[i][0] = '\0';
        }
    }
}
void parse_actor_json(cJSON *actor_json, struct Actor *actor, int i, int window_x, int window_y, int gridScale) {
    // Parse name (use "name" field if exists, else fallback to JSON key)
    cJSON *name = cJSON_GetObjectItem(actor_json, "name");
    if (name->valuestring) {
        strncpy(actor->name, name->valuestring, JSON_MAX_NAME_LEN);
    } else if (actor_json->string) {
        strncpy(actor->name, actor_json->string, JSON_MAX_NAME_LEN);
    } else {
        strcpy(actor->name, "unknown");
    }
    actor->name[JSON_MAX_NAME_LEN-1] = '\0';

    // Parse inventory
    cJSON *inventory = cJSON_GetObjectItem(actor_json, "inventory");
    if (inventory) {
        cJSON *equipped = cJSON_GetObjectItem(inventory, "equipped");
        parse_inventory_array(equipped, actor->inventory.equipped, &actor->inventory.equipped_count);

        cJSON *stored = cJSON_GetObjectItem(inventory, "stored");
        parse_inventory_array(stored, actor->inventory.stored, &actor->inventory.stored_count);
    } else {
        actor->inventory.equipped_count = 0;
        actor->inventory.stored_count = 0;
    }

    // Parse speed
    cJSON *speed = cJSON_GetObjectItem(actor_json, "speed");
    actor->speed = (float)speed->valueint;


    // Parse health array
    cJSON *health = cJSON_GetObjectItem(actor_json, "health");
        int size = cJSON_GetArraySize(health);
        for (int j = 0; j < HEALTH_VALUES; j++) {
        cJSON *h = cJSON_GetArrayItem(health, j);
        actor->health[j] = h->valueint;
        }
    cJSON *money = cJSON_GetObjectItem(actor_json, "money");
    actor->money = money->valueint;
    // Set SDL-related fields
    int i_high = i & 1;
    int i_low = (i & 2) >> 1;
    actor->ActorRect.x = (float)window_x * (float)i_high;
    actor->ActorRect.y = (float)window_y * (float)i_low;
    actor->ActorRect.h = gridScale;
    actor->ActorRect.w = gridScale;
    actor->halfHeight = gridScale * 0.5f;
    actor->halfWidth = gridScale * 0.5f;

    SDL_Color color = {255, 85 * i, 255 - 85 * i, 255};
    actor->ActorColor = color;
}
void parse_characters(cJSON *characters_json, int window_x, int window_y, int gridScale) {
    cJSON *temp = characters_json->child;  // first actor (assuming root is an object)
    int i = 0;

    while (temp != NULL) {
        // temp->string is the actor key (e.g., "john")
        // temp is the actor JSON object

        if (temp!=NULL) {
            parse_actor_json(temp, &Actors[i], i, window_x, window_y, gridScale);
            i++;
        }
        temp = temp->next;
    }
}