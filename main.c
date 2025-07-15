
#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#define MAX_ITEMS 256
#define MAX_NAME_LEN 50
#define HEALTH_VALUES 4
#define MAX_ACTORS 4
#define MAP_SIZE 512
#define DESCRIPTION_LENGTH 20
#define SCREEN_AMOUNT 3
#define ERROR 1
#define NORMAL 0
#define DEFAULT_BG 50,50,50,255
#define DEFAULT_FG 255,255,255,255
#define DEFAULT_NOTICE 255,10,10,255
#define DEFAULT_SCALE 16.0f
#define nsPerFrame 16666666
#define TEMP_MAP_NAME "themap.map"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

cJSON *characters_json;
cJSON *items_json;

typedef struct{
    char equipped[MAX_ITEMS][MAX_NAME_LEN];
    int equipped_count;

    char stored[MAX_ITEMS][MAX_NAME_LEN];
    int stored_count;
} Inventory;
struct Actor{
        SDL_FRect ActorRect;
        int positionHeight; //0=prone,1=sitting,2=on one knee,3=standing
        float speed;
        SDL_Color ActorColor;
        float halfWidth;
        float halfHeight;
        char name[MAX_NAME_LEN];
        Inventory inventory;
        int health[HEALTH_VALUES];
        int money;
        
};
typedef struct {
    uint16_t tiles[MAP_SIZE*MAP_SIZE];
    uint16_t object_tiles[MAP_SIZE*MAP_SIZE];
}Map;
Map *LoadedMaps;
int LoadedMaps_size=0;
struct Actor Actors[MAX_ACTORS];
typedef struct{
    SDL_FRect FRect;
    void (*buttonFunc)(void *arg);
    void *buttonArg;
    char buttonName[MAX_NAME_LEN];
} Button;
typedef struct{
    int (*screenFunc) (void);
    int (*eventFunc) (SDL_Event *);
    Button *Buttons;
    int Buttons_size;
    char screenName[MAX_NAME_LEN];
} screen;


int w = 0, h = 0;
int ActorTurn = 0;
SDL_FRect VisibleFRect;
float gridScale = DEFAULT_SCALE;
int CurrentScreen = 0;
int CurrentLoadedMap = 0;
int MaterialSpriteCount;
float EditorDisplacementX=0;
float EditorDisplacementY=0;
SDL_FPoint EditorMousePosition;
int EditorCurrentCellType=0;
SDL_FRect *EditorFRects;
int EditorFRectsSize = 0;
SDL_Texture *spriteMaterialSheet;
SDL_Texture *spriteCharacterSheet;

SDL_FRect *MaterialSpriteFRects;
SDL_FRect *CharacterSpriteFRects;

TTF_Font *font;
screen screens[SCREEN_AMOUNT];

