
#include "project21.h"

internal void
ApplicationOutputSound(application_memory *Memory, application_sound_output_buffer *SoundBuffer)
{
    int16_t ToneVol = 256;
    application_state *State = (application_state *)Memory->PermanentStorage;
    float32 SineValue = State->tSine;
    int32_t WavePeriod = SoundBuffer->SamplesPerSecond;
    
    int16_t *SampleOut = SoundBuffer->Memory;
    for(int32_t SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        int16_t SampleValue = (int16_t)(SineValue * ToneVol);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;
        
        State->tSine += (2.0f*Pi32)/(float32)WavePeriod;
        if(State->tSine > (2.0f*Pi32))
        {
            State->tSine = 0;
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
        internal_read_file_result File = Memory->INTERNAL_PlatformReadEntireFile(FileName);
        if(File.Contents)
        {
            Memory->INTERNAL_PlatformWriteEntireFile("test.out", File.Size, File.Contents);
            Memory->INTERNAL_PlatformFreeFileMemory(File.Contents);
        }
#endif // file IO testing
        
        State->ToneHz = 1024;
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
            State->BlueOffset -= (int32_t)(4.0f*(Controller->LStickAverageX));
        }
        else
        {
            // Use digital movement tuning
            if(Controller->MoveRight.EndedDown)
            {
                --State->BlueOffset;
            }

            if(Controller->MoveLeft.EndedDown)
            {
                ++State->BlueOffset;
            }

            if(Controller->MoveUp.EndedDown)
            {
                ++State->GreenOffset;
            }

            if(Controller->MoveDown.EndedDown)
            {
                --State->GreenOffset;
            }
        }
    }

    RenderGradient(Buffer, State->BlueOffset, State->GreenOffset);
}

extern "C" APPLICATION_GET_SOUND(ApplicationGetSound) // parameters: (application_memory *Memory, application_sound_output_buffer *Buffer)
{
    application_state *State = (application_state *)Memory->PermanentStorage;
    ApplicationOutputSound(Memory, Buffer);
}

#include "Windows.h"
BOOL WINAPI DllMain(HINSTANCE Instance, DWORD Reason, LPVOID Reserved)
{
    return TRUE;
}