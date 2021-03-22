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

internal application_state *ApplicationStartup(void)
{
    application_state Result = {};
    Result.ToneHz = 256;
    Result.BlueOffset = Result.GreenOffset = 0;
    return(&Result);
}

internal void
ApplicationUpdate(application_memory *Memory,
                  application_offscreen_buffer *BitmapBuffer, 
                  application_sound_output_buffer *SoundBuffer,
                  application_input *Input)
{
    Assert(sizeof(application_state) <= Memory->PermanentStorageSize);
    
    application_state *State = (application_state *)Memory->PermanentStorage;
    if (!Memory->ApplicationIsInitialized)
    {
        State->ToneHz = 256;

        Memory->ApplicationIsInitialized = true; //TODO: Is this the right place to do this?
    }

    input_controller *Input0 = &Input->Controllers[0];
    if(Input0->IsAnalog)
    {
        // Use analog movement tuning
        State->ToneHz = 256 + (int32_t)(128.0f*(Input0->LEndY));
        State->BlueOffset -= (int32_t)4.0f*(Input0->LEndX);
    }
    else
    {
        // Use digital movement tuning
    }

    if(Input0->Down.EndedDown)
    {
        State->GreenOffset += 1;
    }

    ApplicationOutputSound(SoundBuffer, State->ToneHz);
    RenderGradient(BitmapBuffer, State->BlueOffset, State->GreenOffset);
}
