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
    uint32_t SafetyBytes;
    float32 tSine;
    int32_t LatencySampleCount;
};

struct win32_application
{
    HMODULE Library;
    FILETIME LastLibraryUpdateTime;
    application_update *Update;
    application_get_sound *GetSound;
    
    bool32 Valid;
};

#if PROJECT21_INTERNAL
struct INTERNAL_win32_platform_state
{
    void *ApplicationMemoryBase;
    uint64_t ApplicationMemorySize;

    HANDLE RecordingHandle;
    uint32_t InputRecordingIndex;

    HANDLE PlaybackHandle;
    uint32_t InputPlaybackIndex;
};

struct INTERNAL_time_marker
{
    DWORD OutputPlayCursor;
    DWORD OutputWriteCursor;
    DWORD OutputByte;
    DWORD OutputByteCount;
    
    DWORD ExpectedFlipCursor;
    DWORD FlipPlayCursor;
    DWORD FlipWriteCursor;
};
#endif

#define WIN32_NEWPROJECT21_H
#endif