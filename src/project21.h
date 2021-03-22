#ifndef PROJECT21_H

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

struct input_controller
{
    bool32 IsAnalog;
    
    float32 LStartX;
    float32 LStartY;
    float32 LMinX;
    float32 LMinY;
    float32 LMaxX;
    float32 LMaxY;
    float32 LEndX;
    float32 LEndY;

    float32 RStartX;
    float32 RStartY;
    float32 RMinX;
    float32 RMinY;
    float32 RMaxX;
    float32 RMaxY;
    float32 REndX;
    float32 REndY;
    
    union
    {
        button_state Buttons[8];
        struct
        {
            button_state Up;
            button_state Down;
            button_state Left;
            button_state Right;
            button_state LeftShoulder;
            button_state RightShoulder;
            button_state Start;
            button_state Back;
        };
    };
};

struct application_input
{
    input_controller Controllers[4];
};

internal void ApplicationUpdate(application_offscreen_buffer *Buffer, 
                                application_sound_output_buffer *SoundBuffer,
                                application_input *Input);


#define PROJECT21_H
#endif