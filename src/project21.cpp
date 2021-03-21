#include "project21.h"

internal void
ApplicationOutputSound(application_sound_output_buffer *SoundBuffer, int ToneHz)
{
    local_persist float32 tSine;
    int16_t ToneVol = 3000;
    int32_t WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;
    
    int16_t *SampleOut = SoundBuffer->Memory;
    for(int32_t SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        float32 SineValue = sinf(tSine);
        int16_t SampleValue = (int16_t)(SineValue * ToneVol);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;
        
        tSine += (2.0f*Pi32)/(float32)WavePeriod;
    }
}

internal void
RenderGradient(application_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
    // Renders a blue/green gradient pattern to a buffer. 
    uint8_t *Row = (uint8_t *)Buffer->Memory;
    for(int Y = 0;
        Y < Buffer->Height;
        ++Y)
    {
        uint32_t *Pixel = (uint32_t *)Row;
        for(int X = 0;
            X < Buffer->Width;
            ++X)
        {
            uint8_t Blue = X + XOffset;
            uint8_t Green = Y + YOffset;
            *Pixel++ = ((Green << 8) | Blue);
        }
        Row += Buffer->Pitch;
    }
}

internal void
ApplicationUpdate(application_offscreen_buffer *Buffer, int32_t BlueOffset, int32_t GreenOffset, 
                  application_sound_output_buffer *SoundBuffer, int32_t ToneHz)
{
    ApplicationOutputSound(SoundBuffer, ToneHz);
    RenderGradient(Buffer, BlueOffset, GreenOffset);
}
