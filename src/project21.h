#ifndef PROJECT21_H

struct application_offscreen_buffer 
{
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

struct application_sound_output_buffer
{
    int32_t SamplesPerSecond;
    int32_t SampleCount;
    int16_t *Memory;
};

internal void ApplicationUpdate(application_offscreen_buffer *Buffer, int BlueOffSet, int GreenOffset);


#define PROJECT21_H
#endif