#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <thread>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define TILE_SIZE 40

using namespace std;
using namespace std::filesystem;

struct GameObject {
    SDL_Texture* texture;
    SDL_FRect rect;
};

struct Enemy {
    GameObject gameObject;
    SDL_FRect path;
    float speed;
    bool movingRight;
    SDL_Texture* textureLeft;
    SDL_Texture* textureRight;
    bool useLeftTexture;
};

enum GameState {
    LEVEL_SELECT,
    PLAYING,
    WON,
    LOST
};

int totalCoins = 0;
int collectedCoins = 0;

TTF_Font* font = nullptr;
SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;

//NOLINTBEGIN(cppcoreguidelines-narrowing-conversions)
void loadLevel(const string& filePath, vector<GameObject>& gameObjects, const vector<SDL_Texture*>& textures, vector<Enemy>& enemies, GameObject& player) {
    ifstream levelFile(filePath);
    string line;
    float y = 0;
    int playerInit = 0;

    while (getline(levelFile, line)) {
        vector<int> enemyPositions;
        for (int x = 0; x < line.length(); ++x) {
            char tile = line[x];
            SDL_FRect rect = { static_cast<float>(x * TILE_SIZE), y * TILE_SIZE, TILE_SIZE, TILE_SIZE };
            if (tile == '1') {
                gameObjects.push_back({ textures[0], rect });
            } else if (tile == '/') {
                gameObjects.push_back({ textures[1], rect });
            } else if (tile == '+') {
                gameObjects.push_back({ textures[2], rect });
                ++totalCoins;
            } else if (tile == '@') {
                if (++playerInit > 1) {
                    throw runtime_error("Error: Player character initialized more than once!");
                }
                player = { textures[3], rect };
            } else if (tile == '$') {
                enemyPositions.push_back(x);
            }
        }

        // Create enemies and their movement paths
        for (size_t i = 0; i < enemyPositions.size(); i += 2) {
            if (i + 1 < enemyPositions.size()) {
                float startX = enemyPositions[i] * TILE_SIZE;
                float endX = enemyPositions[i + 1] * TILE_SIZE;
                float enemySize = TILE_SIZE * 0.75; // 25% smaller than TILE_SIZE
                float yOffset = TILE_SIZE - enemySize; // Calculate the offset to align to the bottom
                SDL_FRect enemyRect = { startX, y * TILE_SIZE + yOffset, enemySize, enemySize };
                GameObject enemy = { textures[6], enemyRect };
                SDL_FRect path = { startX, y * TILE_SIZE, endX - startX, TILE_SIZE };
                enemies.push_back({ enemy, path, 0.06f, true, textures[6], textures[7], true });
            }
        }
        ++y;
    }
}

bool hasIntersection(const SDL_FRect& A, const SDL_FRect& B) {
    if (A.x + A.w <= B.x || B.x + B.w <= A.x || A.y + A.h <= B.y || B.y + B.h <= A.y) {
        return false;
    }

    return true;
}

