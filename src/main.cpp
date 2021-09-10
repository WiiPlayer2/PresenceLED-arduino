#define FASTLED_ALLOW_INTERRUPTS 0

#include <Arduino.h>
#include <FastLED.h>

#include "comm.cpp"

#define LED_PIN 5
#define NUM_LEDS 64
#define CHIPSET WS2811
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

#define BRIGHTNESS 32

CMD_UPLOAD meta_data;
uint8_t* frame_data = NULL;
uint8_t current_frame;
uint64_t lastFrameTime;

void setup()
{
    delay(3000);

    meta_data.Frames = 0;

    Serial.begin(115200);

    FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS)
        .setCorrection(TypicalSMD5050);
    FastLED.setBrightness(BRIGHTNESS);

    for(int i = 0; i < 256; i += 10)
    {
        fill_rainbow(leds, NUM_LEDS, i);
        delay(100);
        FastLED.show();
    }

    FastLED.clear(true);
}

void draw_frame_RGB(uint8_t frame)
{
    memcpy(leds, frame_data + frame * NUM_LEDS * 3, NUM_LEDS * 3);
}

void draw_frame(uint8_t frame)
{
    switch(meta_data.ColorFormat)
    {
        case COLOR_FORMAT_RGB:
            draw_frame_RGB(frame);
            break;
    }

    FastLED.show();
}

size_t get_frame_data_size(uint8_t color_format, uint8_t frame_count)
{
    switch(color_format)
    {
        case COLOR_FORMAT_RGB:
            return frame_count * 3 * NUM_LEDS;

        default:
            return 0;
    }
}

void cmd_upload(Packet packet)
{
    free(frame_data);

    memcpy(&meta_data, &packet.Data[1], sizeof(CMD_UPLOAD));
    auto frameDataSize = get_frame_data_size(meta_data.ColorFormat, meta_data.Frames);
    frame_data = (uint8_t*)malloc(frameDataSize);
    memcpy(frame_data, &packet.Data[1] + sizeof(CMD_UPLOAD), frameDataSize);

    Serial.write((uint8_t)COMM_RESP_OK);
    Serial.flush();

    current_frame = 0;
    if(meta_data.Frames == 0)
        return;

    draw_frame(0);
    lastFrameTime = millis();
}

void handlePacket(Packet packet)
{
    auto cmd = packet.Data[0];
    switch(cmd)
    {
        case COMM_CMD_UPLOAD:
            cmd_upload(packet);
            break;
    }
}

void loop()
{
    if (Serial.available() > 0)
    {
        auto packet = readPacket();
        if(packet.Size == 0)
        {
            Serial.flush();
        }
        else
        {
            handlePacket(packet);
        }
        free(packet.Data);
    }

    if(meta_data.Frames == 0 || current_frame == meta_data.Frames)
        return;

    auto current_time = millis();
    if(current_time - lastFrameTime > meta_data.Delay)
    {
        lastFrameTime = current_time;
        current_frame++;
        if(meta_data.Loop && current_frame == meta_data.Frames)
            current_frame = 0;
        
        if(current_frame == meta_data.Frames)
            return;

        draw_frame(current_frame);
    }
}
