#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
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
enum InamicPoz {
    S,
    D
};

struct Inamic {
    InamicPoz startPoz;
};

enum GameState {
    LEVEL_SELECT,
    PLAYING,
    WON
};

int totalCoins = 0;
int collectedCoins = 0;

TTF_Font* font = nullptr;
SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;

bool hasIntersection(const SDL_Rect* A, const SDL_Rect* B) {
    if (A->x + A->w <= B->x || B->x + B->w <= A->x || A->y + A->h <= B->y || B->y + B->h <= A->y) {
        return false;
    }

    return true;
}

void loadLevel(const string& filePath, vector<GameObject>& gameObjects, SDL_Renderer* renderer, SDL_Texture* brickTexture, SDL_Texture* vineTexture, SDL_Texture* marioTexture, SDL_Texture* starCoinTexture, GameObject& player) {
    ifstream levelFile(filePath);
    string line;
    int y = 0;
    int playerInit = 0;

    while (getline(levelFile, line)) { // pune inamici
        for (int x = 0; x < line.length(); ++x) {
            char tile = line[x];
            SDL_Rect rect = { x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE };
            if (tile == '1') {
                gameObjects.push_back({ brickTexture, rect });
            } else if (tile == '/') {
                gameObjects.push_back({ vineTexture, rect });
            } else if (tile == '+') {
                gameObjects.push_back({ starCoinTexture, rect });
                totalCoins++;
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

bool hasIntersection(const SDL_Rect& a, const SDL_Rect& b) {
    return hasIntersection(&a, &b);
}

bool isOnVine(const GameObject& player, const vector<GameObject>& gameObjects, SDL_Texture* vineTexture) {
    for (const auto& obj : gameObjects) {
        if (obj.texture == vineTexture) {
            return false;
        }
        if (obj.texture == vineTexture && hasIntersection(player.rect, obj.rect)) {
            return true;
        }
    }
    return false;
}

bool isOnPlatform(const GameObject& player, const vector<GameObject>& gameObjects, SDL_Texture* brickTexture) {
    SDL_Rect belowPlayer = player.rect;
    belowPlayer.y += 1; // Check just below the player
    for (const auto& obj : gameObjects) {
        if (obj.texture == brickTexture && hasIntersection(belowPlayer, obj.rect)) {
            return true;
        }
    }
    return false;
}

void renderText(SDL_Renderer* renderer, const string& text, int x, int y) {
    SDL_Color textColor = { 255, 255, 255, 255 };
    SDL_Surface* textSurface = TTF_RenderText_Solid(font, text.c_str(), textColor);
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_Rect textRect = { x, y, textSurface->w, textSurface->h };
    SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);
}

void renderWinningScreen(SDL_Renderer* renderer, bool isLastLevel) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    renderText(renderer, isLastLevel ? "You have completed the game!" : "You Won!", SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 - 50);
    if (!isLastLevel) {
        renderText(renderer, "Next Level", SCREEN_WIDTH / 2 - 50, SCREEN_HEIGHT / 2 + 50);
    } else {
        renderText(renderer, "Press Space to end the game", SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2 + 50);
    }

    SDL_RenderPresent(renderer);
}

vector<string> getLevelFiles(const string& folderPath) {
    vector<string> levelFiles;
    for (const auto& entry : directory_iterator(folderPath)) {
        if (entry.path().extension() == ".lvl") {
            levelFiles.push_back(entry.path().string());
        }
    }
    return levelFiles;
}

bool isPointInRect(int x, int y, const SDL_Rect& rect) {
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

void drawRectOutline(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawRect(renderer, &rect);
}

void renderLevelSelectScreen(SDL_Renderer* renderer, const vector<string>& levelFiles, int selectedIndex, vector<SDL_Rect>& levelRects, SDL_Texture* backgroundTexture, TTF_Font* levelFont) {
    // Render the background
    SDL_RenderCopy(renderer, backgroundTexture, nullptr, nullptr);

    // Render the title
    renderText(renderer, "Select a Level", SCREEN_WIDTH / 2 - 100, 50);

    levelRects.clear();
    SDL_Color outlineColor = { 255, 255, 255, 255 }; // White color for the outline
    SDL_Color hoverColor = { 255, 0, 0, 255 }; // Red color for hover effect

    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    for (int i = 0; i < levelFiles.size(); ++i) {
        int reverseIndex = levelFiles.size() - 1 - i;
        string levelName = levelFiles[reverseIndex].substr(levelFiles[reverseIndex].find_last_of("/\\") + 1);
        if ((levelFiles.size() - 1) == selectedIndex) {
            levelName = "> " + levelName;
        }
        int x = SCREEN_WIDTH / 2 - 100;
        int y = 100 + i * 30;
        renderText(renderer, levelName, x, y/*, levelFont*/);

        SDL_Rect rect = { x, y, static_cast<int>(levelName.length() * 10), 30 }; // Approximate width
        levelRects.push_back(rect);

        // Draw hover effect
        if (isPointInRect(mouseX, mouseY, rect)) {
            drawRectOutline(renderer, rect, hoverColor);
        } else {
            drawRectOutline(renderer, rect, outlineColor);
        }
    }

    SDL_RenderPresent(renderer);
}

int main() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    window = SDL_CreateWindow("Mario", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    IMG_Init(IMG_INIT_PNG);
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    TTF_Init();
    font = TTF_OpenFont("../resources/opensans.ttf", 24);

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
    Mix_PlayMusic(soundtrack, -1);
    bool musicPlaying = true;

    vector<GameObject> gameObjects;
    GameObject player{};

    vector<string> levelFiles = getLevelFiles("../levels");
    int selectedIndex = 0;
    GameState gameState = LEVEL_SELECT;
    vector<SDL_Rect> levelRects;
    int currentLevelIndex = 0;
    bool isLastLevel = false;

    bool quit = false;
    SDL_Event e;

    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            if (e.type == SDL_KEYDOWN) {
                if (gameState == LEVEL_SELECT) {
                    switch (e.key.keysym.sym) {
                        case SDLK_RETURN:
                            loadLevel(levelFiles[selectedIndex], gameObjects, renderer, brickTexture, vineTexture, marioTextureLeft, starCoinTexture, player);
                            gameState = PLAYING;
                            break;
                        case SDLK_ESCAPE:
                            quit = true;
                            break;
                        case SDLK_m:
                            if (musicPlaying) {
                                Mix_PauseMusic();
                            } else {
                                Mix_ResumeMusic();
                            }
                            musicPlaying = !musicPlaying;
                            break;
                        default: break;
                    }
                } else if (gameState == WON && e.key.keysym.sym == SDLK_SPACE) {
                    quit = true;
                } else {
                    SDL_Rect newRect = player.rect;
                    int moveSpeed = TILE_SIZE;

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
                    case SDLK_p:
                        gameState = WON;
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

                    // Check for collisions with other game objects
                    for (const auto& obj : gameObjects) {
                        if (obj.texture != vineTexture && obj.texture != starCoinTexture && hasIntersection(newRect, obj.rect)) {
                            collision = true;
                            break;
                        }
                        if (obj.texture == starCoinTexture && hasIntersection(newRect, obj.rect)) {
                            collectedCoins++;
                            gameObjects.erase(remove_if(gameObjects.begin(), gameObjects.end(), [&obj](const GameObject& o) {
                                return &o == &obj;
                                }), gameObjects.end());
                            break;
                        }
                    }

                    if (!collision) {
                        player.rect = newRect;
                    }
                }
            } else if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (gameState == LEVEL_SELECT) {
                    int mouseX, mouseY;
                    SDL_GetMouseState(&mouseX, &mouseY);
                    for (int i = 0; i < levelRects.size(); ++i) {
                        if (isPointInRect(mouseX, mouseY, levelRects[i])) {
                            selectedIndex = levelFiles.size() - 1 - i;
                            loadLevel(levelFiles[selectedIndex], gameObjects, renderer, brickTexture, vineTexture, marioTextureLeft, starCoinTexture, player);
                            gameState = PLAYING;
                            break;
                        }
                    }
                } else if (gameState == WON) {
                    if (isLastLevel) {
                        quit = true;
                    } else {
                        currentLevelIndex++;
                        if (currentLevelIndex >= levelFiles.size()) {
                            isLastLevel = true;
                        } else {
                            selectedIndex = currentLevelIndex;
                            loadLevel(levelFiles[selectedIndex], gameObjects, renderer, brickTexture, vineTexture, marioTextureLeft, starCoinTexture, player);
                            gameState = PLAYING;
                        }
                    }
                }
            }
        }
        if (gameState == LEVEL_SELECT) {
            renderLevelSelectScreen(renderer, levelFiles, selectedIndex, levelRects, backgroundTexture, font);
        } else if (gameState == PLAYING) {
            if (!isOnPlatform(player, gameObjects, brickTexture) && !isOnVine(player, gameObjects, vineTexture)) {
                bool atTopOfVine = false;
                SDL_Rect belowPlayer = player.rect;
                belowPlayer.y += 1;

                for (const auto& obj : gameObjects) {
                    if (obj.texture == vineTexture && hasIntersection(belowPlayer, obj.rect)) {
                        atTopOfVine = true;
                        break;
                    }
                }

                if (!atTopOfVine) {
                    int gravity = 1;
                    player.rect.y += gravity;
                    if (player.rect.y + player.rect.h > SCREEN_HEIGHT) { // imagine falling off the screen :')
                        player.rect.y = SCREEN_HEIGHT - player.rect.h;
                    }
                }
            }

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
            SDL_RenderClear(renderer);

            SDL_RenderCopy(renderer, backgroundTexture, nullptr, nullptr);
            for (const auto& obj : gameObjects) {
                SDL_RenderCopy(renderer, obj.texture, nullptr, &obj.rect);
            }
            SDL_RenderCopy(renderer, player.texture, nullptr, &player.rect);

            // Render the coin counter
            string coinText = "Coins: " + to_string(collectedCoins) + "/" + to_string(totalCoins);
            renderText(renderer, coinText, 10, 10);

            SDL_RenderPresent(renderer);

            if (collectedCoins == totalCoins) {
                gameState = WON;
            }
        } else {
            renderWinningScreen(renderer, isLastLevel);
        }
    }

    Mix_FreeMusic(soundtrack);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    Mix_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}