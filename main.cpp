// ReSharper disable CppParameterMayBeConst
// ReSharper disable CppLocalVariableMayBeConst
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

struct Button {
    string text;
    float x;
    float y;
};

enum Character {
    mario,
    luigi,
    none
};

Character playerChar = mario;

enum GameState {
    START_SCREEN,
    SETTINGS,
    ABOUT,
    LEVEL_SELECT,
    PLAYING,
    TRANSITION,
    WON,
    DYING,
    LOST,
    MODE_SELECT
};

enum GameMode {
    NORMAL,
    CUSTOM
};

int totalCoins = 0;
int collectedCoins = 0;

TTF_Font* font = nullptr;
SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;

int lives = 3;
//int totalLives = 0;

//NOLINTBEGIN(cppcoreguidelines-narrowing-conversions)
void loadLevel(const string& filePath, vector<GameObject>& gameObjects, const vector<SDL_Texture*>& textures, const vector<SDL_Texture*>& playerTextures, vector<Enemy>& enemies, GameObject& player, GameObject& door) {
    ifstream levelFile(filePath);
    string line;
    float y = 0;
    bool playerInit = false;
    bool doorInit = false;

    while (getline(levelFile, line)) {
        vector<int> enemyPositions;
        for (int x = 0; x < line.length(); ++x) {
            char tile = line[x];
            SDL_FRect rect = { static_cast<float>(x * TILE_SIZE), y * TILE_SIZE, TILE_SIZE, TILE_SIZE };
            if (tile == '1') {
                gameObjects.push_back({ textures[1], rect });
            } else if (tile == '/') {
                gameObjects.push_back({ textures[2], rect });
            } else if (tile == '+')
            {
                gameObjects.push_back({ textures[3], rect });
                ++totalCoins;
            } else if (tile == '^') {
                gameObjects.push_back({textures[8], rect});
                //++totalLives;
            } else if (tile == '@') {
                if (playerInit) {
                    throw runtime_error("Error: Player character initialized more than once!");
                }
                player = { playerTextures[1], rect };
                playerInit = true;
            } else if (tile == '$') {
                enemyPositions.push_back(x);
            } else if (tile == 'D') {
                if (doorInit) {
                    throw runtime_error("Error: More than one door initialized!");
                }
                rect.h = TILE_SIZE * 2;
                rect.y -= TILE_SIZE;
                door = { textures[6], rect };
                doorInit = true;
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
                GameObject enemy = { textures[4], enemyRect };
                SDL_FRect path = { startX, y * TILE_SIZE, endX - startX, TILE_SIZE };
                enemies.push_back({ enemy, path, 0.05f, true, textures[4], textures[5], true });
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

bool isPointInRectF(int x, int y, const SDL_FRect& rect) {
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

void drawRectOutline(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawRect(renderer, &rect);
}

void drawRectOutlineF(SDL_Renderer* renderer, const SDL_FRect& rect, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawRectF(renderer, &rect);
}

float calcOffset(float n) {
    return (n * 30 - n * 5 / 3) / 4;
}

void renderText(SDL_Renderer* renderer, const string& text, float x, float y, SDL_Color textColor = { 255, 255, 255, 255 }) {
    SDL_Surface* textSurface = TTF_RenderUTF8_Solid(font, text.c_str(), textColor);
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    const float tsw = textSurface -> w;
    const float tsh = textSurface -> h;
    const SDL_FRect textRect = { x, y, tsw, tsh };
    SDL_RenderCopyF(renderer, textTexture, nullptr, &textRect);
    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);
}

//NOLINTBEGIN(bugprone-integer-division)
int padding = 10;

SDL_FRect buttonRect(const Button& button) {
    int textWidth, textHeight;
    TTF_SizeText(font, button.text.c_str(), &textWidth, &textHeight);

    SDL_FRect rect = { button.x - padding, button.y - padding / 2, static_cast<float>(textWidth + padding * 2), static_cast<float>(textHeight + padding) };
    return rect;
}

void renderButton(SDL_Renderer* renderer, const Button& button, SDL_Color color) {
    int textWidth, textHeight;
    TTF_SizeText(font, button.text.c_str(), &textWidth, &textHeight);

    // Add padding to the text size to determine the button size
    SDL_FRect rect = { button.x - padding, button.y - padding / 2, static_cast<float>(textWidth + padding * 2), static_cast<float>(textHeight + padding) };

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRectF(renderer, &rect);

    SDL_Color textColor = { 0, 0, 0, 255 }; // Black color for text
    renderText(renderer, button.text, button.x, button.y, textColor);
}

void renderBackgroundWithFadeOut(SDL_Renderer* renderer, SDL_Texture* backgroundTexture, uint alpha = 64) {
    SDL_RenderCopyF(renderer, backgroundTexture, nullptr, nullptr);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
    SDL_RenderFillRect(renderer, nullptr);
}

bool isButtonClicked(const SDL_FRect& rect, const int mouseX, const int mouseY) {
    return mouseX >= rect.x && mouseY >= rect.y && mouseX <= rect.x + rect.w && mouseY <= rect.y + rect.h;
}

Button normalModeButton = { "Normal Mode", SCREEN_WIDTH / 2 - calcOffset(12), SCREEN_HEIGHT / 2 - 16 };
Button levelSelectButton = { "Level Select", SCREEN_WIDTH / 2 - calcOffset(13), SCREEN_HEIGHT / 2 + 32 };

void renderModeSelectScreen(SDL_Renderer* renderer, SDL_Texture* backgroundTexture) {
    SDL_RenderClear(renderer);
    renderBackgroundWithFadeOut(renderer, backgroundTexture);

    renderText(renderer, "Select Mode", SCREEN_WIDTH / 2 - calcOffset(11), SCREEN_HEIGHT / 2 - 64 );

    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    SDL_Color buttonColor = { 255, 255, 255, 128 };
    SDL_Color buttonHoverColor = { 0, 255, 0, 128 };

    if (isPointInRectF(mouseX, mouseY, buttonRect(normalModeButton)))
        renderButton(renderer, normalModeButton, buttonHoverColor);
    renderButton(renderer, normalModeButton, buttonColor);

    if (isPointInRectF(mouseX, mouseY, buttonRect(levelSelectButton)))
        renderButton(renderer, levelSelectButton, buttonHoverColor);
    renderButton(renderer, levelSelectButton, buttonColor);

    SDL_RenderPresent(renderer);
}

Button playButton = {"Play", SCREEN_WIDTH / 2 - calcOffset(4), SCREEN_HEIGHT / 2 - 16};
Button settingsButton = { "Settings", SCREEN_WIDTH / 2 - calcOffset(8), SCREEN_HEIGHT / 2 + 32 };

void renderStartScreen(SDL_Renderer* renderer, SDL_Texture* backgroundTexture) {
    SDL_RenderClear(renderer);
    renderBackgroundWithFadeOut(renderer, backgroundTexture);

    renderText(renderer, "Super Mario", SCREEN_WIDTH / 2 - calcOffset(11), SCREEN_HEIGHT / 2 - 64 );

    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    SDL_Color buttonColor = { 255, 255, 255, 128 };
    SDL_Color buttonHoverColor = { 0, 255, 0, 128 };

    if (isPointInRectF(mouseX, mouseY, buttonRect(playButton)))
        renderButton(renderer, playButton, buttonHoverColor);
    renderButton(renderer, playButton, buttonColor);

    if (isPointInRectF(mouseX, mouseY, buttonRect(settingsButton)))
        renderButton(renderer, settingsButton, buttonHoverColor);
    renderButton(renderer, settingsButton, buttonColor);

    SDL_RenderPresent(renderer);
}

SDL_FRect marioRect = { SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 - 100, 50, 50 };
SDL_FRect luigiRect = { SCREEN_WIDTH / 2 + 50, SCREEN_HEIGHT / 2 - 100, 50, 50 };

Button aboutButton = { "About", SCREEN_WIDTH / 2 - calcOffset(5), SCREEN_HEIGHT / 2 + 32 };

vector SettingsButtons = { aboutButton };

void renderSettingsScreen(SDL_Renderer* renderer, SDL_Texture* backgroundTexture) {
    SDL_RenderClear(renderer);
    renderBackgroundWithFadeOut(renderer, backgroundTexture);

    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    SDL_Color buttonColor = { 255, 255, 255, 128 };
    SDL_Color buttonHoverColor = { 0, 255, 0, 128 };

    renderText(renderer, "Change Character", SCREEN_WIDTH / 2 - calcOffset(16), SCREEN_HEIGHT / 2 - 150);

    if (isPointInRectF(mouseX, mouseY, buttonRect(aboutButton))) {
        renderButton(renderer, aboutButton, buttonHoverColor);
    } else {
        renderButton(renderer, aboutButton, buttonColor);
    }

    // Draw the two characters
    SDL_Texture* marioTexture = IMG_LoadTexture(renderer, "../resources/player/mario/left.png");
    SDL_Texture* luigiTexture = IMG_LoadTexture(renderer, "../resources/player/luigi/left.png");

    SDL_RenderCopyF(renderer, marioTexture, nullptr, &marioRect);
    SDL_RenderCopyF(renderer, luigiTexture, nullptr, &luigiRect);

    // Draw a thick square border around the selected character
    Character selectedCharacter = (playerChar == mario) ? mario : luigi;
    selectedCharacter == mario ? SDL_SetRenderDrawColor(renderer, 255, 0, 0, 200) : SDL_SetRenderDrawColor(renderer, 0, 255, 0, 200);

    SDL_FRect selectedRect = (selectedCharacter == mario) ? marioRect : luigiRect;
    selectedRect.x -= 5;
    selectedRect.y -= 5;
    selectedRect.w += 10;
    selectedRect.h += 10;
    for (int i = 0; i < 5; ++i) { // Draw a thick border
        SDL_RenderDrawRectF(renderer, &selectedRect);
        selectedRect.x -= 2;
        selectedRect.y -= 2;
        selectedRect.w += 4;
        selectedRect.h += 4;
    }

    SDL_DestroyTexture(marioTexture);
    SDL_DestroyTexture(luigiTexture);

    SDL_RenderPresent(renderer);
}

void renderAboutScreen(SDL_Renderer* renderer, SDL_Texture* backgroundTexture) {
    SDL_RenderClear(renderer);
    renderBackgroundWithFadeOut(renderer, backgroundTexture, 128);

    renderText(renderer, "Project made by Gurzu Matei and Romila Raluca", SCREEN_WIDTH / 2 - calcOffset(45), SCREEN_HEIGHT / 2 - 64);
    renderText(renderer, "Code licensed under GNU GPL 3.0", SCREEN_WIDTH / 2 - calcOffset(31), SCREEN_HEIGHT / 2);
    renderText(renderer, "Assets (excluding /resources/font) © Nintendo Co., Ltd.", SCREEN_WIDTH / 2 - calcOffset(55), SCREEN_HEIGHT / 2 + 32);
    renderText(renderer, "Font licensed under the SIL OFL 1.1", SCREEN_WIDTH / 2 - calcOffset(35), SCREEN_HEIGHT / 2 + 64);

    SDL_RenderPresent(renderer);
}

vector<string> getLevelFiles(const string& folderPath) {
    vector<string> levelFiles;

    for (const auto& entry : directory_iterator(folderPath)) {
        if (entry.path().extension() == ".lvl") {
            levelFiles.push_back(entry.path().string());
        }
    }

    // Manual bubble sort
    for (size_t i = 0; i < levelFiles.size(); ++i) {
        for (size_t j = 0; j < levelFiles.size() - i - 1; ++j) {
            if (levelFiles[j] > levelFiles[j + 1]) {
                swap(levelFiles[j], levelFiles[j + 1]);
            }
        }
    }
    return levelFiles;
}

SDL_Rect leftArrowRect = { SCREEN_WIDTH / 2 - 150, 250, 50, 50 };
SDL_Rect rightArrowRect = { SCREEN_WIDTH / 2 + 100, 250, 50, 50 };

static int levelScrollOffset = 0;

void renderLevelSelectScreen(SDL_Renderer* renderer, const vector<string>& levelFiles, vector<SDL_Rect>& levelRects, SDL_Texture* backgroundTexture) {
    SDL_RenderCopyF(renderer, backgroundTexture, nullptr, nullptr);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 96);
    SDL_RenderFillRect(renderer, nullptr);

    renderText(renderer, "Select a Level", SCREEN_WIDTH / 2 - 100, 150);

    levelRects.clear();
    SDL_Color outlineColor = { 255, 255, 255, 255 }; // White color for the outline
    SDL_Color hoverColor = { 255, 0, 0, 255 }; // Red color for hover effect

    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);
    Sint32 numberOfLevels = levelFiles.size();

    // If there are more than 5 levels, draw arrows and only show a subset
    if (levelFiles.size() > 5) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 51);
        SDL_RenderFillRect(renderer, &leftArrowRect);
        SDL_RenderFillRect(renderer, &rightArrowRect);

        renderText(renderer, "<", leftArrowRect.x + 17, leftArrowRect.y + 10);
        renderText(renderer, ">", rightArrowRect.x + 17, rightArrowRect.y + 10);

        // Only display levels from levelScrollOffset to levelScrollOffset + 5
        for (int i = levelScrollOffset; i < min(levelScrollOffset + 5, numberOfLevels); ++i) {
            string levelName = levelFiles[i].substr(levelFiles[i].find_last_of("/\\") + 1);

            int x = SCREEN_WIDTH / 2 - calcOffset(levelName.length());
            int y = 200 + (i - levelScrollOffset) * 30;
            renderText(renderer, levelName, x, y + ((i - levelScrollOffset) * 5));

            SDL_Rect rect = { x - 5 , y + ((i - levelScrollOffset) * 5), static_cast<int>(levelName.length() * 15), 30 };
            levelRects.push_back(rect);

            if (isPointInRect(mouseX, mouseY, rect)) {
                drawRectOutline(renderer, rect, hoverColor);
            } else {
                drawRectOutline(renderer, rect, outlineColor);
            }
        }
    } else {
        // Display all levels because there are 5 or fewer
        for (int i = 0; i < levelFiles.size(); ++i) {
            string levelName = levelFiles[i].substr(levelFiles[i].find_last_of("/\\") + 1);

            int x = SCREEN_WIDTH / 2 - calcOffset(levelName.length());
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
    }

    SDL_RenderPresent(renderer);
}

float enemySpeed = 0.05f;

void updateEnemies(vector<Enemy>& enemies, const GameObject& player, Mix_Chunk* killSound) {
    for (auto& enemy : enemies) {
        float previousX = enemy.gameObject.rect.x;

        if (enemy.movingRight) {
            enemy.gameObject.rect.x += enemySpeed;
            if (enemy.gameObject.rect.x >= enemy.path.x + enemy.path.w) {
                enemy.movingRight = false;
            }
        } else {
            enemy.gameObject.rect.x -= enemySpeed;
            if (enemy.gameObject.rect.x <= enemy.path.x) {
                enemy.movingRight = true;
            }
        }

        if (previousX / TILE_SIZE != enemy.gameObject.rect.x / TILE_SIZE) {
            enemy.useLeftTexture = !enemy.useLeftTexture;
            enemy.gameObject.texture = enemy.useLeftTexture ? enemy.textureLeft : enemy.textureRight;
        }

        // Check if the player intersects with the top of the enemy
        SDL_FRect playerBottom = player.rect;
        playerBottom.y += player.rect.h;
        playerBottom.h = 1;

        SDL_FRect enemyTop = enemy.gameObject.rect;
        enemyTop.h = 1;

        if (hasIntersection(playerBottom, enemyTop)) {
            Mix_PlayChannel(-1, killSound, 0); // Play the kill sound
            erase_if(enemies, [&enemy](const Enemy& o) {
                return &o == &enemy;
            });
            break;
        }
    }
}

SDL_FRect nextLevelButton = { SCREEN_WIDTH / 2 - calcOffset(10) - 5, SCREEN_HEIGHT / 2 + 32, 150, 32 };

void renderWinningScreen(SDL_Renderer* renderer, bool isLastLevel) {
    SDL_Color outlineColor = { 255, 255, 255, 255 }; // White color for the outline
    SDL_Color hoverColor = { 0, 255, 0, 255 }; // Green color for hover effect

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    if (isLastLevel) {
        renderText(renderer, "You have completed the game!", SCREEN_WIDTH / 2 - calcOffset(28), SCREEN_HEIGHT / 2 - 32);
        renderText(renderer, "Press Space to exit", SCREEN_WIDTH / 2 - calcOffset(19), SCREEN_HEIGHT / 2 + 32);
    } else {
        renderText(renderer, "You won!", SCREEN_WIDTH / 2 - calcOffset(8), SCREEN_HEIGHT / 2 - 32);
        renderText(renderer, "Next Level", SCREEN_WIDTH / 2 - calcOffset(10), SCREEN_HEIGHT / 2 + 32);
        if (isPointInRectF(mouseX, mouseY, nextLevelButton)) {
            drawRectOutlineF(renderer, nextLevelButton, hoverColor);
        } else {
            drawRectOutlineF(renderer, nextLevelButton, outlineColor);
        }
    }

    SDL_RenderPresent(renderer);
}

Button retryLevelButton = { "Retry level", SCREEN_WIDTH / 2 - calcOffset(11), SCREEN_HEIGHT / 2 };
Button tryAgainButton = { "Try again", SCREEN_WIDTH / 2 - calcOffset(9), SCREEN_HEIGHT / 2 + 32 };

void renderLostScreen(SDL_Renderer* renderer, const string& reason) {
    SDL_Color hoverColor = { 0, 255, 0, 255 };

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    if (reason == "time") {
        renderText(renderer, "Time's up!", SCREEN_WIDTH / 2 - calcOffset(10), SCREEN_HEIGHT / 2 - 64);
    } else if (reason == "lives") {
        renderText(renderer, "You ran out of lives!", SCREEN_WIDTH / 2 - calcOffset(21), SCREEN_HEIGHT / 2 - 64);
    } else {
        renderText(renderer, "You Lost!", SCREEN_WIDTH / 2 - calcOffset(9), SCREEN_HEIGHT / 2 - 64);
    }
    //renderText(renderer, "Press Space to exit", SCREEN_WIDTH / 2 - calcOffset(19), SCREEN_HEIGHT / 2 + 64);

    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    if (reason == "lives")
    {
        renderButton(renderer, tryAgainButton, { 255, 255, 255, 128 });
        if (isPointInRectF(mouseX, mouseY, buttonRect(tryAgainButton))) {
            drawRectOutlineF(renderer, buttonRect(tryAgainButton), hoverColor);
        }
    } else {
        renderButton(renderer, retryLevelButton, { 255, 255, 255, 128 });
        if (isPointInRectF(mouseX, mouseY, buttonRect(retryLevelButton))) {
            drawRectOutlineF(renderer, buttonRect(retryLevelButton), hoverColor);
        }
    }
    SDL_RenderPresent(renderer);
}
//NOLINTEND(bugprone-integer-division)

vector<SDL_Texture*> loadBackgroundTextures(SDL_Renderer* renderer, const string& folderPath) {
    vector<SDL_Texture*> backgroundTextures;
    for (const auto& entry : directory_iterator(folderPath)) {
        if (entry.path().extension() == ".png") {
            // ReSharper disable once CppTooWideScope
            SDL_Texture* texture = IMG_LoadTexture(renderer, entry.path().string().c_str());
            if (texture) {
                backgroundTextures.push_back(texture);
            } else {
                cerr << "Failed to load texture: " << entry.path() << " " << SDL_GetError() << endl;
            }
        }
    }

    // Manual bubble sort
    for (size_t i = 0; i < backgroundTextures.size(); ++i) {
        for (size_t j = 0; j < backgroundTextures.size() - i - 1; ++j) {
            if (backgroundTextures[j] > backgroundTextures[j + 1]) {
                swap(backgroundTextures[j], backgroundTextures[j + 1]);
            }
        }
    }
    return backgroundTextures;
}

void changeBackground(const vector<SDL_Texture*>& backgrounds,vector<SDL_Texture*>& textures, const int& currentIndex) {
    cout << "Changing background to: " << ((currentIndex + 1) % backgrounds.size()) << endl;
    cout << "Current index: " << currentIndex << endl;
    int index;
    if (currentIndex >= backgrounds.size() - 1) {
        index = 0;
    } else {
        index = currentIndex;
    }
    cout << "New index: " << index << endl;
    textures[0] = backgrounds[index];
}

vector<SDL_Texture*> playerTextures = {};

SDL_Texture* playerTextureLeft = nullptr;
SDL_Texture* playerTextureRight = nullptr;
SDL_Texture* playerTextureWalkingLeft = nullptr;
SDL_Texture* playerTextureWalkingRight = nullptr;
SDL_Texture* playerTextureLost = nullptr;
SDL_Texture* playerTextureJumpingLeft = nullptr;
SDL_Texture* playerTextureJumpingRight = nullptr;

vector<SDL_Texture*> switchCharacter(Character character, SDL_Renderer* renderer) {
    string characterStr = (character == mario) ? "mario" : "luigi";

    for (auto texture : playerTextures) {
        SDL_DestroyTexture(texture);
    }
    playerTextures.clear();

    playerTextureLeft = IMG_LoadTexture(renderer, ("../resources/player/" + characterStr + "/left.png").c_str());
    playerTextureRight = IMG_LoadTexture(renderer, ("../resources/player/" + characterStr + "/right.png").c_str());
    playerTextureWalkingLeft = IMG_LoadTexture(renderer, ("../resources/player/" + characterStr + "/walkingleft.png").c_str());
    playerTextureWalkingRight = IMG_LoadTexture(renderer, ("../resources/player/" + characterStr + "/walkingright.png").c_str());
    playerTextureLost = IMG_LoadTexture(renderer, ("../resources/player/" + characterStr + "/lost.png").c_str());
    playerTextureJumpingLeft = IMG_LoadTexture(renderer, ("../resources/player/" + characterStr + "/jumpingleft.png").c_str());
    playerTextureJumpingRight = IMG_LoadTexture(renderer, ("../resources/player/" + characterStr + "/jumpingright.png").c_str());

    playerTextures = { playerTextureLeft, playerTextureRight, playerTextureWalkingLeft, playerTextureWalkingRight, playerTextureLost, playerTextureJumpingLeft, playerTextureJumpingRight };

    for (auto& texture : playerTextures) {
        if (!texture) {
            cerr << "Failed to load textures!" << endl << SDL_GetError() << endl;
            SDL_Quit();
            return vector<SDL_Texture*>(-1);
        }
    }

    return playerTextures;
}

int main() {
    // init stuff
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    window = SDL_CreateWindow("Mario", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    IMG_Init(IMG_INIT_PNG);
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    TTF_Init();
    font = TTF_OpenFont("../resources/font/firacode.ttf", 24);

    vector<SDL_Texture*> backgroundTextures = loadBackgroundTextures(renderer, "../resources/backgrounds");
    int currentBackgroundIndex = 0;

    SDL_Texture* brickTexture = IMG_LoadTexture(renderer, "../resources/brick.png");
    SDL_Texture* vineTexture = IMG_LoadTexture(renderer, "../resources/vine.png");
    SDL_Texture* starCoinTexture = IMG_LoadTexture(renderer, "../resources/star-coin.png");
    SDL_Texture* enemyTextureLeft = IMG_LoadTexture(renderer, "../resources/enemy/left.png");
    SDL_Texture* enemyTextureRight = IMG_LoadTexture(renderer, "../resources/enemy/right.png");
    SDL_Texture* doorTextureClosed = IMG_LoadTexture(renderer, "../resources/door/closed.png");
    SDL_Texture* doorTextureOpen = IMG_LoadTexture(renderer, "../resources/door/open.png");
    SDL_Texture* lifeTexture = IMG_LoadTexture(renderer, "../resources/life.png");

    vector textures = { backgroundTextures[currentBackgroundIndex], brickTexture, vineTexture, starCoinTexture, enemyTextureLeft, enemyTextureRight, doorTextureClosed, doorTextureOpen, lifeTexture };

    for (auto& texture : textures) {
        if (!texture) {
            cerr << "Failed to load textures!" << endl << SDL_GetError() << endl;
            SDL_Quit();
            return -1;
        }
    }

    // load music
    Mix_Music* soundtrack = Mix_LoadMUS("../resources/sounds/soundtrack.mp3");
    Mix_Chunk* lostSound = Mix_LoadWAV("../resources/sounds/lost.wav");
    Mix_Chunk* coinSound = Mix_LoadWAV("../resources/sounds/coin.mp3");
    Mix_Chunk* clearSound = Mix_LoadWAV("../resources/sounds/clear.mp3");
    Mix_Chunk* wonSound = Mix_LoadWAV("../resources/sounds/won.mp3");
    Mix_Chunk* jumpSound = Mix_LoadWAV("../resources/sounds/jump.wav");
    Mix_Chunk* killSound = Mix_LoadWAV("../resources/sounds/kill.mp3");
    Mix_Chunk* stepSound = Mix_LoadWAV("../resources/sounds/steps.mp3");
    vector sounds = { lostSound, coinSound, clearSound, wonSound, jumpSound, killSound, stepSound };

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
    GameObject door{};
    bool isOnGround = true;
    bool canDoubleJump = false;
    bool jumped = false;
    bool isWalkingLeft = false;
    float gravity = 0.8;
    Sint32 levelStartTime = 0;
    Sint32 lastJumpTime = 0;
    Sint32 lastStepTime = 0;
    Sint32 dyingStartTime = 0;
    Sint32 transitionStartTime = 0;
    string deathReason;
    bool noMoreLives = false;

    // vector for all the level files, init selected index, game state, level rects, current level index and is last level
    vector<string> levelFiles = getLevelFiles("../levels");
    GameState gameState = START_SCREEN;
    GameMode gameMode = NORMAL;
    vector<SDL_Rect> levelRects;
    int currentLevelIndex = 0;
    bool isLastLevel = false;

    bool quit = false;
    SDL_Event e;

    while (!quit) {
        Sint32 currentTime = SDL_GetTicks();
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
            if (e.type == SDL_KEYDOWN) { // handle key presses for each game state
                switch (e.key.keysym.sym) {
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
                if (gameState == START_SCREEN) {
                    switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        quit = true;
                        break;
                    default: break;
                    }
                } else if (gameState == SETTINGS || gameState == MODE_SELECT) {
                    switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        gameState = START_SCREEN;
                        break;
                    default: break;
                    }
                } else if (gameState == LEVEL_SELECT) {
                    switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        gameState = MODE_SELECT;
                        break;
                    default: break;
                    }
                } else if (gameState == ABOUT) {
                    switch (e.key.keysym.sym) {
                        case SDLK_ESCAPE:
                            gameState = SETTINGS;
                            break;
                        default: break;
                    }
                } else if ((gameState == WON || gameState == LOST) && e.key.keysym.sym == SDLK_SPACE) {
                    // ReSharper disable once CppExpressionWithoutSideEffects
                    // ReSharper disable once CppDFAConstantConditions
                    gameState == START_SCREEN;
                } else if (gameState == DYING) {
                    if (e.key.keysym.sym == SDLK_SPACE) {
                        gameState = LOST;
                    }
                } else if (gameState == PLAYING) {
                    Sint32 stepCooldown = 685.877;
                    SDL_FRect newRect = player.rect;
                    float moveSpeed = TILE_SIZE;

                    switch (e.key.keysym.sym) {
                    case SDLK_SPACE:
                        if (isOnGround) {
                            if (isWalkingLeft) {
                                player.texture = playerTextures[5];
                            } else {
                                player.texture = playerTextures[6];
                            }
                            Mix_PlayChannel(-1, jumpSound, 0);
                            gravity = 0.04;
                            isOnGround = false;
                            jumped = true;
                            canDoubleJump = true;
                            newRect.y -= moveSpeed; // first jump
                            lastJumpTime = currentTime;
                        } else if (canDoubleJump && (currentTime - lastJumpTime) < 500) {
                            Mix_PlayChannel(-1, jumpSound, 0);
                            newRect.y -= moveSpeed;  // Double jump
                            jumped = true;
                            canDoubleJump = false;
                        }
                        break;
                    case SDLK_w:
                        if (hasIntersection(player.rect, door.rect) && collectedCoins == totalCoins) {
                            gameState = TRANSITION;
                            transitionStartTime = currentTime;
                            door.texture = doorTextureOpen;
                        } else { newRect.y -= moveSpeed; }
                        break;
                    case SDLK_s:
                        newRect.y += moveSpeed;
                        break;
                    case SDLK_a:
                        newRect.x -= moveSpeed;
                        player.texture = playerTextureWalkingLeft;
                        if (currentTime - lastStepTime > stepCooldown) {
                            Mix_PlayChannel(-1, stepSound, 0);
                            lastStepTime = currentTime;
                        }
                        isWalkingLeft = true;
                        break;
                    case SDLK_d:
                        newRect.x += moveSpeed;
                        player.texture = playerTextureWalkingRight;
                        if (currentTime - lastStepTime > stepCooldown) {
                            Mix_PlayChannel(-1, stepSound, 0);
                            lastStepTime = currentTime;
                        }
                        isWalkingLeft = false;
                        break;
                    case SDLK_p:
                        collectedCoins = totalCoins;
                        break;
                    case SDLK_o:
                        collectedCoins = totalCoins;
                        isLastLevel = true;
                        gameState = WON;
                        break;
                    case SDLK_l:
                        if (musicPlaying) {
                            Mix_HaltMusic();
                            Mix_VolumeMusic(64);
                            Mix_PlayChannel(-1, lostSound, 0);
                            musicPlaying = false;
                        }
                        dyingStartTime = currentTime;
                        player.texture = playerTextures[4];
                        gameState = DYING;
                        break;
                    case SDLK_ESCAPE:
                        gameState = START_SCREEN;
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
                        if (obj.texture == starCoinTexture && hasIntersection(newRect, obj.rect)) {
                            ++collectedCoins;
                            erase_if(gameObjects, [&obj](const GameObject& o) {
                                return &o == &obj;
                            });
                            Mix_PlayChannel(0, coinSound, 0); // Play the coin sound on any available channel
                            break;
                        }
                        if (obj.texture == lifeTexture && hasIntersection(newRect, obj.rect)) {
                            ++lives;
                            erase_if(gameObjects, [&obj](const GameObject& o) {
                                return &o == &obj;
                            });
                            Mix_PlayChannel(0, coinSound, 0); // Play the coin sound on any available channel
                            break;
                        }
                        if (obj.texture != vineTexture && obj.texture != starCoinTexture && hasIntersection(newRect, obj.rect)) {
                            collision = true;
                            break;
                        }
                    }

                    if (!collision) {
                        player.rect = newRect;
                    }

                }
            } else if (e.type == SDL_MOUSEBUTTONDOWN) { // clicking with the mouse
                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);

                if (gameState == LEVEL_SELECT) {
                    for (int i = 0; i < levelRects.size(); ++i) {
                        if (isPointInRect(mouseX, mouseY, levelRects[i])) {
                            currentLevelIndex = i + levelScrollOffset;
                            gameObjects.clear();
                            collectedCoins = 0;
                            playerTextures = switchCharacter(playerChar, renderer);
                            loadLevel(levelFiles[i], gameObjects, textures, playerTextures, enemies, player, door);
                            gameState = PLAYING;
                            levelStartTime = 0;
                            break;
                        }
                    }
                    if (isPointInRect(mouseX, mouseY, leftArrowRect)) {
                        levelScrollOffset = max(0, levelScrollOffset - 1);
                    } else if (isPointInRect(mouseX, mouseY, rightArrowRect)) {
                        Sint32 numberOfLevels = levelFiles.size();
                        levelScrollOffset = min(numberOfLevels - 5, levelScrollOffset + 1);
                    }
                } else if (gameState == WON) {
                    Mix_PauseMusic();
                    if (isPointInRectF(mouseX, mouseY, nextLevelButton)) {
                        if (soundPlayed) {
                            Mix_HaltGroup(-1);
                        }
                        ++currentLevelIndex;
                        if (currentLevelIndex >= levelFiles.size()) {
                            isLastLevel = true;
                            gameState = WON;
                        } else {
                            totalCoins = 0;
                            collectedCoins = 0;
                            gameObjects.clear();
                            enemies.clear();
                            door.texture = doorTextureClosed;
                            changeBackground(backgroundTextures, textures, currentLevelIndex);
                            loadLevel(levelFiles[currentLevelIndex], gameObjects, textures, playerTextures, enemies, player, door);
                            gameState = PLAYING;
                            Mix_ResumeMusic();
                            musicPlaying = true;
                            soundPlayed = false;
                        }
                    }
                } else if (gameState == START_SCREEN) {
                    if (isButtonClicked(buttonRect(playButton), mouseX, mouseY)){
                        gameState = MODE_SELECT;
                    }
                    if (isButtonClicked(buttonRect(settingsButton), mouseX, mouseY)) {
                        gameState = SETTINGS;
                    }
                } else if (gameState == SETTINGS) {
                    if (isPointInRectF(mouseX, mouseY, marioRect)) {
                        playerChar = mario;
                    } else if (isPointInRectF(mouseX, mouseY, luigiRect)) {
                        playerChar = luigi;
                    }
                    if (isButtonClicked(buttonRect(aboutButton), mouseX, mouseY)) {
                        gameState = ABOUT;
                    }
                } else if (gameState == MODE_SELECT) {
                    if (isButtonClicked(buttonRect(normalModeButton), mouseX, mouseY)) {
                        gameObjects.clear();
                        enemies.clear();
                        totalCoins = 0;
                        collectedCoins = 0;
                        playerTextures = switchCharacter(playerChar, renderer);
                        loadLevel(levelFiles[0], gameObjects, textures, playerTextures, enemies, player, door);
                        gameState = PLAYING;
                        levelStartTime = 0;
                    }
                    if (isButtonClicked(buttonRect(levelSelectButton), mouseX, mouseY)) {
                        gameState = LEVEL_SELECT;
                        gameMode = CUSTOM;
                    }
                } else if (gameState == LOST) {
                    if (isPointInRectF(mouseX, mouseY, buttonRect(retryLevelButton)) || isPointInRectF(mouseX, mouseY, buttonRect(tryAgainButton))) {
                        totalCoins = 0;
                        collectedCoins = 0;
                        gameObjects.clear();
                        enemies.clear();
                        if (noMoreLives) {
                            currentLevelIndex = 0;
                            changeBackground(backgroundTextures, textures, currentLevelIndex);
                            loadLevel(levelFiles[currentLevelIndex], gameObjects, textures, playerTextures, enemies, player, door);
                        } else {
                            loadLevel(levelFiles[currentLevelIndex], gameObjects, textures, playerTextures, enemies, player, door);
                        }
                        gameState = PLAYING;
                        Mix_ResumeMusic();
                        musicPlaying = true;
                        soundPlayed = false;
                    }
                }
            }
        }
        if (gameState == START_SCREEN) {
            renderStartScreen(renderer, textures[0]);
        } else if (gameState == MODE_SELECT) {
            renderModeSelectScreen(renderer, textures[0]);
        } else if (gameState == SETTINGS) {
            renderSettingsScreen(renderer, textures[0]);
        } else if (gameState == ABOUT) {
            renderAboutScreen(renderer, textures[0]);
        } else if (gameState == LEVEL_SELECT) {
            levelStartTime = 0;
            renderLevelSelectScreen(renderer, levelFiles, levelRects, textures[0]);
        } else if (gameState == PLAYING) {
            constexpr Sint32 levelTimeLimit = 100000;
            if (levelStartTime == 0) {
                levelStartTime = SDL_GetTicks();
            }

            Sint32 remainingTime = (levelTimeLimit > (currentTime - levelStartTime)) ? (levelTimeLimit - (currentTime - levelStartTime)) / 1000 : 0;

            if (levelStartTime > 0 && (currentTime - levelStartTime) > levelTimeLimit) {
                if (musicPlaying) {
                    Mix_PauseMusic();
                    Mix_VolumeMusic(64);
                    Mix_PlayChannel(-1, lostSound, 0);
                    musicPlaying = false;
                }
                dyingStartTime = currentTime;
                player.texture = playerTextures[4];
                deathReason = "time";
                gameState = DYING;
            }

            for (const auto& enemy : enemies) {
                if (hasIntersection(player.rect, enemy.gameObject.rect)) {
                    if (musicPlaying) {
                        Mix_PauseMusic();
                        Mix_VolumeMusic(64);
                        Mix_PlayChannel(-1, lostSound, 0);
                        musicPlaying = false;
                    }
                    dyingStartTime = currentTime;
                    player.texture = playerTextures[4];
                    deathReason = "enemy";
                    gameState = DYING;
                }
            }
            if ( jumped && isOnPlatform(player, gameObjects, brickTexture)) { // bs fix for jumping
                isOnGround = true;
                gravity = 0.15;
                jumped = false;

                if (isWalkingLeft) {
                    player.texture = playerTextureLeft;
                } else {
                    player.texture = playerTextureRight;
                }
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
                    if (currentTime - lastJumpTime > 500) {
                        gravity = 0.8;
                    }
                    player.rect.y += gravity;
                    if (player.rect.y + player.rect.h > SCREEN_HEIGHT) { // imagine falling off the screen :')
                        player.rect.y = SCREEN_HEIGHT - player.rect.h;
                    }
                }
            }

            // Calculate the y position of the last tile row
            float lastTileRowY = (SCREEN_HEIGHT / TILE_SIZE - 1) * TILE_SIZE; // NOLINT(*-integer-division)

            // Check if the player's y position is more than the last tile row's y position
            if (player.rect.y >= lastTileRowY) {
                if (musicPlaying) {
                    Mix_PauseMusic();
                    Mix_VolumeMusic(64);
                    Mix_PlayChannel(-1, lostSound, 0);
                    musicPlaying = false;
                }
                dyingStartTime = currentTime;
                player.texture = playerTextures[4];
                deathReason = "fall";
                gameState = DYING;
            }

            // render the screen and all the game objects
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
            SDL_RenderClear(renderer);

            SDL_RenderCopyF(renderer, textures[0], nullptr, nullptr);
            for (const auto& obj : gameObjects) {
                SDL_RenderCopyF(renderer, obj.texture, nullptr, &obj.rect);
            }
            SDL_RenderCopyF(renderer, door.texture, nullptr, &door.rect);
            SDL_RenderCopyF(renderer, player.texture, nullptr, &player.rect);

            updateEnemies(enemies, player, killSound);
            for (const auto& enemy : enemies) {
                SDL_RenderCopyF(renderer, enemy.gameObject.texture, nullptr, &enemy.gameObject.rect);
            }

            // Render the remaining time on the screen
            char* timeText = new char[("Time: " + to_string(remainingTime)).length() + 1];
            strcpy(timeText, ("Time: " + to_string(remainingTime)).c_str());

            renderText(renderer, timeText, SCREEN_WIDTH / 2 - calcOffset(strlen(timeText)), SCREEN_HEIGHT - 32); // NOLINT(*-integer-division)
            delete[] timeText;

            // draw the coin counter and level text
            string coinText = "Coins: " + to_string(collectedCoins) + "/" + to_string(totalCoins);
            string atLevel = "Level: " + to_string(currentLevelIndex + 1);
            renderText(renderer, coinText, 10, SCREEN_HEIGHT - 32);
            renderText(renderer, atLevel, SCREEN_WIDTH - 124, SCREEN_HEIGHT - 32);

            for (int i = 0; i < lives; ++i) {
                SDL_Rect lifeRect = { SCREEN_WIDTH - (i + 1) * (TILE_SIZE + 5), 10, TILE_SIZE, TILE_SIZE };
                SDL_RenderCopy(renderer, lifeTexture, nullptr, &lifeRect);
            }

            SDL_RenderPresent(renderer);
        } else if (gameState == TRANSITION) {

            float transitionDuration = 2000;
            // ReSharper disable once CppTooWideScopeInitStatement
            float progress = (currentTime - transitionStartTime) / transitionDuration;

            if (progress >= 1.0f) {
                gameState = WON;
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

                SDL_RenderClear(renderer);

                // Render the player and other game objects here
                SDL_RenderCopyF(renderer, textures[0], nullptr, nullptr);
                for (const auto& obj : gameObjects) {
                    SDL_RenderCopyF(renderer, obj.texture, nullptr, &obj.rect);
                }
                SDL_RenderCopyF(renderer, door.texture, nullptr, &door.rect);
                SDL_RenderCopyF(renderer, player.texture, nullptr, &player.rect);

                // Apply the fade effect
                Uint8 alpha = progress * 255;
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
                SDL_RenderFillRect(renderer, nullptr);

                SDL_RenderPresent(renderer);
            }
        } else if (gameState == DYING) {
            float dyingDuration = 2000;
            // ReSharper disable once CppTooWideScopeInitStatement
            float progress = (currentTime - dyingStartTime) / dyingDuration;

            if (progress >= 1.0f) {
                gameState = LOST;
                --lives;
                if (lives <= 0) {
                    noMoreLives = true;
                    deathReason = "lives";
                }
            } else {
                if (progress < 0.2f) {
                    player.rect.y += 0;
                }
                else if (progress < 0.5f) {
                    player.rect.y -= 0.02; // Move the player up
                } else if (progress > 0.5f){
                    player.rect.y += 0.09; // Move the player down faster
                }

                SDL_RenderClear(renderer);

                // Render the player and other game objects here
                SDL_RenderCopyF(renderer, textures[0], nullptr, nullptr);
                for (const auto& obj : gameObjects) {
                    SDL_RenderCopyF(renderer, obj.texture, nullptr, &obj.rect);
                }
                SDL_RenderCopyF(renderer, player.texture, nullptr, &player.rect);

                // Apply the fade effect
                Uint8 alpha = progress * 255;
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, alpha);
                SDL_RenderFillRect(renderer, nullptr);

                SDL_RenderPresent(renderer);
            }
        } else if (gameState == LOST) {
            levelStartTime = 0;
            renderLostScreen(renderer, deathReason);
        } else {
            levelStartTime = 0;
            if (gameMode == CUSTOM) { isLastLevel = false; }
            renderWinningScreen(renderer, isLastLevel);
        }
    }

    // free up resources
    Mix_FreeMusic(soundtrack);
    for (auto sound : sounds ) {
        Mix_FreeChunk(sound);
    }
    for (auto texture : playerTextures) {
        SDL_DestroyTexture(texture);
    }
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