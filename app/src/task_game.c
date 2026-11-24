#include "task_game.h"
#include "app_ipc.h"

#define LANE_COUNT 3U
#define STEER_THRESHOLD 55
#define STEER_RELEASE 25

static TaskHandle_t xGameTaskHandle;

static void prvMovePlayer(AppGameState *game, int8_t x, uint8_t *latched)
{
    if (x > -STEER_RELEASE && x < STEER_RELEASE)
    {
        *latched = 0U;
        return;
    }
    if (*latched == 0U && x < -STEER_THRESHOLD && game->player_lane > 0U)
    {
        game->player_lane--;
        *latched = 1U;
    }
    else if (*latched == 0U && x > STEER_THRESHOLD && game->player_lane < LANE_COUNT - 1U)
    {
        game->player_lane++;
        *latched = 1U;
    }
}

static void prvAdvanceObstacle(AppGameState *game)
{
    game->obstacle_y += 2U;
    if (game->obstacle_y + GAME_OBSTACLE_SIZE > GAME_PLAYER_Y &&
        game->obstacle_y < GAME_PLAYER_Y + GAME_OBSTACLE_SIZE &&
        game->obstacle_lane == game->player_lane)
    {
        game->scene = GAME_OVER;
    }
    else if (game->obstacle_y >= GAME_PLAYER_Y + GAME_OBSTACLE_SIZE)
    {
        if (game->score < 9999U)
        {
            game->score++;
        }
        game->obstacle_y = GAME_TRACK_TOP;
        game->obstacle_lane = (uint8_t)((game->obstacle_lane + 1U) % LANE_COUNT);
    }
}

static void prvGameTask(void *pvParameters)
{
    const TickType_t period = pdMS_TO_TICKS(40);
    TickType_t lastWake = xTaskGetTickCount();
    AppGameState game = { GAME_READY, 1U, 0U, GAME_TRACK_TOP, 0U };
    AppInputState input;
    uint32_t lastStart = 0U;
    uint32_t lastMode = 0U;
    uint8_t steeringLatched = 0U;

    (void)pvParameters;
    for (;;)
    {
        if (xQueuePeek(xAppInputQueue, &input, 0) == pdPASS)
        {
            const uint8_t startPressed = input.start_presses != lastStart;
            lastStart = input.start_presses;

            if (input.mode_presses != lastMode)
            {
                lastMode = input.mode_presses;
                game.scene = (lastMode % 2U == 0U) ? GAME_READY : GAME_PC;
            }
            else if (startPressed && (game.scene == GAME_READY || game.scene == GAME_OVER))
            {
                game.scene = GAME_RUNNING;
                game.player_lane = 1U;
                game.obstacle_lane = (uint8_t)(xTaskGetTickCount() % LANE_COUNT);
                game.obstacle_y = GAME_TRACK_TOP;
                game.score = 0U;
                steeringLatched = 1U;
            }
            else if (game.scene == GAME_RUNNING)
            {
                prvMovePlayer(&game, input.x, &steeringLatched);
                prvAdvanceObstacle(&game);
            }
        }

        xQueueOverwrite(xAppGameQueue, &game);
        vTaskDelayUntil(&lastWake, period);
    }
}

BaseType_t task_game_create(void)
{
    return xTaskCreate(prvGameTask, "GAME_TASK", 192, NULL,
                       tskIDLE_PRIORITY + 1, &xGameTaskHandle);
}
