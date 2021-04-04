
#include "project21.h"

internal void
ApplicationOutputSound(application_state *State, application_sound_output_buffer *SoundBuffer)
{
    int16_t ToneVol = 256;
    int32_t WavePeriod = SoundBuffer->SamplesPerSecond / State->ToneHz;
    
    int16_t *SampleOut = SoundBuffer->Memory;
    for(int32_t SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
#if 1        
        float32 SineValue = sinf(State->tSine);
        int16_t SampleValue = (int16_t)(SineValue * ToneVol);
#else
        int16_t SampleValue = 0;
#endif
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;
        
        State->tSine += (2.0f*Pi32)/(float32)WavePeriod;
        if(State->tSine > (2.0f*Pi32))
        {
            State->tSine -= 2.0f*Pi32;
        }
    }
}

internal void
Render10x10Square(application_offscreen_buffer *BackBuffer, int32_t PlayerX, int32_t PlayerY, uint32_t Color)
{
    uint8_t *EndOfBuffer = (uint8_t *)BackBuffer->Memory + BackBuffer->Pitch*BackBuffer->Height;
    
    int32_t Top = PlayerY;
    int32_t Bottom = PlayerY + 10;
    for(int32_t X = PlayerX; X < PlayerX + 10; ++X)
    {
        uint8_t *Pixel = (uint8_t *)BackBuffer->Memory + Top*BackBuffer->Pitch + X*BackBuffer->BytesPerPixel;
        for(int32_t Y = Top; Y < Bottom; ++Y)
        {
            if((Pixel >= BackBuffer->Memory) && ((Pixel + 4) < EndOfBuffer))
            {
                *(uint32_t *)Pixel = Color;
            }
            
            Pixel += BackBuffer->Pitch;
        }
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
            uint8_t Blue = (uint8_t)(X + XOffset);
            uint8_t Green = (uint8_t)(Y + YOffset);
            *Pixel++ = ((Green << 8) | Blue);
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

#if 1 // file IO testing
        char *FileName = __FILE__;
        internal_read_file_result File = Memory->INTERNAL_PlatformReadEntireFile(Thread, FileName);
        if(File.Contents)
        {
            Memory->INTERNAL_PlatformWriteEntireFile(Thread, "test.out", File.Size, File.Contents);
            Memory->INTERNAL_PlatformFreeFileMemory(Thread, File.Contents);
        }
#endif // file IO testing
        
        State->PlayerX = 100;
        State->PlayerY = 100;
        
        State->ToneHz = 1024;
        State->tSine = 0;
        Memory->ApplicationIsInitialized = true; //TODO: Is this the right place to do this?
    }

    for(uint32_t ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex)
    {
        application_input_device *Controller = GetController(Input, ControllerIndex);
        if(Controller->IsAnalog)
        {
            // Use analog movement tuning
            State->ToneHz = 1024 + (int32_t)(128.0f*(Controller->LStickAverageY));
        }
        else
        {
            // Use digital movement tuning
        }

        State->PlayerX += (int32_t)(4.0f*Controller->LStickAverageX);
        State->PlayerY -= (int32_t)(4.0f*Controller->LStickAverageY);
        if(State->tAction > 0)
        {
             State->PlayerY += (int32_t)(5.0f*sinf(0.5f*Pi32*State->tAction));
        }
        if(Controller->ActionDown.EndedDown)
        {
            State->tAction = 4.0f;
        }
        State->tAction -= 0.033f;
    }

    RenderGradient(Buffer, State->BlueOffset, State->GreenOffset);
    Render10x10Square(Buffer, State->PlayerX, State->PlayerY, COLOR_MAGENTA);
    Render10x10Square(Buffer, Input->MouseX, Input->MouseY, COLOR_YELLOW);
}

extern "C" APPLICATION_GET_SOUND(ApplicationGetSound) // parameters: (application_memory *Memory, application_sound_output_buffer *Buffer)
{
    application_state *ApplicationState = (application_state *)Memory->PermanentStorage;
    ApplicationOutputSound(ApplicationState, Buffer);
}
