#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define TILE_SIZE 40

#include <SDL2/SDL.h>
#include <vector>
#include <string>
#include <filesystem>
#include <chrono>

using namespace std;

struct GameObject {
    SDL_Texture* texture;
    SDL_Rect rect;
};

filesystem::file_time_type getLastWriteTime(const string& filePath);
bool init(SDL_Window*& window, SDL_Renderer*& renderer);
SDL_Texture* loadTexture(const std::string& path, SDL_Renderer* renderer);
void close(SDL_Window* window, SDL_Renderer* renderer, std::vector<GameObject>& gameObjects);
void loadLevel(const std::string& filePath, std::vector<GameObject>& gameObjects, SDL_Renderer* renderer, SDL_Texture* brickTexture, SDL_Texture* vineTexture, SDL_Texture* marioTexture, GameObject& player);
void handleInput(const SDL_Event& e, GameObject& player, const std::vector<GameObject>& gameObjects, SDL_Texture* vineTexture, SDL_Texture* marioTextureLeft, SDL_Texture* marioTextureRight, bool& quit);
bool checkCollision(const SDL_Rect& a, const SDL_Rect& b);
bool isOnVine(const GameObject& player, const std::vector<GameObject>& gameObjects, SDL_Texture* vineTexture);
bool isOnPlatform(const GameObject& player, const std::vector<GameObject>& gameObjects, SDL_Texture* brickTexture);

#endif // FUNCTIONS_H