#include "functions.h"
#include <iostream>
#include <fstream>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <filesystem>
#include <chrono>

using namespace std;

file_time_type getLastWriteTime(const string& filePath) {
    return last_write_time(filePath);
}

bool init(SDL_Window*& window, SDL_Renderer*& renderer) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << endl;
        return false;
    }

    window = SDL_CreateWindow("Mario", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << endl;
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << endl;
        return false;
    }

    if (constexpr int imgFlags = IMG_INIT_PNG; !(IMG_Init(imgFlags) & imgFlags)) {
        cerr << "SDL_image could not initialize! SDL_image Error: " << IMG_GetError() << endl;
        return false;
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        cerr << "SDL_mixer could not initialize! SDL_mixer Error: " << Mix_GetError() << endl;
        return false;
    }

    return true;
}

SDL_Texture* loadTexture(const string& path, SDL_Renderer* renderer) {
    SDL_Texture* newTexture = IMG_LoadTexture(renderer, path.c_str());
    if (!newTexture) {
        char* basePath = SDL_GetBasePath();
        string altPath = string(basePath) + "resources/" + filesystem::path(path).filename().string();
        SDL_free(basePath);
        newTexture = IMG_LoadTexture(renderer, altPath.c_str());
        if (!newTexture) {
            cerr << "Unable to load texture " << path << " or " << altPath << endl;
        }
    }
    return newTexture;
}

void close(SDL_Window* window, SDL_Renderer* renderer) {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    Mix_Quit();
    IMG_Quit();
    SDL_Quit();
}

void loadLevel(const string& filePath, vector<GameObject>& gameObjects, SDL_Renderer* renderer, SDL_Texture* brickTexture, SDL_Texture* vineTexture, SDL_Texture* marioTexture, SDL_Texture* starCoinTexture ,GameObject& player) {
    ifstream levelFile(filePath);
    string line;
    int y = 0;
    int playerInit = 0;

    while (getline(levelFile, line)) {
        size_t commentPos = line.find('#');
        if (commentPos != string::npos) {
            line = line.substr(0, commentPos);
        }

        for (int x = 0; x < line.length(); ++x) {
            char tile = line[x];
            SDL_Rect rect = { x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE };
            if (tile == '1') {
                gameObjects.push_back({ brickTexture, rect });
            } else if (tile == '/') {
                gameObjects.push_back({ vineTexture, rect });
            } else if (tile == '+') {
                gameObjects.push_back({ starCoinTexture, rect });
            } else if (tile == '@') {
                if (++playerInit > 1) {
                    throw runtime_error("Error: Player character initialized more than once!");
                }
                player = { marioTexture, rect };
            }
        }
        ++y;
    }
}

bool checkCollision(const SDL_Rect& a, const SDL_Rect& b) {
    return SDL_HasIntersection(&a, &b);
}

bool isOnVine(const GameObject& player, const vector<GameObject>& gameObjects, SDL_Texture* vineTexture) {
    for (const auto& obj : gameObjects) {
        if (obj.texture == vineTexture && checkCollision(player.rect, obj.rect)) {
            return true;
        }
    }
    return false;
}

bool isOnPlatform(const GameObject& player, const vector<GameObject>& gameObjects, SDL_Texture* brickTexture) {
    SDL_Rect belowPlayer = player.rect;
    belowPlayer.y += 1; // Check just below the player
    for (const auto& obj : gameObjects) {
        if (obj.texture == brickTexture && checkCollision(belowPlayer, obj.rect)) {
            return true;
        }
    }
    return false;
}

void handleInput(const SDL_Event& e, GameObject& player, const vector<GameObject>& gameObjects, SDL_Texture* vineTexture, SDL_Texture* marioTextureLeft, SDL_Texture* marioTextureRight, bool& quit, bool& musicPlaying, Mix_Music* soundtrack) {
    if (e.type == SDL_KEYDOWN) {
        SDL_Rect newRect = player.rect;
        constexpr int moveSpeed = TILE_SIZE;

        switch (e.key.keysym.sym) {
        case SDLK_w:
            newRect.y -= moveSpeed;
            break;
        case SDLK_s:
            newRect.y += moveSpeed;
            break;
        case SDLK_a:
            newRect.x -= moveSpeed;
            player.texture = marioTextureLeft;
            break;
        case SDLK_d:
            newRect.x += moveSpeed;
            player.texture = marioTextureRight;
            break;
        case SDLK_m:
            if (musicPlaying) {
                Mix_PauseMusic();
            } else {
                Mix_ResumeMusic();
            }
            musicPlaying = !musicPlaying;
            break;
        case SDLK_ESCAPE:
            quit = true;
            break;
        default: break;
        }

        // Ensure the player does not go out of the screen's bounds
        if (newRect.x < 0) newRect.x = 0;
        if (newRect.x + newRect.w > SCREEN_WIDTH) newRect.x = SCREEN_WIDTH - newRect.w;
        if (newRect.y < 0) newRect.y = 0;
        if (newRect.y + newRect.h > SCREEN_HEIGHT) newRect.y = SCREEN_HEIGHT - newRect.h;

        bool collision = false;
        for (const auto& obj : gameObjects) {
            if (obj.texture == player.texture) continue; // Skip player texture
            if (obj.texture != vineTexture && checkCollision(newRect, obj.rect)) {
                collision = true;
                break;
            }
        }

        if (!collision) {
            player.rect = newRect;
        }
    }
}