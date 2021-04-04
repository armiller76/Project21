#ifndef WIN32_NEWPROJECT21_H

#define WIN32_FILENAME_MAX_COUNT MAX_PATH

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
};

struct win32_application
{
    HMODULE Library;
    FILETIME LastLibraryUpdateTime;
    
    //IMPORTANT: Can be set to 0 if pointer cannot be found - guard usage with if(n.Update)
    application_update *Update;
    //IMPORTANT: Can be set to 0 if pointer cannot be found - guard usage with if(n.Update)
    application_get_sound *GetSound;
    
    bool32 Valid;
};

struct win32_replay_buffer
{
    HANDLE FileHandle;
    HANDLE MemoryMapHandle;
    char ReplayFileName[WIN32_FILENAME_MAX_COUNT];
    void *Memory;
};

struct win32_platform_state
{
    void *ApplicationMemoryBase;
    uint64_t ApplicationTotalMemorySize;
    win32_replay_buffer ReplayBuffers[4];

    HANDLE RecordingHandle;
    uint32_t InputRecordingIndex;

    HANDLE PlaybackHandle;
    uint32_t InputPlaybackIndex;

    char EXEFilename[WIN32_FILENAME_MAX_COUNT];
    char *AfterLastSlash;
};

#if PROJECT21_INTERNAL
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