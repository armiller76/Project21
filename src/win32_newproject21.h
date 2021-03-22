#ifndef WIN32_NEWPROJECT21_H

#define ArrayCount(Array) (sizeof(Array)/sizeof((Array)[0]))

struct win32_offscreen_buffer
{
    BITMAPINFO BitmapInfo;
    void *Memory;
    int Width;
    int Height;
    int BytesPerPixel;
    int Pitch;

};

struct win32_window_dim
{
    int Width;
    int Height;
};

struct win32_sound_output
{
    uint32_t SamplesPerSecond;
    uint32_t SampleIndex;
    uint32_t BytesPerSample;
    size_t BufferSize;
    float32 tSine;
    int32_t LatencySampleCount;
};



#define WIN32_NEWPROJECT21_H
#endif