int INT_CONSTANTS[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

void loadMap(void *arg) {
    int*mapid=(int*)arg;
    // Open the binary file for reading
    FILE *file = fopen(TEMP_MAP_NAME, "rb");
    if (!file) {
        perror("Failed to open file");
        return;
    }
    Map *map;
    if(!mapid || *mapid>=LoadedMaps_size){
        LoadedMaps_size++;
        LoadedMaps = realloc(LoadedMaps,sizeof(Map)*LoadedMaps_size);
        map = &LoadedMaps[LoadedMaps_size-1];
        *mapid = LoadedMaps_size-1;
    }
    else{map = &LoadedMaps[*mapid];}
    int map_linear_size = MAP_SIZE*MAP_SIZE;
    fread(&map->tiles,sizeof(uint16_t),map_linear_size,file);
    fread(&map->object_tiles,sizeof(uint16_t),map_linear_size,file);
    fclose(file);
}
void saveMap(void *arg){
    int*mapid=(int*)arg;
    FILE *file = fopen(TEMP_MAP_NAME,"wb");
    if (!file) {
        perror("Failed to open file");
        return;
    }
    if(!mapid||*mapid>=LoadedMaps_size){return;}
    fwrite(&LoadedMaps[*mapid],sizeof(Map),1,file);
    fclose(file);
}
float SnapToGrid(float x, float y, float w, float h, float *return_x, float *return_y) {
    float snap_x = x/w;
    float snap_y = y/h;
    int temp = (int)snap_x;
    temp = temp * (int)w;
    snap_x = (float)temp;
    temp = (int)snap_y;
    temp = temp * (int)h;
    snap_y = (float)temp;
    *return_x = snap_x;
    *return_y = snap_y;
}
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
void parse_inventory_array(cJSON *array, char dest[MAX_ITEMS][MAX_NAME_LEN], int *count) {
    int size = cJSON_GetArraySize(array);
    *count = size > MAX_ITEMS ? MAX_ITEMS : size;
    for (int i = 0; i < *count; i++) {
        cJSON *item = cJSON_GetArrayItem(array, i);
        if (item->valuestring != NULL) {
            strncpy(dest[i], item->valuestring, MAX_NAME_LEN);
            dest[i][MAX_NAME_LEN-1] = '\0';
        } else {
            dest[i][0] = '\0';
        }
    }
}
void parse_actor_json(cJSON *actor_json, struct Actor *actor, int i, int window_x, int window_y, int gridScale) {
    // Parse name (use "name" field if exists, else fallback to JSON key)
    cJSON *name = cJSON_GetObjectItem(actor_json, "name");
    if (name->valuestring) {
        strncpy(actor->name, name->valuestring, MAX_NAME_LEN);
    } else if (actor_json->string) {
        strncpy(actor->name, actor_json->string, MAX_NAME_LEN);
    } else {
        strcpy(actor->name, "unknown");
    }
    actor->name[MAX_NAME_LEN-1] = '\0';

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

void ChangeEditorCellType(void *arg){
    EditorCurrentCellType = *(int*)arg;
}
int LoadAllTextures(){
    SDL_Surface *spriteMaterialSheetSurface = IMG_Load("resources/Textures-small.png");
    //SDL_Surface *spriteCharacterSheetSurface = IMG_Load("resources/Character.png");
    //if (!spriteCharacterSheetSurface || !spriteCharacterSheetSurface) {
    if (!spriteMaterialSheetSurface){
        printf("Failed to load sprite sheet: %s",SDL_GetError());
        return ERROR;
    }
    spriteMaterialSheet = SDL_CreateTextureFromSurface(renderer, spriteMaterialSheetSurface);
    SDL_DestroySurface(spriteMaterialSheetSurface);
    SDL_SetTextureScaleMode(spriteMaterialSheet,SDL_SCALEMODE_PIXELART);
    
    //spriteCharacterSheet = SDL_CreateTextureFromSurface(renderer, spriteCharacterSheetSurface);
    //SDL_DestroySurface(spriteCharacterSheetSurface);
    //if(!spriteMaterialSheet || !spriteCharacterSheet){
    if(!spriteMaterialSheet){
        printf("Sprite error: %s\n",SDL_GetError());
    }
    float w; float h;
    SDL_GetTextureSize(spriteMaterialSheet,&w,&h);
    MaterialSpriteCount = (int)w/h;
    MaterialSpriteFRects = malloc(MaterialSpriteCount*sizeof(SDL_FRect));
    for(int i=0;i<MaterialSpriteCount;i++){
        SDL_FRect textureFRect = {i*h,0,h,h};
        MaterialSpriteFRects[i] = textureFRect;
    }
    //SDL_GetTextureSize(spriteCharacterSheet,&w,&h);
    //CharacterSpriteFRects = malloc(spriteCount*sizeof(SDL_FRect));
    //float halfh = h/2;
    //for(int i=0;i<spriteCount;i++){
    //    SDL_FRect textureFRect = {i*halfh,0,halfh,h};
    //    CharacterSpriteFRects[i] = textureFRect;
    //}
    return NORMAL;
}
void DrawCircle(SDL_Renderer * renderer, int32_t centreX, int32_t centreY, int32_t radius)
{
   const int32_t diameter = (radius * 2);

   int32_t x = (radius - 1);
   int32_t y = 0;
   int32_t tx = 1;
   int32_t ty = 1;
   int32_t error = (tx - diameter);

   while (x >= y)
   {
      //  Each of the following renders an octant of the circle
      SDL_RenderPoint(renderer, centreX + x, centreY - y);
      SDL_RenderPoint(renderer, centreX + x, centreY + y);
      SDL_RenderPoint(renderer, centreX - x, centreY - y);
      SDL_RenderPoint(renderer, centreX - x, centreY + y);
      SDL_RenderPoint(renderer, centreX + y, centreY - x);
      SDL_RenderPoint(renderer, centreX + y, centreY + x);
      SDL_RenderPoint(renderer, centreX - y, centreY - x);
      SDL_RenderPoint(renderer, centreX - y, centreY + x);

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
int checkButtons(SDL_FPoint *point){
    for (int i=0;i<screens[CurrentScreen].Buttons_size;i++){
        if(SDL_PointInRectFloat(point,&screens[CurrentScreen].Buttons[i].FRect)){
            screens[CurrentScreen].Buttons[i].buttonFunc(screens[CurrentScreen].Buttons[i].buttonArg);
            return true;
        }
    }
    return false;
}
int balloc(screen *Screen,int size){
    Screen->Buttons_size = size;
    Screen->Buttons = (Button*)calloc(Screen->Buttons_size,sizeof(Button));
    if (!Screen->Buttons) {
        printf("Memory allocation failed.\n");
    }
}
void ChangeScreen(void *screenID){
    if(!screenID){CurrentScreen=0;return;}
    CurrentScreen=*(int*)screenID; 
}
int ScreenMap(){
    SDL_SetRenderDrawColor(renderer,DEFAULT_BG);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    const float gridScale = 16.0f;
    float grid_w = w/gridScale*2.0f;
    float grid_h = h/gridScale*2.0f;
    for (float i=0;i<grid_w;i++){
        float x_line = i*gridScale;
        SDL_RenderLine(renderer,x_line,0.0f,x_line,h);
    }
    for (float i=0;i<grid_h;i++){
        float y_line = i*gridScale;
        SDL_RenderLine(renderer,0.0f,y_line,w,y_line);
    }
    for (int i=0;i<4;i++){
        struct Actor actor = Actors[i];
        SDL_SetRenderDrawColor(renderer, actor.ActorColor.r, actor.ActorColor.g, actor.ActorColor.b, actor.ActorColor.a);
        SDL_RenderFillRect(renderer,&actor.ActorRect);
    }
    struct Actor actor = Actors[ActorTurn];
    SDL_SetRenderDrawColor(renderer, actor.ActorColor.r, actor.ActorColor.g, actor.ActorColor.b, actor.ActorColor.a);
    DrawCircle(renderer,actor.ActorRect.x+actor.halfWidth,actor.ActorRect.y+actor.halfHeight,actor.speed);
    SDL_RenderPresent(renderer);
    return NORMAL;
}
int ScreenMenu(){
    SDL_SetRenderDrawColor(renderer,DEFAULT_BG);
    SDL_RenderClear(renderer);
    SDL_Color bg = {100,100,100,255};
    SDL_Color fg = {255,255,255};
    
    for (int i=1;i<SCREEN_AMOUNT;i++){
        SDL_SetRenderDrawColor(renderer,bg.r,bg.g,bg.b,bg.a);
        
        
        SDL_RenderFillRect(renderer,&screens[0].Buttons[i-1].FRect);
        
        SDL_Surface* textSurface = TTF_RenderText_Solid(font, screens[i].screenName,strlen(screens[i].screenName), fg);
        if (!textSurface){
            printf("%s\n",SDL_GetError());
            return ERROR;
        }
        
        SDL_Texture* buttonText = SDL_CreateTextureFromSurface(renderer, textSurface);

        SDL_RenderTexture(renderer, buttonText, NULL, &screens[0].Buttons[i-1].FRect);

        SDL_DestroySurface(textSurface);
        SDL_DestroyTexture(buttonText);
    }
    SDL_RenderPresent(renderer);
    return NORMAL;
}
int ScreenEditor(){
    SDL_SetRenderDrawColor(renderer,DEFAULT_BG);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer,DEFAULT_FG);
    for(int i=0;i<MAP_SIZE*MAP_SIZE;i++){
        SDL_FPoint corner = {(SDL_fmodf(i,MAP_SIZE)*gridScale)+EditorDisplacementX,((i/MAP_SIZE)*gridScale)+EditorDisplacementY};
        if (!SDL_PointInRectFloat(&corner,&VisibleFRect)){continue;}
        SDL_FRect DestFRect = {corner.x,corner.y,gridScale,gridScale};
        SDL_FRect textureFRect = MaterialSpriteFRects[LoadedMaps[CurrentLoadedMap].tiles[i]];
        SDL_RenderTexture(renderer,spriteMaterialSheet,&textureFRect,&DestFRect);
    }
    SDL_FRect DestFRect = {EditorMousePosition.x,EditorMousePosition.y,gridScale,gridScale};
    SDL_FRect textureFRect = MaterialSpriteFRects[EditorCurrentCellType];
    SDL_RenderTexture(renderer,spriteMaterialSheet,&textureFRect,&DestFRect);
    SDL_FRect originPointFRect = {0.0f+EditorDisplacementX,0.0f+EditorDisplacementY,10,10};
    SDL_SetRenderDrawColor(renderer,DEFAULT_NOTICE);
    SDL_RenderRect(renderer,&originPointFRect);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    
    float grid_w = w/gridScale;
    float grid_h = h/gridScale;
    for (float i=0;i<grid_w;i++){
        float x_line = i*gridScale+SDL_fmodf(EditorDisplacementX,gridScale);
        SDL_RenderLine(renderer,x_line,0.0f,x_line,h);
    }
    for (float i=0;i<grid_h;i++){
        float y_line = i*gridScale+SDL_fmodf(EditorDisplacementY,gridScale);
        SDL_RenderLine(renderer,0.0f,y_line,w,y_line);
    }
    SDL_SetRenderDrawColor(renderer,50,50,50,128);
    SDL_FRect buttonBg = {0,h-40,w,40};
    SDL_RenderFillRect(renderer,&buttonBg);
    
    SDL_SetRenderDrawColor(renderer,0,0,0,128);
    printf("%d\r",EditorCurrentCellType);
    //General purpose buttons (0 to 2)
    for (int i=0;i<screens[CurrentScreen].Buttons_size-MaterialSpriteCount;i++){
        Button button = screens[CurrentScreen].Buttons[i];
        SDL_RenderFillRect(renderer,&button.FRect);
        SDL_Color fg = {DEFAULT_FG};
        SDL_Surface* textSurface = TTF_RenderText_Solid(font,button.buttonName,strlen(button.buttonName),fg);
        SDL_Texture* buttonText = SDL_CreateTextureFromSurface(renderer, textSurface);
        SDL_RenderTexture(renderer, buttonText, NULL, &button.FRect);
        //SDL_DestroySurface(textSurface);
        //SDL_DestroyTexture(buttonText);
    }
    //Material Pallette (3 to spritecount)
    for (int i=0;i<MaterialSpriteCount;i++){
        SDL_RenderTexture(renderer,spriteMaterialSheet,&MaterialSpriteFRects[i],&screens[CurrentScreen].Buttons[i+3].FRect);
    }

    SDL_RenderPresent(renderer);
    return NORMAL;
}


int EventMap(SDL_Event *event){
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN){
        SDL_FPoint point;
        SDL_GetMouseState(&point.x,&point.y);
        struct Actor actor = Actors[ActorTurn];
        float dx = actor.ActorRect.x+actor.halfHeight - point.x;
        float dy = actor.ActorRect.y+actor.halfWidth - point.y;
        float dist = dx*dx+dy*dy;
        if (dist < actor.speed*actor.speed){
            SnapToGrid(point.x,point.y,actor.ActorRect.w,actor.ActorRect.h,&Actors[ActorTurn].ActorRect.x,&Actors[ActorTurn].ActorRect.y);
        }
        ActorTurn = (ActorTurn+1)%4;
        checkButtons(&point);
    }
    return NORMAL;
}
int EventMenu(SDL_Event *event){
    if(event->type == SDL_EVENT_MOUSE_BUTTON_DOWN){
        SDL_FPoint point;
        SDL_GetMouseState(&point.x,&point.y);
        checkButtons(&point);
    }
    return NORMAL;
}
int EventEditor(SDL_Event *event){
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN){
        if(event->button.button == SDL_BUTTON_LEFT){
            SDL_FPoint point;
            SDL_GetMouseState(&point.x,&point.y);
            if(checkButtons(&point)){return NORMAL;}
            SnapToGrid(point.x-EditorDisplacementX,point.y-EditorDisplacementY,gridScale,gridScale,&point.x,&point.y);
            EditorFRectsSize++;
            EditorFRects = realloc(EditorFRects,sizeof(SDL_FRect)*EditorFRectsSize);
            SDL_FRect FRect = {point.x,point.y,gridScale,gridScale};
            EditorFRects[EditorFRectsSize-1]=FRect;
            LoadedMaps[CurrentLoadedMap].tiles[(int)(point.x/gridScale)+((int)(point.y/gridScale)*MAP_SIZE)]=EditorCurrentCellType;
        }    
    }
    if (event->type == SDL_EVENT_MOUSE_WHEEL){
            gridScale += event->wheel.y;
            if (gridScale<=0.1f){gridScale=DEFAULT_SCALE;}
    }
    if (event->type == SDL_EVENT_MOUSE_MOTION){
        if (event->motion.state & SDL_BUTTON_RMASK) {
                EditorDisplacementX+=event->motion.xrel*(gridScale*0.1f);
                EditorDisplacementY+=event->motion.yrel*(gridScale*0.1f);
        }
        SDL_GetMouseState(&EditorMousePosition.x,&EditorMousePosition.y);
    }
    return NORMAL;
}


int InitMenu(){
    #define MenuButtonW 270.0f
    #define MenuButtonH 40.0f
    #define MenuButtonPad 5.0f
    SDL_GetRenderOutputSize(renderer, &w, &h);
    screen *InitScreen = &screens[0];
    strcpy(InitScreen->screenName,"Menu");
    InitScreen->screenFunc = &ScreenMenu;
    InitScreen->eventFunc = &EventMenu;
    if (balloc(InitScreen,SCREEN_AMOUNT-1)==ERROR){printf("balloc fault\n");return ERROR;}
    float buttonStep = (h+(MenuButtonH+MenuButtonPad*SCREEN_AMOUNT))/SCREEN_AMOUNT;
    for (int i=1;i<SCREEN_AMOUNT;i++){
        SDL_FRect buttonRect = {w*0.30f,buttonStep*i,MenuButtonW,MenuButtonH};
        InitScreen->Buttons[i-1].FRect = buttonRect;
        InitScreen->Buttons[i-1].buttonFunc = &ChangeScreen;
        InitScreen->Buttons[i-1].buttonArg = &INT_CONSTANTS[i];
        strcpy(InitScreen->Buttons[i-1].buttonName,screens[i].screenName);
    }
    return NORMAL;
}
int InitMap(){
    screen *InitScreen = &screens[1];
    strcpy(InitScreen->screenName,"Map");
    InitScreen->screenFunc = &ScreenMap;
    InitScreen->eventFunc = &EventMap;
    if (balloc(InitScreen,1)==ERROR){printf("balloc fault\n");return ERROR;}
    InitScreen->Buttons[0].buttonFunc = &ChangeScreen;
    strcpy(InitScreen->Buttons[0].buttonName,"To Menu");
    return NORMAL;
}
int InitEditor(){
    #define MenuButtonW 270.0f
    #define MenuButtonH 40.0f
    #define MenuButtonPad 5.0f
    screen *InitScreen = &screens[2];
    strcpy(InitScreen->screenName,"Editor");
    InitScreen->screenFunc = &ScreenEditor;
    InitScreen->eventFunc = &EventEditor;
    if (balloc(InitScreen,3+MaterialSpriteCount)==ERROR){printf("balloc fault\n");return ERROR;}
    InitScreen->Buttons[0].buttonFunc = &ChangeScreen;
    strcpy(InitScreen->Buttons[0].buttonName,"To Menu");
    InitScreen->Buttons[1].buttonFunc = &saveMap;
    InitScreen->Buttons[1].buttonArg = &CurrentLoadedMap;
    strcpy(InitScreen->Buttons[1].buttonName,"Save");
    InitScreen->Buttons[2].buttonFunc = &loadMap;
    InitScreen->Buttons[2].buttonArg = &CurrentLoadedMap;
    strcpy(InitScreen->Buttons[2].buttonName,"Load");
    float usedButtonSpace = 0.0;
    for (int i=0;i<MaterialSpriteCount;i++){
        float bx = i*MaterialSpriteFRects[i].w;
        float by = h-32;
        float bh = MaterialSpriteFRects[i].h;
        float bw = MaterialSpriteFRects[i].w;
        usedButtonSpace+=bw;
        SDL_FRect buttonFRect = {bx,by,bh,bw};
        InitScreen->Buttons[i+3].FRect = buttonFRect;
        InitScreen->Buttons[i+3].buttonFunc = &ChangeEditorCellType;
        InitScreen->Buttons[i+3].buttonArg = &INT_CONSTANTS[i];
    }
    for (int i=0;i<3;i++){
        float buttonSizeX = (float)strlen(InitScreen->Buttons[i].buttonName)*16.0f;
        SDL_FRect buttonFRect = {usedButtonSpace+10,h-32,buttonSizeX,32};
        usedButtonSpace+=buttonSizeX+10;
        InitScreen->Buttons[i].FRect = buttonFRect;
    }
    LoadedMaps_size++;
    LoadedMaps = realloc(LoadedMaps,sizeof(Map)*LoadedMaps_size);
    Map *map = &LoadedMaps[LoadedMaps_size-1];
    for (int i=0;i<MAP_SIZE*MAP_SIZE;i++){
        map->tiles[i] = 0;
    }
    for (int i=0;i<MAP_SIZE*MAP_SIZE;i++){
        map->object_tiles[i] = 0;
    }
    return NORMAL;
}
int (*InitFuncs[SCREEN_AMOUNT])(void) = {&InitMap,*InitEditor,&InitMenu};

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_SetRenderVSync(renderer, 1);
    int ret = TTF_Init();
    if (ret < 0){
        printf("%d",ret);
    }
    /* Create the window */
    const int window_x = 800;
    const int window_y = 600;
    SDL_FRect FRect = {0.0f,0.0f,800.0f,600.0f};
    VisibleFRect = FRect;
    if (!SDL_CreateWindowAndRenderer("Hello World", window_x, window_y, SDL_WINDOW_MAXIMIZED, &window, &renderer)) {
        SDL_Log("Couldn't create window and renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    const float gridScale = 16.0f;
    ReadJSON("data/items.json",&items_json);
    ReadJSON("data/characters.json",&characters_json);
    if (characters_json == NULL) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
        fprintf(stderr, "JSON parse error before: %s\n", error_ptr);
    } else {
        fprintf(stderr, "Unknown JSON parse error\n");
    }
    }
    parse_characters(characters_json, window_x, window_y, gridScale);

    font = TTF_OpenFont("F77.ttf",72.0f);
    if (font == NULL){
        printf("%s\n",SDL_GetError());
    }
    if(LoadAllTextures()==ERROR){return SDL_APP_FAILURE;}
    SDL_GetRenderOutputSize(renderer, &w, &h);
    for (int i=0;i<SCREEN_AMOUNT;i++){
        InitFuncs[i]();
    }
    return SDL_APP_CONTINUE;
}
/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{   
    //if (event->type==SDL_EVENT_KEY_DOWN){printf("%d",event->key);}
    if (SDL_SCANCODE_Q == event->key.scancode || event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    if (screens[CurrentScreen].eventFunc(event)==ERROR){
        return SDL_APP_FAILURE;
    };
    return SDL_APP_CONTINUE;
}
/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    Uint64 frameStart = SDL_GetPerformanceCounter();
    float x, y;
    const float scale = 1.0f;
    
    SDL_GetRenderOutputSize(renderer, &w, &h);
    SDL_SetRenderScale(renderer, scale, scale);
    //screens[CurrentScreen].screenFunc();
    if(screens[CurrentScreen].screenFunc()==ERROR){return SDL_APP_FAILURE;}
    

    Uint64 frameEnd = SDL_GetPerformanceCounter();
    Uint64 renderingNS = (frameEnd - frameStart) * 1000000000 / SDL_GetPerformanceFrequency();
    if (renderingNS < nsPerFrame) {
        SDL_DelayNS(nsPerFrame - renderingNS);
    }
    return SDL_APP_CONTINUE;
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    // delete the JSON object
    cJSON_Delete(characters_json);
    
}