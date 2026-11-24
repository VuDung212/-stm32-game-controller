#include "task_comm.h"
#include "app_ipc.h"
#include "main.h"

#define UART_SYNC 0xFFU
#define UART_STATUS_CONNECTED 0xC1U

extern UART_HandleTypeDef huart1;
static TaskHandle_t xCommTaskHandle;

static void prvCommTask(void *pvParameters)
{
    const TickType_t period = pdMS_TO_TICKS(20);
    TickType_t lastWake = xTaskGetTickCount();

    (void)pvParameters;
    for (;;)
    {
        AppGameState game;
        AppInputState input;
        uint8_t frame[6] = { UART_SYNC, 0U, 127U, 127U, 0U, 0U };
        uint8_t response;
        uint8_t connected = 0U;

        if (xQueuePeek(xAppGameQueue, &game, 0) == pdPASS &&
            game.scene == GAME_PC &&
            xQueuePeek(xAppInputQueue, &input, 0) == pdPASS &&
            (input.mode_presses % 2U) != 0U)
        {
            frame[1] = 1U;
            frame[2] = (uint8_t)(input.x + 127);
            frame[3] = (uint8_t)(input.y + 127);
            frame[4] = input.buttons;
        }
        frame[5] = (frame[1] ^ frame[2] ^ frame[3] ^ frame[4]) & 0x7FU;

        if (HAL_UART_Transmit(&huart1, frame, sizeof(frame), 3) == HAL_OK &&
            HAL_UART_Receive(&huart1, &response, 1, 8) == HAL_OK)
        {
            connected = response == UART_STATUS_CONNECTED;
        }
        xQueueOverwrite(xAppConnectionQueue, &connected);
        vTaskDelayUntil(&lastWake, period);
    }
}

BaseType_t task_comm_create(void)
{
    return xTaskCreate(prvCommTask, "COMM_TASK", 192, NULL,
                       tskIDLE_PRIORITY + 1, &xCommTaskHandle);
}
