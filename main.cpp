#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define TILE_SIZE 40

using namespace std;
using namespace std::filesystem;

struct GameObject {
    SDL_Texture* texture;
    SDL_Rect rect;
};

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
        if (obj.texture == vineTexture) {
            return false;
        }
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

int main(int argc, char* args[]) {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    if (!init(window, renderer)) {
        cerr << "Failed to initialize!" << endl;
        return -1;
    }

    SDL_Texture* brickTexture = IMG_LoadTexture(renderer, "../resources/brick.png");
    SDL_Texture* vineTexture = IMG_LoadTexture(renderer, "../resources/vine.png");
    SDL_Texture* starCoinTexture = IMG_LoadTexture(renderer, "../resources/star-coin.png");
    SDL_Texture* marioTextureLeft = IMG_LoadTexture(renderer, "../resources/mario-l.png");
    SDL_Texture* marioTextureRight = IMG_LoadTexture(renderer, "../resources/mario-r.png");
    SDL_Texture* backgroundTexture = IMG_LoadTexture(renderer, "../resources/background.png");

    if (!brickTexture || !vineTexture || !marioTextureLeft || !marioTextureRight || !backgroundTexture) {
        cerr << "Failed to load textures!" << endl;
        SDL_Quit();
        return -1;
    }

    Mix_Music* soundtrack = Mix_LoadMUS("../resources/soundtrack.mp3");
    if (!soundtrack) {
        cerr << "Failed to load soundtrack! SDL_mixer Error: " << Mix_GetError() << endl;
        SDL_Quit();
        return -1;
    }

    Mix_PlayMusic(soundtrack, -1);
    bool musicPlaying = true;

    vector<GameObject> gameObjects;
    GameObject player{};
    string levelPath = "../level.txt";
    loadLevel(levelPath, gameObjects, renderer, brickTexture, vineTexture, marioTextureLeft, starCoinTexture, player);

    bool quit = false;
    SDL_Event e;

    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            handleInput(e, player, gameObjects, vineTexture, marioTextureLeft, marioTextureRight, quit, musicPlaying, soundtrack);
        }

        if (!isOnPlatform(player, gameObjects, brickTexture) && !isOnVine(player, gameObjects, vineTexture)) {
            bool atTopOfVine = false;
            SDL_Rect belowPlayer = player.rect;
            belowPlayer.y += 1;

            for (const GameObject& obj : gameObjects) {
                if (obj.texture == vineTexture && checkCollision(belowPlayer, obj.rect)) {
                    atTopOfVine = true;
                    break;
                }
            }

            if (!atTopOfVine) {
                constexpr int gravity = 1;
                player.rect.y += gravity;
                if (player.rect.y + player.rect.h > SCREEN_HEIGHT) { // imagine falling off the screen :')
                    player.rect.y = SCREEN_HEIGHT - player.rect.h;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
        SDL_RenderClear(renderer);

        SDL_RenderCopy(renderer, backgroundTexture, nullptr, nullptr);
        for (const GameObject& obj : gameObjects) {
            SDL_RenderCopy(renderer, obj.texture, nullptr, &obj.rect);
        }
        SDL_RenderCopy(renderer, player.texture, nullptr, &player.rect);

        SDL_RenderPresent(renderer);
    }

    Mix_FreeMusic(soundtrack);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    Mix_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}