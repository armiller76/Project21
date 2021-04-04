#ifndef PROJECT21_H

/*
    PROJECT21_INTERNAL:
        0 - public release
        1 - internal build
    
    PROJECT21_SLOW:
        0 - no slow code!
        1 - slow code okay/expected
*/

/*************************************/
/*     Definitions and Utilities     */
/*************************************/
//IMPORTANT take this out ASAP
#define PROJECT21_INTERNAL 1 // for now, so VSC stops being a bitch
#define PROJECT21_AUDIODEBUG 0 // set nonzero to enable drawing audio sync functions

#include <math.h> // currenly only using sinf - can I do better / as well?
#include <stdint.h>

#define local_persist static
#define global_variable static
#define internal static

#define COLOR_WHITE     0xFFFFFFFF
#define COLOR_RED       0xFFFF0000
#define COLOR_BLUE      0xFF0000FF
#define COLOR_GREEN     0xFF00FF00
#define COLOR_CYAN      0xFF00FFFF
#define COLOR_MAGENTA   0xFFFF00FF
#define COLOR_YELLOW    0xFFFFFF00
#define COLOR_BLACK     0xFF000000

#define PROJECT21_WINDOWWIDTH 1280
#define PROJECT21_WINDOWHEIGHT 720

#define Pi32 3.14159265359f

typedef float float32;
typedef double float64;
typedef int32_t bool32;

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

struct thread_context
{
    int32_t PlaceHolder_;
};


/**************************************************************/
/*Services that the platform layer provides to the application*/
/**************************************************************/

#if PROJECT21_INTERNAL
struct internal_read_file_result
{
    uint32_t Size;
    void *Contents;
};

// IMPORTANT:
// DO NOT USE outside of internal testing. NOT FOR SHIPPING!
// these are blocking calls, and do not protect against lost data!
#define INTERNAL_PLATFORM_FREE_FILE_MEMORY(name) void name(thread_context *Thread, void *Bitmapmemory)
typedef INTERNAL_PLATFORM_FREE_FILE_MEMORY(INTERNAL_platform_free_file_memory);

#define INTERNAL_PLATFORM_READ_ENTIRE_FILE(name) internal_read_file_result name(thread_context *Thread, char *Filename)
typedef INTERNAL_PLATFORM_READ_ENTIRE_FILE(INTERNAL_platform_read_entire_file);

#define INTERNAL_PLATFORM_WRITE_ENTIRE_FILE(name) bool32 name(thread_context *Thread, char *Filename, uint32_t MemorySize, void *Memory)
typedef INTERNAL_PLATFORM_WRITE_ENTIRE_FILE(INTERNAL_platform_write_entire_file);

#endif


/**************************************************************/
/*Services/data that the application provides to the platform layer*/
/**************************************************************/

struct application_offscreen_buffer 
{
    void *Memory;
    int32_t Width;
    int32_t Height;
    int32_t Pitch;
    int32_t BytesPerPixel;
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
    button_state MouseButtons[5];
    int32_t MouseX;
    int32_t MouseY;
    int32_t MouseZ;
    
    //TODO: add clocks in here?
    application_input_device Controllers[5]; // Controller 0 = Keyboard, 1-4 = other XInput devices
};
inline application_input_device *GetController(application_input *Input, uint32_t ControllerIndex)
{
    Assert(ControllerIndex < ArrayCount(Input->Controllers));
    return(&Input->Controllers[ControllerIndex]);
}

struct application_memory
{
    bool32 ApplicationIsInitialized;
    uint64_t PermanentStorageSize;
    uint64_t TransientStorageSize;
    
    //IMPORTANT: MUST BE CLEARED TO ZERO AT STARTUP!
    void *PermanentStorage;

    //IMPORTANT: MUST BE CLEARED TO ZERO AT STARTUP!
    void *TransientStorage;

    INTERNAL_platform_free_file_memory *INTERNAL_PlatformFreeFileMemory;
    INTERNAL_platform_read_entire_file *INTERNAL_PlatformReadEntireFile;
    INTERNAL_platform_write_entire_file *INTERNAL_PlatformWriteEntireFile;
};

#define APPLICATION_UPDATE(name) void name(thread_context *Thread, application_memory *Memory, application_offscreen_buffer *Buffer, application_input *Input)
typedef APPLICATION_UPDATE(application_update);

#define APPLICATION_GET_SOUND(name) void name(thread_context *Thread, application_memory *Memory, application_sound_output_buffer *Buffer)
typedef APPLICATION_GET_SOUND(application_get_sound);

// this does not need to be visible to Win32....
struct application_state
{
    int32_t ToneHz;
    float32 tSine;
    int32_t BlueOffset;
    int32_t GreenOffset;

    int32_t PlayerX;
    int32_t PlayerY;
    float32 tAction;
};

#define PROJECT21_H
#endif