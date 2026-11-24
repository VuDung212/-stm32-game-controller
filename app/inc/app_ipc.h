#ifndef APP_IPC_H
#define APP_IPC_H

#include "FreeRTOS.h"
#include "queue.h"

#define APP_BUTTON_START (1U << 0)
#define GAME_TRACK_TOP 12U
#define GAME_PLAYER_Y 56U
#define GAME_OBSTACLE_SIZE 6U

typedef struct
{
    int8_t x;
    int8_t y;
    uint8_t buttons;
    uint32_t start_presses;
    uint32_t mode_presses;
} AppInputState;

typedef enum { GAME_READY, GAME_RUNNING, GAME_OVER, GAME_PC } GameScene;

typedef struct
{
    GameScene scene;
    uint8_t player_lane;
    uint8_t obstacle_lane;
    uint8_t obstacle_y;
    uint16_t score;
} AppGameState;

/* Each mailbox holds one complete, latest snapshot. */
extern QueueHandle_t xAppInputQueue;
extern QueueHandle_t xAppGameQueue;
extern QueueHandle_t xAppConnectionQueue;

#endif
