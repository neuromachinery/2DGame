#define HEALTH_VALUES 4
#define MAX_ITEMS 256
#define JSON_MAX_NAME_LEN 50
#define MAX_ACTORS 4
#include <SDL3/SDL.h>
typedef struct{
    char equipped[MAX_ITEMS][JSON_MAX_NAME_LEN];
    int equipped_count;

    char stored[MAX_ITEMS][JSON_MAX_NAME_LEN];
    int stored_count;
} Inventory;
struct Actor{
        SDL_FRect ActorRect;
        int positionHeight; //0=prone,1=sitting,2=on one knee,3=standing
        float speed;
        SDL_Color ActorColor;
        float halfWidth;
        float halfHeight;
        char name[JSON_MAX_NAME_LEN];
        Inventory inventory;
        int health[HEALTH_VALUES];
        int money;
        
};
extern struct Actor Actors[MAX_ACTORS];
int ReadJSON(char *path,cJSON **json);
void parse_characters(cJSON *characters_json, int window_x, int window_y, int gridScale);