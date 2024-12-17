#include <SDL2/SDL.h>

typedef struct {
    float x, y;          // Position
    float vx, vy;        // Velocity
    bool onGround;       // Is Mario on the ground?
    bool canDoubleJump;  // Can Mario perform a double jump?
    bool climbing;       // Is Mario climbing?
} Mario;

#define GRAVITY 0.5f
#define JUMP_SPEED -10.0f
#define MOVE_SPEED 5.0f
#define CLIMB_SPEED 3.0f


void handleMarioMovement(SDL_Event *event, Mario *mario, const Uint8 *keys) {
    
    if (keys[SDL_SCANCODE_LEFT]) {
        mario->vx = -MOVE_SPEED;
    } else if (keys[SDL_SCANCODE_RIGHT]) {
        mario->vx = MOVE_SPEED;
    } else {
        mario->vx = 0;  
    }


    static Uint32 lastJumpTime = 0;
    if (event->type == SDL_KEYDOWN && event->key.keysym.scancode == SDL_SCANCODE_SPACE) {
        Uint32 currentTime = SDL_GetTicks();

        if (mario->onGround) {
            mario->vy = JUMP_SPEED;  // First jump
            mario->onGround = false;
            mario->canDoubleJump = true;
        } else if (mario->canDoubleJump && (currentTime - lastJumpTime) < 250) {
            mario->vy = JUMP_SPEED;  // Double jump
            mario->canDoubleJump = false;
        }

        lastJumpTime = currentTime;
    }


    if (keys[SDL_SCANCODE_UP] && mario->climbing) {
        mario->vy = -CLIMB_SPEED;  
    } else if (keys[SDL_SCANCODE_DOWN] && mario->climbing) {
        mario->vy = CLIMB_SPEED;  
    } else if (!mario->climbing) {
        mario->vy += GRAVITY;  
    }

    mario->x += mario->vx;
    mario->y += mario->vy;


    if (mario->y >= 500) {  
        mario->y = 500;
        mario->vy = 0;
        mario->onGround = true;
        mario->canDoubleJump = false;  
    }
}