#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <iostream>
#include <vector>
#include <string>
#include "functions.h"

using namespace std;

int main(int argc, char* args[]) {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;

    if (!init(window, renderer)) {
        cerr << "Failed to initialize!" << endl;
        return -1;
    }

    SDL_Texture* brickTexture = loadTexture("../resources/brick.png", renderer);
    SDL_Texture* vineTexture = loadTexture("../resources/vine.png", renderer);
    SDL_Texture* starCoinTexture = loadTexture("../resources/star-coin.png", renderer);
    SDL_Texture* marioTextureLeft = loadTexture("../resources/mario-l.png", renderer);
    SDL_Texture* marioTextureRight = loadTexture("../resources/mario-r.png", renderer);
    SDL_Texture* backgroundTexture = loadTexture("../resources/background.png", renderer);

    if (!brickTexture || !vineTexture || !marioTextureLeft || !marioTextureRight || !backgroundTexture) {
        cerr << "Failed to load textures!" << endl;
        close(window, renderer);
        return -1;
    }

    Mix_Music* soundtrack = Mix_LoadMUS("../resources/soundtrack.mp3");
    if (!soundtrack) {
        cerr << "Failed to load soundtrack! SDL_mixer Error: " << Mix_GetError() << endl;
        close(window, renderer);
        return -1;
    }

    Mix_PlayMusic(soundtrack, -1);
    bool musicPlaying = true;

    vector<GameObject> gameObjects;
    GameObject player{};
    string levelPath = "../level.txt";
    loadLevel(levelPath, gameObjects, renderer, brickTexture, vineTexture, marioTextureLeft, starCoinTexture, player);

    auto lastWriteTime = last_write_time(levelPath);

    bool quit = false;
    SDL_Event e;

    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            handleInput(e, player, gameObjects, vineTexture, marioTextureLeft, marioTextureRight, quit, musicPlaying, soundtrack);
        }

        // Check if the level file has been modified
        auto currentWriteTime = getLastWriteTime(levelPath);
        if (currentWriteTime != lastWriteTime) {
            // Reset the game state
            gameObjects.clear();
            loadLevel(levelPath, gameObjects, renderer, brickTexture, vineTexture, marioTextureLeft, starCoinTexture, player);
            lastWriteTime = currentWriteTime;
        }

        if (!isOnPlatform(player, gameObjects, brickTexture) && !isOnVine(player, gameObjects, vineTexture)) {
            bool atTopOfVine = false;
            SDL_Rect belowPlayer = player.rect;
            belowPlayer.y += 1; // Check just below the player

            for (const auto& obj : gameObjects) {
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

        // Render the background
        SDL_RenderCopy(renderer, backgroundTexture, nullptr, nullptr);

        for (const auto& obj : gameObjects) {
            SDL_RenderCopy(renderer, obj.texture, nullptr, &obj.rect);
        }
        SDL_RenderCopy(renderer, player.texture, nullptr, &player.rect);

        SDL_RenderPresent(renderer);
    }

    Mix_FreeMusic(soundtrack);
    close(window, renderer);
    return 0;
}