#include "task_display.h"
#include "app_ipc.h"
#include "main.h"
#include <string.h>

#define OLED_WIDTH 128U
#define OLED_HEIGHT 64U
#define OLED_PAGES (OLED_HEIGHT / 8U)

extern I2C_HandleTypeDef hi2c1;
static TaskHandle_t xDisplayTaskHandle;
static uint8_t frame[OLED_PAGES][OLED_WIDTH + 1U];

/* 5x7 font for the screens used by this project. */
static const uint8_t font[][5] =
{
    ['0'-' '] = {0x3E,0x51,0x49,0x45,0x3E},
    ['1'-' '] = {0x00,0x42,0x7F,0x40,0x00},
    ['2'-' '] = {0x42,0x61,0x51,0x49,0x46},
    ['3'-' '] = {0x21,0x41,0x45,0x4B,0x31},
    ['4'-' '] = {0x18,0x14,0x12,0x7F,0x10},
    ['5'-' '] = {0x27,0x45,0x45,0x45,0x39},
    ['6'-' '] = {0x3C,0x4A,0x49,0x49,0x30},
    ['7'-' '] = {0x01,0x71,0x09,0x05,0x03},
    ['8'-' '] = {0x36,0x49,0x49,0x49,0x36},
    ['9'-' '] = {0x06,0x49,0x49,0x29,0x1E},
    ['A'-' '] = {0x7E,0x11,0x11,0x11,0x7E},
    ['B'-' '] = {0x7F,0x49,0x49,0x49,0x36},
    ['C'-' '] = {0x3E,0x41,0x41,0x41,0x22},
    ['D'-' '] = {0x7F,0x41,0x41,0x22,0x1C},
    ['E'-' '] = {0x7F,0x49,0x49,0x49,0x41},
    ['F'-' '] = {0x7F,0x09,0x09,0x09,0x01},
    ['G'-' '] = {0x3E,0x41,0x49,0x49,0x7A},
    ['H'-' '] = {0x7F,0x08,0x08,0x08,0x7F},
    ['I'-' '] = {0x00,0x41,0x7F,0x41,0x00},
    ['L'-' '] = {0x7F,0x40,0x40,0x40,0x40},
    ['U'-' '] = {0x3F,0x40,0x40,0x40,0x3F},
    ['K'-' '] = {0x7F,0x08,0x14,0x22,0x41},
    ['M'-' '] = {0x7F,0x02,0x0C,0x02,0x7F},
    ['N'-' '] = {0x7F,0x04,0x08,0x10,0x7F},
    ['O'-' '] = {0x3E,0x41,0x41,0x41,0x3E},
    ['P'-' '] = {0x7F,0x09,0x09,0x09,0x06},
    ['R'-' '] = {0x7F,0x09,0x19,0x29,0x46},
    ['S'-' '] = {0x46,0x49,0x49,0x49,0x31},
    ['T'-' '] = {0x01,0x01,0x7F,0x01,0x01},
    ['V'-' '] = {0x1F,0x20,0x40,0x20,0x1F},
    ['W'-' '] = {0x3F,0x40,0x38,0x40,0x3F},
    ['Y'-' '] = {0x07,0x08,0x70,0x08,0x07}
};

static void prvText(uint8_t x, uint8_t page, const char *text)
{
    while (*text != '\0' && x <= OLED_WIDTH - 6U)
    {
        memcpy(&frame[page][x + 1U], font[*text - ' '], 5);
        x += 6U;
        text++;
    }
}

static void prvRect(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
    uint8_t row;
    uint8_t column;

    for (row = y; row < y + height && row < OLED_HEIGHT; row++)
    {
        for (column = x; column < x + width && column < OLED_WIDTH; column++)
        {
            frame[row / 8U][column + 1U] |= (uint8_t)(1U << (row % 8U));
        }
    }
}

