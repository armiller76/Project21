#include "project21.h"

internal void
ApplicationOutputSound(application_sound_output_buffer *SoundBuffer, int ToneHz)
{
    local_persist float32 tSine;
    int16_t ToneVol = 256;
    int32_t WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;
    
    int16_t *SampleOut = SoundBuffer->Memory;
    for(int32_t SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        float32 SineValue = sinf(tSine);
        int16_t SampleValue = (int16_t)(SineValue * ToneVol);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;
        
        tSine += (2.0f*Pi32)/(float32)WavePeriod;
        if(tSine > (2.0f*Pi32))
        {
            tSine = 0;
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

inline application_input_device *GetController(application_input *Input, uint32_t ControllerIndex)
{
    Assert(ControllerIndex < ArrayCount(Input->Controllers));
    return(&Input->Controllers[ControllerIndex]);
}

#if 0
internal application_state *ApplicationStartup(void)
{
    application_state Result = {};
    Result.ToneHz = 256;
    Result.BlueOffset = Result.GreenOffset = 0;
    return(&Result);
}
#endif

internal void
ApplicationUpdate(application_memory *Memory,
                  application_offscreen_buffer *BitmapBuffer, 
                  application_input *Input)
{
    Assert(sizeof(application_state) <= Memory->PermanentStorageSize);
    
    application_state *State = (application_state *)Memory->PermanentStorage;
    if (!Memory->ApplicationIsInitialized)
    {
#if 1 // file IO testing
        char *FileName = __FILE__;
        internal_read_file_result File = INTERNAL_PlatformReadEntireFile(FileName);
        if(File.Contents)
        {
            INTERNAL_PlatformWriteEntireFile("test.out", File.Size, File.Contents);
            INTERNAL_PlatformFreeFileMemory(File.Contents);
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

    RenderGradient(BitmapBuffer, State->BlueOffset, State->GreenOffset);
}

internal void 
ApplicationGetSoundForFrame(application_memory *Memory, application_sound_output_buffer *Buffer)
{
    application_state *State = (application_state *)Memory->PermanentStorage;
    ApplicationOutputSound(Buffer, State->ToneHz);
}
