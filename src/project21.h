#ifndef PROJECT21_H

/*
    PROJECT21_INTERNAL:
        0 - public release
        1 - internal build
    
    PROJECT21_SLOW:
        0 - no slow code!
        1 - slow code okay/expected
*/

/*********************/
/*     Utilities     */
/*********************/

//IMPORTANT take this out ASAP
#define PROJECT21_INTERNAL 1 // for now, so VSC stops being a bitch


#if PROJECT21_SLOW
#define Assert(Expression) if(!(Expression)) { *(int *)0 = 0; }
#else
#define Assert(Expression)
#endif

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

#define ArrayCount(Array) (sizeof(Array)/sizeof((Array)[0]))

inline uint32_t
SafeTruncateUInt64(uint64_t Value)
{
    Assert(Value < 0xFFFFFFFF);
    return((uint32_t)Value);
}

/**************************************************************/
/*Services that the platform layer provides to the application*/
/**************************************************************/

#if PROJECT21_INTERNAL
struct internal_read_file_result
{
    uint32_t Size;
    void *Contents;
};
internal internal_read_file_result INTERNAL_PlatformReadEntireFile(char *Filename);
internal void INTERNAL_PlatformFreeFileMemory(void *Bitmapmemory);
internal bool32 INTERNAL_PlatformWriteEntireFile(char *Filename, uint32_t MemorySize, void *Memory);
#endif


/**************************************************************/
/*Services that the application provides to the platform layer*/
/**************************************************************/

struct application_offscreen_buffer 
{
    void *Memory;
    int32_t Width;
    int32_t Height;
    int32_t Pitch;
};

struct application_sound_output_buffer
{
    int32_t SamplesPerSecond;
    int32_t SampleCount;
    int16_t *Memory;
};

struct button_state
{
    int32_t HalfTransitionCount;
    bool32 EndedDown;
};

struct application_input_device
{
    bool32 IsAnalog;
    bool32 IsConnected;
    float32 LStickAverageX;
    float32 LStickAverageY;
    float32 RStickAverageX;
    float32 RStickAverageY;
    float32 LTriggerAverage;
    float32 RTriggerAverage;
    union
    {
        button_state Buttons[14];
        struct
        {
            button_state MoveUp;
            button_state MoveDown;
            button_state MoveLeft;
            button_state MoveRight;
            button_state ActionUp;
            button_state ActionDown;
            button_state ActionLeft;
            button_state ActionRight;
            button_state LeftShoulder;
            button_state RightShoulder;
            button_state LeftTrigger;
            button_state RightTrigger;
            button_state Start;
            button_state Back;
        };
    };
};

struct application_clocks
{
    //TODO: add clocks to application_input
    float32 SecondsElapsed;
};

struct application_input
{
    //TODO: add clocks here?
    application_input_device Controllers[5]; // Controller 0 = Keyboard, 1-4 = other XInput devices
};

struct application_memory
{
    bool32 ApplicationIsInitialized;
    uint64_t PermanentStorageSize;
    uint64_t TransientStorageSize;
    
    //IMPORTANT: MUST BE CLEARED TO ZERO AT STARTUP!
    void *PermanentStorage;

    //IMPORTANT: MUST BE CLEARED TO ZERO AT STARTUP!
    void *TransientStorage;
};


internal void ApplicationUpdate(application_memory *Memory, application_offscreen_buffer *Buffer, application_input *Input);
internal void ApplicationGetSoundForFrame(application_memory *Memory, application_sound_output_buffer *Buffer);

// this does not need to be visible to Win32....
struct application_state
{
    int32_t ToneHz;
    int32_t BlueOffset;
    int32_t GreenOffset;
};

#define PROJECT21_H
#endif