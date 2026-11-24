#include "rtos_startup.h"
#include "app_ipc.h"
#include "task_input.h"
#include "task_game.h"
#include "task_display.h"
#include "task_comm.h"

QueueHandle_t xAppInputQueue;
QueueHandle_t xAppGameQueue;
QueueHandle_t xAppConnectionQueue;

void rtos_startup(void)
{
    BaseType_t result;

    xAppInputQueue = xQueueCreate(1, sizeof(AppInputState));
    xAppGameQueue = xQueueCreate(1, sizeof(AppGameState));
    xAppConnectionQueue = xQueueCreate(1, sizeof(uint8_t));
    configASSERT(xAppInputQueue && xAppGameQueue && xAppConnectionQueue);
    if (!xAppInputQueue || !xAppGameQueue || !xAppConnectionQueue)
    {
        return;
    }

    result = task_input_create();
    configASSERT(result == pdPASS);
    result = task_game_create();
    configASSERT(result == pdPASS);
    result = task_display_create();
    configASSERT(result == pdPASS);
    result = task_comm_create();
    configASSERT(result == pdPASS);
}
