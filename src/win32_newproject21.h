#ifndef WIN32_NEWPROJECT21_H

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
    uint32_t BufferSize;
    float32 tSine;
    int32_t LatencySampleCount;
};

#if PROJECT21_INTERNAL
struct INTERNAL_time_marker
{
    DWORD PlayCursor;
    DWORD WriteCursor;
};
#endif

#define WIN32_NEWPROJECT21_H
#endif