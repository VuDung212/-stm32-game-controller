#include <Arduino.h>
#include <BleGamepad.h>
#include <NimBLEService.h>
#include <atomic>

constexpr int UartRx = 16;
constexpr int UartTx = 17;
constexpr uint8_t FrameStart = 0xFF;
constexpr uint8_t Center = 127;
constexpr uint32_t InputTimeoutMs = 200;
// Leave room for the HID UUID, appearance and flags in the BLE advertisement.
constexpr char DeviceName[] = "STM32 XY Pad";
constexpr uint8_t ButtonCount = 4;
// Distinguish this layout from BleGamepad's default 8-axis product.
constexpr uint16_t GamepadProductId = 0xBBAC;

class ControllerGamepad : public BleGamepad
{
public:
    using BleGamepad::BleGamepad;

    void sendInput(uint8_t x, uint8_t y, uint8_t buttons)
    {
        NimBLECharacteristic *report = inputReport.load();
        if (report == nullptr)
        {
            return;
        }

        // Generic HID format: four button bits, four padding bits, then X/Y
        // as signed 16-bit little-endian values. BleGamepad 0.8.0's generic
        // sender copies 16 button bytes even when this report is only 5 bytes.
        const uint16_t axisX = static_cast<int16_t>(x) - Center;
        const uint16_t axisY = static_cast<int16_t>(y) - Center;
        const uint8_t data[] = {
            static_cast<uint8_t>(buttons & 0x0F),
            static_cast<uint8_t>(axisX),
            static_cast<uint8_t>(axisX >> 8),
            static_cast<uint8_t>(axisY),
            static_cast<uint8_t>(axisY >> 8)
        };
        report->setValue(data, sizeof(data));
        if (isConnected())
        {
            report->notify();
        }
    }

protected:
    void onStarted(NimBLEServer *server) override
    {
        NimBLEService *hid = server->getServiceByUUID("1812");
        NimBLECharacteristic *report = hid ? hid->getCharacteristic("2A4D") : nullptr;
        if (report == nullptr || getHidReportSize() != 5)
        {
            Serial.println("HID input report setup failed");
            return;
        }

        inputReport.store(report);
        sendInput(Center, Center, 0);
        Serial.println("HID ready: 2 axes, 4 buttons, 5 bytes");
    }

private:
    std::atomic<NimBLECharacteristic *> inputReport{nullptr};
};

static ControllerGamepad gamepad(DeviceName, "STM32 Project", 100);
static BleGamepadConfiguration gamepadConfig;

void setup()
{
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, UartRx, UartTx);

    gamepadConfig.setAutoReport(false);
    gamepadConfig.setPid(GamepadProductId);
    gamepadConfig.setButtonCount(ButtonCount);
    gamepadConfig.setHatSwitchCount(0);
    gamepadConfig.setWhichAxes(true, true, false, false, false, false, false, false);
    gamepadConfig.setAxesMin(-127);
    gamepadConfig.setAxesMax(127);
    gamepad.begin(&gamepadConfig);
    Serial.printf("BLE gamepad: %s\n", DeviceName);
}

void loop()
{
    static uint8_t payload[5];
    static size_t position = sizeof(payload);
    static uint32_t lastFrame;
    static uint32_t lastInputLog;
    static bool active;

    while (Serial2.available() > 0)
    {
        const uint8_t byte = static_cast<uint8_t>(Serial2.read());
        if (byte == FrameStart)
        {
            position = 0;
        }
        else if (position < sizeof(payload))
        {
            payload[position++] = byte;
            if (position == sizeof(payload))
            {
                const uint8_t checksum =
                    (payload[0] ^ payload[1] ^ payload[2] ^ payload[3]) & 0x7F;
                if (payload[0] <= 1 && payload[3] <= 0x0F && checksum == payload[4])
                {
                    lastFrame = millis();
                    const bool pcMode = payload[0] != 0;
                    Serial2.write(gamepad.isConnected() ? 0xC1 : 0xC0);
                    if (pcMode)
                    {
                        gamepad.sendInput(payload[1], payload[2], payload[3]);
                        if (lastFrame - lastInputLog >= 1000)
                        {
                            lastInputLog = lastFrame;
                            Serial.printf("PC input: X=%d Y=%d buttons=%02X\n",
                                          static_cast<int>(payload[1]) - Center,
                                          static_cast<int>(payload[2]) - Center,
                                          static_cast<unsigned>(payload[3]));
                        }
                    }
                    else if (active)
                    {
                        gamepad.sendInput(Center, Center, 0);
                    }
                    active = pcMode;
                }
            }
        }
    }

    if (active && millis() - lastFrame >= InputTimeoutMs)
    {
        gamepad.sendInput(Center, Center, 0);
        active = false;
    }
    delay(1);
}
