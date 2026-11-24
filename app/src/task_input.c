#include "task_input.h"
#include "app_ipc.h"
#include "main.h"

#define MODE_BUTTON (1U << 4)
#define GAME_BUTTONS 0x0FU
#define ADC_CENTER 2048
#define ADC_DEAD_ZONE 160

extern ADC_HandleTypeDef hadc1;
static TaskHandle_t xInputTaskHandle;

static uint8_t prvReadButtons(void)
{
    static const uint16_t pins[] = { GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_10, GPIO_PIN_11 };
    uint8_t buttons = 0U;
    uint8_t index;

    for (index = 0U; index < sizeof(pins) / sizeof(pins[0]); index++)
    {
        if (HAL_GPIO_ReadPin(GPIOB, pins[index]) == GPIO_PIN_RESET)
        {
            buttons |= (uint8_t)(1U << index);
        }
    }
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_8) == GPIO_PIN_RESET)
    {
        buttons |= MODE_BUTTON;
    }
    return buttons;
}

static HAL_StatusTypeDef prvInitJoystick(void)
{
    ADC_InjectionConfTypeDef config = {0};

    /* Injected ranks keep separate results in JDR1 and JDR2. */
    /* Configure the trigger before calibration enables the ADC. */
    config.InjectedChannel = ADC_CHANNEL_0;
    config.InjectedRank = ADC_INJECTED_RANK_1;
    config.InjectedSamplingTime = ADC_SAMPLETIME_55CYCLES_5;
    config.InjectedNbrOfConversion = 2;
    config.ExternalTrigInjecConv = ADC_INJECTED_SOFTWARE_START;
    if (HAL_ADCEx_InjectedConfigChannel(&hadc1, &config) != HAL_OK)
    {
        return HAL_ERROR;
    }

    config.InjectedChannel = ADC_CHANNEL_1;
    config.InjectedRank = ADC_INJECTED_RANK_2;
    if (HAL_ADCEx_InjectedConfigChannel(&hadc1, &config) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_ADCEx_Calibration_Start(&hadc1);
}

static int8_t prvScaleAxis(uint32_t sample)
{
    const int32_t offset = (int32_t)sample - ADC_CENTER;

    if (offset > ADC_DEAD_ZONE)
    {
        return (int8_t)((offset - ADC_DEAD_ZONE) * 127 / (2047 - ADC_DEAD_ZONE));
    }
    if (offset < -ADC_DEAD_ZONE)
    {
        return (int8_t)((offset + ADC_DEAD_ZONE) * 127 / (2048 - ADC_DEAD_ZONE));
    }
    return 0;
}

static void prvInputTask(void *pvParameters)
{
    const TickType_t period = pdMS_TO_TICKS(10);
    AppInputState input = {0};
    uint8_t previousSample = prvReadButtons();
    uint8_t stableButtons = previousSample;
    const HAL_StatusTypeDef adcReady = prvInitJoystick();
    TickType_t lastWake = xTaskGetTickCount();

    (void)pvParameters;
    for (;;)
    {
        const uint8_t sample = prvReadButtons();
        const uint8_t unchanged = (uint8_t)~(sample ^ previousSample);
        const uint8_t nextStable = (stableButtons & (uint8_t)~unchanged) | (sample & unchanged);
        const uint8_t pressed = nextStable & (uint8_t)~stableButtons;

        if (pressed & MODE_BUTTON)
        {
            input.mode_presses++;
        }
        if (pressed & APP_BUTTON_START)
        {
            input.start_presses++;
        }
        stableButtons = nextStable;
        previousSample = sample;
        input.buttons = stableButtons & GAME_BUTTONS;
        input.x = 0;
        input.y = 0;

        if (adcReady == HAL_OK && HAL_ADCEx_InjectedStart(&hadc1) == HAL_OK)
        {
            if (HAL_ADCEx_InjectedPollForConversion(&hadc1, 2) == HAL_OK)
            {
                input.x = prvScaleAxis(HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_1));
                input.y = prvScaleAxis(HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_2));
            }
            else
            {
                HAL_ADCEx_InjectedStop(&hadc1);
            }
        }

        xQueueOverwrite(xAppInputQueue, &input);
        vTaskDelayUntil(&lastWake, period);
    }
}

BaseType_t task_input_create(void)
{
    return xTaskCreate(prvInputTask, "INPUT_TASK", 192, NULL,
                       tskIDLE_PRIORITY + 2, &xInputTaskHandle);
}