static void prvDrawScore(uint8_t page, uint16_t score)
{
    char digits[5] = { '0', '0', '0', '0', '\0' };
    uint8_t index = 4U;

    while (index > 0U)
    {
        index--;
        digits[index] = (char)('0' + score % 10U);
        score /= 10U;
    }
    prvText(4, page, "SCORE");
    prvText(40, page, digits);
}

static void prvRender(const AppGameState *game, uint8_t connected)
{
    memset(frame, 0, sizeof(frame));
    switch (game->scene)
    {
        case GAME_READY:
            prvText(20, 2, "NE VAT CAN");
            prvText(20, 4, "A BAT DAU");
            prvText(20, 6, "PA8 PC");
            break;
        case GAME_OVER:
            prvText(20, 2, "GAME OVER");
            prvDrawScore(4, game->score);
            prvText(20, 6, "A CHOI LAI");
            break;
        case GAME_PC:
            prvText(20, 2, "PC BLUETOOTH");
            prvText(20, 4, connected ? "DA KET NOI" : "CHO KET NOI");
            prvText(20, 6, "PA8 OLED");
            break;
        case GAME_RUNNING:
            prvDrawScore(0, game->score);
            prvRect(42, GAME_TRACK_TOP, 1, OLED_HEIGHT - GAME_TRACK_TOP);
            prvRect(85, GAME_TRACK_TOP, 1, OLED_HEIGHT - GAME_TRACK_TOP);
            prvRect(17U + 43U * game->player_lane, GAME_PLAYER_Y, 8, GAME_OBSTACLE_SIZE);
            prvRect(17U + 43U * game->obstacle_lane, game->obstacle_y, 8, GAME_OBSTACLE_SIZE);
            break;
    }
}

static uint16_t prvInitDisplay(void)
{
    static uint8_t commands[] =
    {
        0x00, 0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
        0x8D, 0x14, 0x20, 0x02, 0xA1, 0xC8, 0xDA, 0x12,
        0x81, 0x7F, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF
    };
    uint16_t address;

    for (address = 0x3CU << 1; address <= (0x3DU << 1); address += 2U)
    {
        if (HAL_I2C_IsDeviceReady(&hi2c1, address, 1, 5) == HAL_OK &&
            HAL_I2C_Master_Transmit(&hi2c1, address, commands, sizeof(commands), 10) == HAL_OK)
        {
            return address;
        }
    }
    return 0U;
}

static HAL_StatusTypeDef prvSendPage(uint16_t address, uint8_t page)
{
    uint8_t command[] = { 0x00, (uint8_t)(0xB0U | page), 0x00, 0x10 };

    frame[page][0] = 0x40;
    if (HAL_I2C_Master_Transmit(&hi2c1, address, command, sizeof(command), 5) != HAL_OK)
    {
        return HAL_ERROR;
    }
    return HAL_I2C_Master_Transmit(&hi2c1, address, frame[page], sizeof(frame[page]), 20);
}

static void prvDisplayTask(void *pvParameters)
{
    const TickType_t period = pdMS_TO_TICKS(16);
    TickType_t lastWake = xTaskGetTickCount();
    uint16_t address = 0U;
    uint8_t page = 0U;

    (void)pvParameters;
    for (;;)
    {
        if (address == 0U)
        {
            address = prvInitDisplay();
        }
        if (address != 0U)
        {
            if (page == 0U)
            {
                AppGameState game = { GAME_READY, 1U, 0U, GAME_TRACK_TOP, 0U };
                uint8_t connected = 0U;
                xQueuePeek(xAppGameQueue, &game, 0);
                xQueuePeek(xAppConnectionQueue, &connected, 0);
                prvRender(&game, connected);
            }

            if (prvSendPage(address, page) == HAL_OK)
            {
                page = (page + 1U) % OLED_PAGES;
            }
            else
            {
                address = 0U;
                page = 0U;
            }
        }
        vTaskDelayUntil(&lastWake, period);
    }
}

BaseType_t task_display_create(void)
{
    return xTaskCreate(prvDisplayTask, "DISPLAY_TASK", 256, NULL,
                       tskIDLE_PRIORITY + 1, &xDisplayTaskHandle);
}
