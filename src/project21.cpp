
#include "project21.h"

internal void
ApplicationOutputSound(application_state *State, application_sound_output_buffer *SoundBuffer)
{
    int16_t ToneVol = 256;
    int32_t WavePeriod = SoundBuffer->SamplesPerSecond / 768;// 768 is ToneHz
    
    int16_t *SampleOut = SoundBuffer->Memory;
    for(int32_t SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
#if 0        
        float32 SineValue = sinf(State->tSine);
        int16_t SampleValue = (int16_t)(SineValue * ToneVol);
#else
        int16_t SampleValue = 0;
#endif
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;
        
#if 0
        State->tSine += (2.0f*Pi32)/(float32)WavePeriod;
        if(State->tSine > (2.0f*Pi32))
        {
            State->tSine -= 2.0f*Pi32;
        }
#endif
    }
}

internal uint32_t
RoundFloat32ToUint32(float32 FloatingPointIn)
{
    return((uint32_t)(FloatingPointIn + 0.5f));
}

internal void
DrawRectangle(application_offscreen_buffer *Buffer, float32 MinX, float32 MinY, float32 MaxX, float32 MaxY, uint32_t Color)
{
    int32_t IntMinX = RoundFloat32ToUint32(MinX);
    int32_t IntMaxX = RoundFloat32ToUint32(MaxX);
    int32_t IntMinY = RoundFloat32ToUint32(MinY);
    int32_t IntMaxY = RoundFloat32ToUint32(MaxY);

    if(IntMinX < 0)
    {
        IntMinX = 0;
    }
    if(IntMinY < 0)
    {
        IntMinY = 0;
    }
    if(IntMaxX > Buffer->Width)
    {
        IntMaxX = Buffer->Width;
    }
    if(IntMaxY > Buffer->Height)
    {
        IntMaxY = Buffer->Height;
    }

    uint8_t *Row = (uint8_t *)Buffer->Memory + IntMinY*Buffer->Pitch + IntMinX*Buffer->BytesPerPixel;
    for(int32_t Y = IntMinY; Y < IntMaxY; ++Y)
    {
        uint32_t *Pixel = (uint32_t *)Row;
        for(int32_t X = IntMinX; X < IntMaxX; ++X)
        {
            *Pixel++ = Color;
        }
            
        Row += Buffer->Pitch;
    }
}

extern "C" APPLICATION_UPDATE(ApplicationUpdate)//(application_memory *Memory, application_offscreen_buffer *Buffer, application_input *Input)
{
    Assert(sizeof(application_state) <= Memory->PermanentStorageSize);
    
    application_state *State = (application_state *)Memory->PermanentStorage;
    if (!Memory->ApplicationIsInitialized)
    {
        Memory->ApplicationIsInitialized = true; //TODO: Is this the right place to do this?
    }

    for(uint32_t ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ++ControllerIndex)
    {
        application_input_device *Controller = GetController(Input, ControllerIndex);
        if(Controller->IsAnalog)
        {
            // Use analog movement tuning
        }
        else
        {
            // Use digital movement tuning
        }
    }

    DrawRectangle(Buffer, 0.0f, 0.0f, (float32)Buffer->Width, (float32)Buffer->Height, COLOR_BLACK);
    DrawRectangle(Buffer, 50.0f, 150.0f, 250.0f, 200.0f, COLOR_YELLOW);
}

extern "C" APPLICATION_GET_SOUND(ApplicationGetSound) // parameters: (application_memory *Memory, application_sound_output_buffer *Buffer)
{
    application_state *ApplicationState = (application_state *)Memory->PermanentStorage;
    ApplicationOutputSound(ApplicationState, Buffer);
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
            uint8_t Blue = (uint8_t)(X + XOffset);
            uint8_t Green = (uint8_t)(Y + YOffset);
            *Pixel++ = ((Green << 8) | Blue);
        }
        Row += Buffer->Pitch;
    }
}