bool isOnVine(const GameObject& player, const vector<GameObject>& gameObjects, const SDL_Texture* vineTexture) {
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

bool isOnPlatform(const GameObject& player, const vector<GameObject>& gameObjects, const SDL_Texture* brickTexture) {
    SDL_FRect belowPlayer = player.rect;
    belowPlayer.y += 1; // Check just below the player
    for (const auto& obj : gameObjects) { //NOLINT(readability-use-anyofallof)
        if (obj.texture == brickTexture && hasIntersection(belowPlayer, obj.rect)) {
            return true;
        }
    }
    return false;
}

bool isPointInRect(int x, int y, const SDL_Rect& rect) {
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

void drawRectOutline(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawRect(renderer, &rect);
}

void renderText(SDL_Renderer* renderer, const string& text, float x, float y) {
    SDL_Color textColor = { 255, 255, 255, 255 };
    SDL_Surface* textSurface = TTF_RenderText_Solid(font, text.c_str(), textColor);
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    float tsw = textSurface -> w;
    float tsh = textSurface -> h;
    SDL_FRect textRect = { x, y, tsw, tsh };
    SDL_RenderCopyF(renderer, textTexture, nullptr, &textRect);
    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);
}

//NOLINTBEGIN(bugprone-integer-division)
vector<string> getLevelFiles(const string& folderPath) {
    vector<string> levelFiles;

    for (const auto& entry : directory_iterator(folderPath)) {
        if (entry.path().extension() == ".lvl") {
            levelFiles.push_back(entry.path().string());
        }
    }
    return levelFiles;
}

void renderLevelSelectScreen(SDL_Renderer* renderer, const vector<string>& levelFiles, int selectedIndex, vector<SDL_Rect>& levelRects, SDL_Texture* backgroundTexture, TTF_Font* levelFont) {
    SDL_RenderCopyF(renderer, backgroundTexture, nullptr, nullptr);
    renderText(renderer, "Select a Level", SCREEN_WIDTH / 2 - 100, 150);

    levelRects.clear();
    SDL_Color outlineColor = { 255, 255, 255, 255 }; // White color for the outline
    SDL_Color hoverColor = { 255, 0, 0, 255 }; // Red color for hover effect

    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    for (int i = 0; i < levelFiles.size(); ++i) {
        int reverseIndex = levelFiles.size() - 1 - i;
        string levelName = levelFiles[reverseIndex].substr(levelFiles[reverseIndex].find_last_of("/\\") + 1);

        int x = SCREEN_WIDTH / 2 - (levelName.length() * 30 / 4);
        int y = 200 + i * 30;
        renderText(renderer, levelName, x, y + ( i * 5));

        SDL_Rect rect = { x - 5 , y + ( i * 5), static_cast<int>(levelName.length() * 15), 30 };
        levelRects.push_back(rect);

        if (isPointInRect(mouseX, mouseY, rect)) {
            drawRectOutline(renderer, rect, hoverColor);
        } else {
            drawRectOutline(renderer, rect, outlineColor);
        }
    }

    SDL_RenderPresent(renderer);
}

void updateEnemies(vector<Enemy>& enemies, const GameObject& player, Mix_Chunk* killSound) {
    for (auto& enemy : enemies) {
        float previousX = enemy.gameObject.rect.x;

        if (enemy.movingRight) {
            enemy.gameObject.rect.x += enemy.speed;
            if (enemy.gameObject.rect.x >= enemy.path.x + enemy.path.w) {
                enemy.movingRight = false;
            }
        } else {
            enemy.gameObject.rect.x -= enemy.speed;
            if (enemy.gameObject.rect.x <= enemy.path.x) {
                enemy.movingRight = true;
            }
        }

        if (static_cast<int>(previousX / TILE_SIZE) != static_cast<int>(enemy.gameObject.rect.x / TILE_SIZE)) {
            enemy.useLeftTexture = !enemy.useLeftTexture;
            enemy.gameObject.texture = enemy.useLeftTexture ? enemy.textureLeft : enemy.textureRight;
        }

        // Check if the player intersects with the top of the enemy
        SDL_FRect playerBottom = player.rect;
        playerBottom.y += player.rect.h;

        if (hasIntersection(playerBottom, enemy.gameObject.rect)) {
            Mix_PlayChannel(-1, killSound, 0); // Play the kill sound
            erase_if(enemies, [&enemy](const Enemy& o) {
                return &o == &enemy;
            });
            break;
        }
    }
}

void renderWinningScreen(SDL_Renderer* renderer, bool isLastLevel) {
    SDL_Color outlineColor = { 255, 255, 255, 255 }; // White color for the outline
    SDL_Color hoverColor = { 0, 255, 0, 255 }; // Green color for hover effect

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    if (isLastLevel) {
        renderText(renderer, "You have completed the game!", SCREEN_WIDTH / 2 - 210, SCREEN_HEIGHT / 2 - 50);
        renderText(renderer, "Press Space to exit", SCREEN_WIDTH / 2 - 142.5, SCREEN_HEIGHT / 2 + 20);
    } else {
        renderText(renderer, "You won!", SCREEN_WIDTH / 2 - 55, SCREEN_HEIGHT / 2 - 100);
        renderText(renderer, "Next Level", SCREEN_WIDTH / 2 - 70, SCREEN_HEIGHT / 2 + 40 - 100);
        SDL_Rect nextLevelButton = { SCREEN_WIDTH / 2 - 75, SCREEN_HEIGHT / 2 + 40 - 100, 150, 32 };
        if (isPointInRect(mouseX, mouseY, nextLevelButton)) {
            drawRectOutline(renderer, nextLevelButton, hoverColor);
        } else {
            drawRectOutline(renderer, nextLevelButton, outlineColor);
        }
    }

    SDL_RenderPresent(renderer);
}

void renderLostScreen(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    renderText(renderer, "You Lost!", SCREEN_WIDTH / 2 - 67.5, SCREEN_HEIGHT / 2 - 50);
    renderText(renderer, "Press Space to exit", SCREEN_WIDTH / 2 - 142.5, SCREEN_HEIGHT / 2);
    SDL_RenderPresent(renderer);
}

//NOLINTEND(bugprone-integer-division)

int main() {
    // init stuff
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    window = SDL_CreateWindow("Mario", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    IMG_Init(IMG_INIT_PNG);
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    TTF_Init();
    font = TTF_OpenFont("../resources/font/firacode.ttf", 24);

    // load me textures
    SDL_Texture* brickTexture = IMG_LoadTexture(renderer, "../resources/brick.png");
    SDL_Texture* vineTexture = IMG_LoadTexture(renderer, "../resources/vine.png");
    SDL_Texture* starCoinTexture = IMG_LoadTexture(renderer, "../resources/star-coin.png");
    SDL_Texture* marioTextureLeft = IMG_LoadTexture(renderer, "../resources/mario-l.png");
    SDL_Texture* marioTextureRight = IMG_LoadTexture(renderer, "../resources/mario-r.png");
    SDL_Texture* backgroundTexture = IMG_LoadTexture(renderer, "../resources/background.png");
    SDL_Texture* enemyTextureLeft = IMG_LoadTexture(renderer, "../resources/enemy-l.png");
    SDL_Texture* enemyTextureRight = IMG_LoadTexture(renderer, "../resources/enemy-r.png");
    vector textures = { brickTexture, vineTexture, starCoinTexture, marioTextureLeft, marioTextureRight, backgroundTexture, enemyTextureLeft, enemyTextureRight };

    if (!brickTexture || !vineTexture || !marioTextureLeft || !marioTextureRight || !backgroundTexture) {
        cerr << "Failed to load textures!" << endl;
        SDL_Quit();
        return -1;
    }

    // load me music
    Mix_Music* soundtrack = Mix_LoadMUS("../resources/sounds/soundtrack.mp3");
    Mix_Music* lostSound = Mix_LoadMUS("../resources/sounds/lost.mp3");
    Mix_Chunk* coinSound = Mix_LoadWAV("../resources/sounds/coin.mp3");
    Mix_Chunk* clearSound = Mix_LoadWAV("../resources/sounds/clear.mp3");
    Mix_Chunk* wonSound = Mix_LoadWAV("../resources/sounds/won.mp3");
    Mix_Chunk* jumpSound = Mix_LoadWAV("../resources/sounds/jump.mp3");
    Mix_Chunk* killSound = Mix_LoadWAV("../resources/sounds/kill.mp3");
    Mix_Chunk* stepSound = Mix_LoadWAV("../resources/sounds/steps.mp3");

    Mix_VolumeMusic(64);
    Mix_VolumeChunk(coinSound, 64);
    Mix_VolumeChunk(clearSound, 64);
    Mix_VolumeChunk(wonSound, 64);
    Mix_PlayMusic(soundtrack, -1);
    bool musicPlaying = true;
    bool soundPlayed = false;

    // vectors for all GameObjects and Enemies, init player, bools and gravity
    vector<GameObject> gameObjects;
    vector<Enemy> enemies;
    GameObject player{};
    bool isOnGround = true;
    bool canDoubleJump = false;
    float gravity = 0.3;
    Uint32 lastJumpTime = 0;
    Uint32 lastStepTime = 0;
    Uint32 stepCooldown = 685.877;

    // vector for all the level files, init selected index, game state, level rects, current level index and is last level
    vector<string> levelFiles = getLevelFiles("../levels");
    int selectedIndex = 0;
    GameState gameState = LEVEL_SELECT;
    vector<SDL_Rect> levelRects;
    int currentLevelIndex = 0;
    bool isLastLevel = false;

    bool quit = false;
    SDL_Event e;


    while (!quit) {
        Uint32 currentTime = SDL_GetTicks();
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            if (e.type == SDL_KEYDOWN) { // handle key presses for each game state
                if (gameState == LEVEL_SELECT) {
                    switch (e.key.keysym.sym) {
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
                } else if ((gameState == WON || gameState == LOST) && e.key.keysym.sym == SDLK_SPACE) {
                    quit = true;
                } else {
                    SDL_FRect newRect = player.rect;
                    float moveSpeed = TILE_SIZE;

                    switch (e.key.keysym.sym) {
                    case SDLK_SPACE:
                        if (isOnGround) {
                            Mix_PlayChannel(-1, jumpSound, 0);
                            gravity = 0.04;
                            isOnGround = false;
                            canDoubleJump = true;
                            newRect.y -= moveSpeed; // first jump
                            lastJumpTime = currentTime;
                        } else if (canDoubleJump && (currentTime - lastJumpTime) < 500) {
                            Mix_PlayChannel(-1, jumpSound, 0);
                            newRect.y -= moveSpeed;  // Double jump
                            canDoubleJump = false;
                        }
                        break;
                    case SDLK_w:
                        newRect.y -= moveSpeed;
                        break;
                    case SDLK_s:
                        newRect.y += moveSpeed;
                        break;
                    case SDLK_a:
                        newRect.x -= moveSpeed;
                        player.texture = marioTextureLeft;
                        if (currentTime - lastStepTime > stepCooldown) {
                            Mix_PlayChannel(-1, stepSound, 0);
                            lastStepTime = currentTime;
                        }
                        break;
                    case SDLK_d:
                        newRect.x += moveSpeed;
                        player.texture = marioTextureRight;
                        if (currentTime - lastStepTime > stepCooldown) {
                            Mix_PlayChannel(-1, stepSound, 0);
                            lastStepTime = currentTime;
                        }
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
                        collectedCoins = totalCoins;
                        gameState = WON;
                        break;
                    case SDLK_o:
                        collectedCoins = totalCoins;
                        isLastLevel = true;
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
                            ++collectedCoins;
                            erase_if(gameObjects, [&obj](const GameObject& o) {
                                return &o == &obj;
                            });
                            Mix_PlayChannel(0, coinSound, 0); // Play the coin sound on any available channel
                            break;
                        }
                    }

                    if (!collision) {
                        player.rect = newRect;
                    }
                }
            } else if (e.type == SDL_MOUSEBUTTONDOWN) { // clicking with the mouse
                if (gameState == LEVEL_SELECT) {
                    int mouseX, mouseY;
                    SDL_GetMouseState(&mouseX, &mouseY);
                    for (int i = 0; i < levelRects.size(); ++i) {
                        if (isPointInRect(mouseX, mouseY, levelRects[i])) {
                            selectedIndex = levelFiles.size() - 1 - i;
                            gameObjects.clear();
                            collectedCoins = 0;
                            loadLevel(levelFiles[selectedIndex], gameObjects, textures, enemies, player);
                            gameState = PLAYING;
                            break;
                        }
                    }
                } else if (gameState == WON) {
                    Mix_PauseMusic();
                    int mouseX, mouseY;
                    SDL_GetMouseState(&mouseX, &mouseY);
                    SDL_Rect nextLevelButton = { SCREEN_WIDTH / 2 - 60, SCREEN_HEIGHT / 2 + 40 - 100, 120, 40 };
                    if (isPointInRect(mouseX, mouseY, nextLevelButton)) {
                        ++currentLevelIndex;
                        if (currentLevelIndex >= levelFiles.size()) {
                            isLastLevel = true;
                            gameState = WON;
                        } else {
                            totalCoins = 0;
                            collectedCoins = 0;
                            gameObjects.clear();
                            enemies.clear();
                            selectedIndex = levelFiles.size() - currentLevelIndex - 1;
                            loadLevel(levelFiles[selectedIndex], gameObjects, textures, enemies, player);
                            gameState = PLAYING;
                            Mix_ResumeMusic(); // Resume the soundtrack
                            musicPlaying = true;
                            soundPlayed = false;
                        }
                    }
                }
            }
        }
        if (gameState == LEVEL_SELECT) {
            renderLevelSelectScreen(renderer, levelFiles, selectedIndex, levelRects, backgroundTexture, font);
        } else if (gameState == PLAYING) {
            for (const auto& enemy : enemies) {
                if (hasIntersection(player.rect, enemy.gameObject.rect)) {
                    gameState = LOST;
                }
            }
            if (isOnPlatform(player, gameObjects, brickTexture)) { // bs fix for jumping
                isOnGround = true;
                gravity = 0.3;
            } else {
                isOnGround = false;
            }
            if (!isOnPlatform(player, gameObjects, brickTexture) && !isOnVine(player, gameObjects, vineTexture)) { // apply gravity
                bool atTopOfVine = false;
                SDL_FRect belowPlayer = player.rect;
                belowPlayer.y += 1;

                for (const auto& obj : gameObjects) {
                    if (obj.texture == vineTexture && hasIntersection(belowPlayer, obj.rect)) {
                        atTopOfVine = true;
                        break;
                    }
                }

                if (!atTopOfVine) {
                    if (currentTime - lastJumpTime > 250) {
                        gravity = 0.5;
                    }
                    player.rect.y += gravity;
                    if (player.rect.y + player.rect.h > SCREEN_HEIGHT) { // imagine falling off the screen :')
                        player.rect.y = SCREEN_HEIGHT - player.rect.h;
                    }
                }
            }

            // render the screen and all the game objects
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
            SDL_RenderClear(renderer);

            SDL_RenderCopyF(renderer, backgroundTexture, nullptr, nullptr);
            for (const auto& obj : gameObjects) {
                SDL_RenderCopyF(renderer, obj.texture, nullptr, &obj.rect);
            }
            SDL_RenderCopyF(renderer, player.texture, nullptr, &player.rect);

            updateEnemies(enemies, player, killSound);
            for (const auto& enemy : enemies) {
                SDL_RenderCopyF(renderer, enemy.gameObject.texture, nullptr, &enemy.gameObject.rect);
            }

            // draw the coin counter and level text
            string coinText = "Coins: " + to_string(collectedCoins) + "/" + to_string(totalCoins);
            string atLevel = "Level: " + to_string(currentLevelIndex + 1);
            renderText(renderer, coinText, 10, SCREEN_HEIGHT - 40);
            renderText(renderer, atLevel, SCREEN_WIDTH - 124, SCREEN_HEIGHT - 40);

            SDL_RenderPresent(renderer);

            if (collectedCoins == totalCoins) {
                gameState = WON;
            }
        } else if (gameState == LOST) {
            if (musicPlaying) {
                Mix_HaltMusic();
                Mix_VolumeMusic(64);
                Mix_PlayMusic(lostSound, 1);
                musicPlaying = false;
            }
            renderLostScreen(renderer);
        } else {
            if (currentLevelIndex >= levelFiles.size() -1 ) {
                isLastLevel = true;
                if (!soundPlayed) {
                    Mix_PauseMusic();
                    Mix_PlayChannel(-1, clearSound, 0);
                    soundPlayed = true;
                }
            }
            if (!soundPlayed) {
                Mix_PauseMusic();
                Mix_PlayChannel(-1, wonSound, 0);
                soundPlayed = true;
            }
            renderWinningScreen(renderer, isLastLevel);
        }
    }

    // free up resources
    Mix_FreeMusic(soundtrack);
    Mix_FreeMusic(lostSound);
    Mix_FreeChunk(coinSound);
    Mix_FreeChunk(clearSound);
    Mix_FreeChunk(wonSound);
    Mix_FreeChunk(jumpSound);
    Mix_FreeChunk(killSound);
    Mix_FreeChunk(stepSound);
    for (auto texture : textures) {
        SDL_DestroyTexture(texture);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    Mix_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}
//NOLINTEND(cppcoreguidelines-narrowing-conversions)