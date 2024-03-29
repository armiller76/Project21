
#include "project21.h"

#include <Windows.h>
#include <stdio.h>
#include <malloc.h>
#include <Xinput.h>
#include <dsound.h>

#include "win32_newproject21.h"

global_variable bool32 GlobalRunning;
global_variable bool32 GlobalPause;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalAudioBuffer;
global_variable int64_t GlobalPerformanceCounterFrequency;

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) { return(ERROR_DEVICE_NOT_CONNECTED); } // stub function in case dll doesn't load
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) { return(ERROR_DEVICE_NOT_CONNECTED); } // stub function in case dll doesn't load
global_variable x_input_set_state *XInputSetState_;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

inline LARGE_INTEGER
Win32GetWallClock()
{
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return(Result);
}

inline float32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    float32 Result = (float32)(End.QuadPart - Start.QuadPart) / GlobalPerformanceCounterFrequency;
    return(Result);
}

internal void
StringCat(char *SourceA, size_t SourceACount, char *SourceB, size_t SourceBCount, char *Destination, size_t DestinationCount)
{
    
    //TODO: Bounds checking
    for(uint32_t Index = 0; Index < SourceACount; ++Index)
    {
        *Destination++ = *SourceA++;
    }
    for(uint32_t Index = 0; Index < SourceBCount; ++Index)
    {
        *Destination++ = *SourceB++;
    }
    *Destination = 0;
}

internal uint32_t
StringLength(char* String)
{
    uint32_t Result = 0;
    while(*String++)
    {
        ++Result;
    }
    return(Result);
}

internal void
INTERNAL_Win32GetEXEFilenameAndPath(win32_platform_state *State)
{
    DWORD SizeOfFilename = GetModuleFileName(0, State->EXEFilename, sizeof(State->EXEFilename));
    State->AfterLastSlash = State->EXEFilename;
    for(char *Scanner = State->EXEFilename; *Scanner; ++Scanner)
    {
        if(*Scanner == '\\')
        {
            State->AfterLastSlash = Scanner + 1;
        }
    }
}

internal void
INTERNAL_Win32AddPathToBuildDirectory(win32_platform_state *State, char *Filename, char *Destination, uint32_t DestCount)
{
    //TODO: check if pointers are valid??
    StringCat(State->EXEFilename, State->AfterLastSlash - State->EXEFilename, Filename, StringLength(Filename), Destination, DestCount);
}

INTERNAL_PLATFORM_FREE_FILE_MEMORY(INTERNAL_PlatformFreeFileMemory) // parameters: (void *Bitmapmemory)
{
    if(Bitmapmemory)
    {
        VirtualFree(Bitmapmemory, 0, MEM_RELEASE);
    }
}

INTERNAL_PLATFORM_READ_ENTIRE_FILE(INTERNAL_PlatformReadEntireFile) // parameters: (char *Filename)
{
    internal_read_file_result Result = {};

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        if(GetFileSizeEx(FileHandle, &FileSize))
        {
            //TODO: ReadFile can only read 0xFFFFFFFF bytes. For now that's fine.
            uint32_t FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
            Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            if(Result.Contents)
            {
                DWORD BytesRead;
                if(ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) && (BytesRead == FileSize32))
                {
                    Result.Size = FileSize32;
                }
                else // ReadFile failed
                {
                    //TODO: Logging
                    INTERNAL_PlatformFreeFileMemory(Thread, Result.Contents);
                    Result.Contents = 0;
                }
            }
            else // couldn't get memory
            {
                //TODO: Logging/Error handling
            }
        }
        else // couldn't get file size
        {
            //TODO: Logging/Error handling
        }
        CloseHandle(FileHandle);
    } 
    else // couldn't get file handle
    {
        //TODO: Logging/Error handling
    }

    return(Result);
}

INTERNAL_PLATFORM_WRITE_ENTIRE_FILE(INTERNAL_PlatformWriteEntireFile) // parameters: (char *Filename, uint32_t MemorySize, void *Memory)
{
    bool32 Result = false;

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten;
        if(WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
        {
            Result = (BytesWritten == MemorySize);    
        }
        else // WriteFile failed
        {
            //TODO: Logging/Error handling
        }
        CloseHandle(FileHandle);
    } 
    else // couldn't get file handle
    {
        //TODO: Loggin/Error handling
    }
    return(Result);
}

internal win32_window_dim
Win32GetWindowDimension(HWND Window)
{
    win32_window_dim Result;
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;
    return(Result);
}

inline FILETIME
Win32GetLastWriteTime(char *Filename)
{
    FILETIME Result = {};
    WIN32_FILE_ATTRIBUTE_DATA FileData;

    if(GetFileAttributesEx(Filename, GetFileExInfoStandard, &FileData))
    {
        Result = FileData.ftLastWriteTime;
    }

    return(Result);
}

internal win32_application
Win32LoadApplication(char *LibraryFilename, char *CopiedFilename)
{
    win32_application Result = {};

    Result.LastLibraryUpdateTime = Win32GetLastWriteTime(LibraryFilename);

    CopyFile(LibraryFilename, CopiedFilename, FALSE);
    
    Result.Library = LoadLibrary(CopiedFilename);
    
    if (Result.Library)
    {
        Result.Update = (application_update *)GetProcAddress(Result.Library, "ApplicationUpdate");
        Result.GetSound = (application_get_sound *)GetProcAddress(Result.Library, "ApplicationGetSound");
        Result.Valid = (Result.Update && Result.GetSound);
    }

    if(!Result.Valid)
    {
        Result.Update = 0;
        Result.GetSound = 0;
    }

    return(Result);
}

internal void
WIN32UnloadApplicationLibrary(win32_application *Application)
{
    if(Application->Library)
    {
        FreeLibrary(Application->Library);
        Application->Library = 0;
    }
    Application->Valid = false;
    Application->GetSound = 0;
    Application->Update = 0;
}

internal void
Win32InitializeXInput(void)
{
    HMODULE XInputDLL = LoadLibraryW(XINPUT_DLL_W);
    if (XInputDLL)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputDLL, "XInputGetState");
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputDLL, "XInputSetState");
    }

}

internal void
Win32InitializeBackbuffer(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    //TODO: Consider returning bool
    if(Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE); // MEM_DECOMMIT might also be appropriate?
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = 4;
    Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;

    Buffer->BitmapInfo.bmiHeader.biSize = sizeof(Buffer->BitmapInfo.bmiHeader);
    Buffer->BitmapInfo.bmiHeader.biWidth = Buffer->Width;
    Buffer->BitmapInfo.bmiHeader.biHeight = -Buffer->Height; // negative height specifies top-down DIB (top-left origin)
    Buffer->BitmapInfo.bmiHeader.biPlanes = 1;
    Buffer->BitmapInfo.bmiHeader.biBitCount = 32;
    Buffer->BitmapInfo.bmiHeader.biCompression = BI_RGB;

    SIZE_T BitmapMemorySize = Buffer->Width * Buffer->Height * Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
}

internal void
Win32InitializeDirectSound(HWND Window, uint32_t SamplesPerSecond, uint32_t BufferSize)
{
    // load the library
    HMODULE DirectSoundDLL = LoadLibraryW(L"dsound.dll");
    if (DirectSoundDLL)
    {       
        // get a DS Object
        direct_sound_create *DSCreate = (direct_sound_create *)GetProcAddress(DirectSoundDLL, "DirectSoundCreate");
        LPDIRECTSOUND DS;

        if (DSCreate && SUCCEEDED(DSCreate(0, &DS, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels*WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec*WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

            if(SUCCEEDED(DS->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                // create a primary buffer
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                DSBUFFERDESC BufferDesc = {};
                BufferDesc.dwSize = sizeof(BufferDesc);
                BufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
                
                if(SUCCEEDED(DS->CreateSoundBuffer(&BufferDesc, &PrimaryBuffer, 0)))
                {
                    if(SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
                    {
                        OutputDebugStringW(L"Primary Buffer Format Successfully Set!\n");
                    } // SetFormat
                    else
                    {
                        //TODO: Logging/error handling - unable to set format
                    }                    

                } // CreateSoundBuffer
                else
                {
                    //TODO: Logging/error handling - unable to create primary buffer
                }
            } // SetCooperativeLevel
            else
            {
                //TODO: Logging/error handling - unable to set cooperative level
            }

            DSBUFFERDESC BufferDesc = {};
            BufferDesc.dwSize = sizeof(BufferDesc);
            BufferDesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
            BufferDesc.dwBufferBytes = BufferSize;
            BufferDesc.lpwfxFormat = &WaveFormat;
            if(SUCCEEDED(DS->CreateSoundBuffer(&BufferDesc, &GlobalAudioBuffer, 0)))
            {
                OutputDebugStringW(L"Audio Buffer Created Successfully!\n");
            }
            else
            {
                // Logging/error handling - unable to create secondary buffer
            }
        } // DSCreate
        else
        {
            //TODO: Logging/error handling - bad pointer to function, or unable to create directsound object
        }
    }

}

internal void
Win32ClearSoundBuffer(win32_sound_output *SoundOutput)
{
    VOID *BufferRegion1;
    VOID *BufferRegion2;
    DWORD Region1Size;
    DWORD Region2Size;
    if(SUCCEEDED(GlobalAudioBuffer->Lock(0, SoundOutput->BufferSize,
                                             &BufferRegion1, &Region1Size, 
                                             &BufferRegion2, &Region2Size,
                                            0)))
    {
        uint8_t *DestSample = (uint8_t *)BufferRegion1;
        for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex)
        {
            *DestSample++ = 0;
        }
        DestSample = (uint8_t *)BufferRegion2;
        for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex)
        {
            *DestSample++ = 0;
        }
        
        GlobalAudioBuffer->Unlock(BufferRegion1, Region1Size, BufferRegion2, Region2Size);
    }
}

internal void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, application_sound_output_buffer *SourceBuffer)
{
    VOID *Region1;
    VOID *Region2;
    DWORD Region1Size;
    DWORD Region2Size;
    if(SUCCEEDED(GlobalAudioBuffer->Lock(ByteToLock, BytesToWrite,
                                             &Region1, &Region1Size, 
                                             &Region2, &Region2Size,
                                             0)))
    {
        DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
        DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
        int16_t *SourceSample = SourceBuffer->Memory;
        
        int16_t *DestSample = (int16_t *)Region1;
        for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->SampleIndex;
        }

        DestSample = (int16_t *)Region2;
        for(DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->SampleIndex;
        }

        GlobalAudioBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32CopyBackbufferToWindow(HDC DeviceContext, int WindowWidth, int WindowHeight, win32_offscreen_buffer *Buffer, int X, int Y, int Width, int Height)
{
    StretchDIBits(DeviceContext, 
                  0, 0, Buffer->Width, Buffer->Height, 
                  0, 0, Buffer->Width, Buffer->Height, 
                  Buffer->Memory, &Buffer->BitmapInfo, DIB_RGB_COLORS, SRCCOPY);

}

internal float32
Win32NormalizeXInputTrigger(BYTE Value, BYTE DeadzoneThreshold)
{
    float32 Result = 0.0f;
    //TODO: Consider adjusting Value upward by DZT, and 255.0f down by DZT, to smooth transition from zero?
    if(Value > DeadzoneThreshold)
    {
        Result = (float32)Value / 255.0f;
    }

    return(Result);
}

internal float32
Win32NormalizeXInputStick(SHORT Value, SHORT DeadzoneThreshold)
{
    float32 Result = 0.0f;
    //TODO: Consider adjusting Value upward by DZT, and Min/Max down by DZT, to smooth transition from zero?    
    if(Value < -DeadzoneThreshold)
    {
        Result = (float32)Value / 32768.0f;
    }
    else if(Value > DeadzoneThreshold)
    {
        Result = (float32)Value / 32767.0f;
    }

    return(Result);
}

internal void
Win32ProcessXInputButton(DWORD XInputButtonState, button_state *OldState, DWORD ButtonBit, button_state *NewState)
{
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal void
Win32ProcessKeyboardButton(button_state *NewState, bool32 IsDown)
{
    if(NewState->EndedDown != IsDown){
        NewState->EndedDown = IsDown;
        ++NewState->HalfTransitionCount;
    }
}


#if PROJECT21_AUDIODEBUG 
//might want to use this function for stuff outside audio but for now #if out to prevent compiler warning
internal void
INTERNAL_Win32DrawVertical(win32_offscreen_buffer *BackBuffer, int32_t X, int32_t Top, int32_t Bottom, uint32_t Color)
{
    if (Top <= 0) 
    { 
        Top = 0; 
    }
    
    if (Bottom > BackBuffer->Height) 
    { 
        Bottom = BackBuffer->Height;
    }
    
    if ((X >= 0) && (X < BackBuffer->Width))
    {
        uint8_t *Pixel = (uint8_t *)BackBuffer->Memory + Top*BackBuffer->Pitch + X*BackBuffer->BytesPerPixel;
        for(int32_t Y = Top; Y < Bottom; ++Y)
        {
            *(uint32_t *)Pixel = Color;
            Pixel += BackBuffer->Pitch;
        }
    }
}
#endif

#if PROJECT21_INTERNAL

#if PROJECT21_AUDIODEBUG
internal void
INTERNAL_Win32DrawAudioCursor(win32_offscreen_buffer *BackBuffer, win32_sound_output *SoundOutput, float32 ScreenToBufferRatio, 
                              uint32_t PadX, uint32_t Top, uint32_t Bottom, DWORD Value, uint32_t Color)
{
        uint32_t X = PadX + (uint32_t)(ScreenToBufferRatio * (float32)Value);
        INTERNAL_Win32DrawVertical(BackBuffer, X, Top, Bottom, Color);
}

internal void
INTERNAL_Win32DrawAudioSync(win32_offscreen_buffer *BackBuffer, uint32_t CursorCount, INTERNAL_time_marker *Cursors, uint32_t CurrentCursor, 
                            win32_sound_output *SoundOutput, float32 SecondsPerFrame)
{
    DWORD PlayColor = COLOR_WHITE;
    DWORD WriteColor = COLOR_RED;
    DWORD FlipColor = COLOR_YELLOW;
    DWORD WindowColor = COLOR_MAGENTA;

    uint32_t PadX = 16;
    uint32_t PadY = 16;
    uint32_t SegmentHeight = 64;
    float32 ScreenToBufferRatio = (float32)(BackBuffer->Width - 2*PadX) / (float32)(SoundOutput->BufferSize);

    for (uint32_t CursorIndex = 0; CursorIndex < CursorCount; ++CursorIndex)
    {
        INTERNAL_time_marker *Marker = &Cursors[CursorIndex];
        Assert(Marker->OutputPlayCursor < SoundOutput->BufferSize);
        Assert(Marker->OutputWriteCursor < SoundOutput->BufferSize);
        Assert(Marker->OutputByte < SoundOutput->BufferSize);
        Assert(Marker->FlipPlayCursor < SoundOutput->BufferSize);
        Assert(Marker->FlipWriteCursor < SoundOutput->BufferSize);
        uint32_t Top =  PadY;
        uint32_t Bottom = PadY + SegmentHeight;
        if(CursorIndex == CurrentCursor)
        {
            Top += (SegmentHeight + PadY);
            Bottom += (SegmentHeight + PadY);
            uint32_t FirstTop = Top;
            INTERNAL_Win32DrawAudioCursor(BackBuffer, SoundOutput, ScreenToBufferRatio, PadX, Top, Bottom, Marker->OutputPlayCursor, PlayColor);
            INTERNAL_Win32DrawAudioCursor(BackBuffer, SoundOutput, ScreenToBufferRatio, PadX, Top, Bottom, Marker->OutputWriteCursor, WriteColor);

            Top += (SegmentHeight + PadY);
            Bottom += (SegmentHeight + PadY);

            INTERNAL_Win32DrawAudioCursor(BackBuffer, SoundOutput, ScreenToBufferRatio, PadX, Top, Bottom, Marker->OutputByte, PlayColor);
            INTERNAL_Win32DrawAudioCursor(BackBuffer, SoundOutput, ScreenToBufferRatio, PadX, Top, Bottom, Marker->OutputByte + Marker->OutputByteCount, WriteColor);

            Top += (SegmentHeight + PadY);
            Bottom += (SegmentHeight + PadY);
            INTERNAL_Win32DrawAudioCursor(BackBuffer, SoundOutput, ScreenToBufferRatio, PadX, FirstTop, Bottom, Marker->ExpectedFlipCursor, FlipColor);
        }

        INTERNAL_Win32DrawAudioCursor(BackBuffer, SoundOutput, ScreenToBufferRatio, PadX, Top, Bottom, Marker->FlipPlayCursor, PlayColor);
        INTERNAL_Win32DrawAudioCursor(BackBuffer, SoundOutput, ScreenToBufferRatio, PadX, Top, Bottom, Marker->FlipWriteCursor, WriteColor);
        INTERNAL_Win32DrawAudioCursor(BackBuffer, SoundOutput, ScreenToBufferRatio, PadX, Top, Bottom, Marker->FlipPlayCursor + 480*SoundOutput->BytesPerSample, WindowColor);
    }
}
#endif // #if PROJECT21_AUDIODEBUG

internal void
Win32GetRecordedInputFilename(win32_platform_state *State, int32_t RecordedSlotIndex, bool32 InputStream, char *Destination, uint32_t DestCount)
{
    char Temp[64];
    sprintf_s(Temp, "project21_loop_%d_%s.p21", RecordedSlotIndex, InputStream ? "input" : "state");
    INTERNAL_Win32AddPathToBuildDirectory(State, Temp, Destination, DestCount);
}

internal win32_replay_buffer *
Win32GetReplayBuffer(win32_platform_state *Platform, uint32_t Index)
{
    Assert(Index > 0);
    Assert(Index < ArrayCount(Platform->ReplayBuffers));
    win32_replay_buffer *Result = &Platform->ReplayBuffers[Index];
    return(Result);
}

internal void
Win32BeginInputRecording(win32_platform_state *Platform, uint32_t RecordingIndex)
{
    win32_replay_buffer *Replay = Win32GetReplayBuffer(Platform, RecordingIndex);
    if(Replay->Memory)
    {
        Platform->InputRecordingIndex = RecordingIndex;

        char Filename[WIN32_FILENAME_MAX_COUNT];
        Win32GetRecordedInputFilename(Platform, RecordingIndex, true, Filename, sizeof(Filename));
        Platform->RecordingHandle = CreateFile(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

        CopyMemory(Replay->Memory, Platform->ApplicationMemoryBase, Platform->ApplicationTotalMemorySize);
    }
}

internal void
Win32BeginInputPlayback(win32_platform_state *Platform, uint32_t PlaybackIndex)
{
    win32_replay_buffer *Replay = Win32GetReplayBuffer(Platform, PlaybackIndex);
    if(Replay->Memory)
    {
        Platform->InputPlaybackIndex = PlaybackIndex;
        
        char Filename[WIN32_FILENAME_MAX_COUNT];
        Win32GetRecordedInputFilename(Platform, PlaybackIndex, true, Filename, sizeof(Filename));
        Platform->PlaybackHandle = CreateFile(Filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

        CopyMemory(Platform->ApplicationMemoryBase, Replay->Memory, Platform->ApplicationTotalMemorySize);
    }
}
 
internal void
Win32EndInputRecording(win32_platform_state *Platform)
{
    CloseHandle(Platform->RecordingHandle);
    Platform->InputRecordingIndex = 0;
}

internal void
Win32EndInputPlayback(win32_platform_state *Platform)
{
    CloseHandle(Platform->PlaybackHandle);
    Platform->InputPlaybackIndex = 0;
}

internal void
Win32RecordInput(win32_platform_state *Platform, application_input *Input)
{
    DWORD BytesWritten;
    WriteFile(Platform->RecordingHandle, Input, sizeof(*Input), &BytesWritten, 0);
}

internal void
Win32PlaybackInput(win32_platform_state *Platform, application_input *Input)
{
    DWORD BytesRead = 0;
    if(ReadFile(Platform->PlaybackHandle, Input, sizeof(*Input), &BytesRead, 0))
    {
        if(BytesRead == 0)
        {
            uint32_t PlayingIndex = Platform->InputPlaybackIndex;
            Win32EndInputPlayback(Platform);
            Win32BeginInputPlayback(Platform, PlayingIndex);
            ReadFile(Platform->PlaybackHandle, Input, sizeof(*Input), &BytesRead, 0);
        }
    }
}
#endif

internal void
Win32ProcessWindowsMessages(win32_platform_state *Platform, application_input_device *Keyboard)
{
    // Windows message loop
    MSG Message;    
    while(PeekMessageW(&Message, 0, 0, 0, PM_REMOVE))
    {
        if(Message.message == WM_QUIT)
        {
            GlobalRunning = false;
        }

        switch(Message.message)
        {
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                uint32_t VKCode = (uint32_t)Message.wParam;
                bool32 WasDown = ((Message.lParam & (1 << 30)) != 0);
                bool32 IsDown = ((Message.lParam & (1 << 31)) == 0);
                if(WasDown != IsDown)
                {
                    if(VKCode == 'W')
                    {
                        Win32ProcessKeyboardButton(&Keyboard->MoveUp, IsDown);
                    }
                    else if(VKCode == 'A')
                    {
                        Win32ProcessKeyboardButton(&Keyboard->MoveLeft, IsDown);
                    }
                    else if(VKCode == 'S')
                    {
                        Win32ProcessKeyboardButton(&Keyboard->MoveDown, IsDown);
                    }
                    else if(VKCode == 'D')
                    {
                        Win32ProcessKeyboardButton(&Keyboard->MoveRight, IsDown);
                    }
                    else if(VKCode == 'Q')
                    {
                        Win32ProcessKeyboardButton(&Keyboard->LeftShoulder, IsDown);
                    }
                    else if(VKCode == 'E')
                    {
                        Win32ProcessKeyboardButton(&Keyboard->RightShoulder, IsDown);
                    }
                    else if(VKCode == VK_UP)
                    {
                        Win32ProcessKeyboardButton(&Keyboard->ActionUp, IsDown);
                    }
                    else if(VKCode == VK_LEFT)
                    {
                        Win32ProcessKeyboardButton(&Keyboard->ActionLeft, IsDown);
                    }
                    else if(VKCode == VK_DOWN)
                    {
                        Win32ProcessKeyboardButton(&Keyboard->ActionDown, IsDown);
                    }
                    else if(VKCode == VK_RIGHT)
                    {
                        Win32ProcessKeyboardButton(&Keyboard->ActionRight, IsDown);
                    }
                    else if(VKCode == VK_ESCAPE)
                    {
                        Win32ProcessKeyboardButton(&Keyboard->Start, IsDown);
                    }
                    else if(VKCode == VK_SPACE)
                    {
                        Win32ProcessKeyboardButton(&Keyboard->Back, IsDown);
                    }
#if PROJECT21_INTERNAL
                    else if(VKCode == 'P')
                    {
                        if (IsDown)
                        {
                            GlobalPause = !GlobalPause;
                        }
                    }
                    else if(VKCode == 'L')
                    {
                        if(IsDown)
                        {
                            if(Platform->InputPlaybackIndex == 0)
                            {
                                if(Platform->InputRecordingIndex == 0)
                                {
                                    Win32BeginInputRecording(Platform, 1);
                                }
                                else
                                {
                                    Win32EndInputRecording(Platform);
                                    Win32BeginInputPlayback(Platform, 1);
                                }
                            }
                            else
                            {
                                Win32EndInputPlayback(Platform);
                                //Platform->InputPlaybackIndex = 0;
                            }
                        }
                    }
#endif
                }
                bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
                if((VKCode == VK_F4) && AltKeyWasDown)
                {
                    GlobalRunning = false;
                }
            } break;

            default:
            {
                TranslateMessage(&Message);
                DispatchMessageW(&Message);
            } break;
        } // switch(Message.message)
    } // while(PeekMessage)
}

internal LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
           UINT Message,
           WPARAM WParam,
           LPARAM LParam)
{

    LRESULT Result = 0;

    switch(Message)
    {
        case WM_SIZE: 
        {
        } break;

        case WM_DESTROY:
        {
            //TODO: Placeholder, see day 003 https://youtu.be/GAi_nTx1zG8
            GlobalRunning = false;
        } break;

        case WM_CLOSE:
        {
            //TODO: Placeholder, see day 003 https://youtu.be/GAi_nTx1zG8
            GlobalRunning = false;
        } break;

        case WM_ACTIVATEAPP:
        {
#if 0
            if(WParam == TRUE)
            {
                SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 255, LWA_ALPHA);
            }
            else
            {
                SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 96, LWA_ALPHA);
            }
#endif
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            Assert(!"How on earth did you get here? This should be in Win32ProcessWindowsMessages...")
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            win32_window_dim Dim = Win32GetWindowDimension(Window);
            HDC DeviceContext = BeginPaint(Window, &Paint);
            Win32CopyBackbufferToWindow(DeviceContext, Dim.Width, Dim.Height, 
                                &GlobalBackbuffer, 
                                Paint.rcPaint.left, Paint.rcPaint.top,
                                Paint.rcPaint.right - Paint.rcPaint.left, 
                                Paint.rcPaint.bottom - Paint.rcPaint.top);
            EndPaint(Window, &Paint);
        } break;

        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        }
    }
    return (Result);
}

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
{
    win32_platform_state Platform = {};
    INTERNAL_Win32GetEXEFilenameAndPath(&Platform);

    char ApplicationDLLFilename[] = "project21.dll";
    char ApplicationDLLFullPath[WIN32_FILENAME_MAX_COUNT];
    INTERNAL_Win32AddPathToBuildDirectory(&Platform, ApplicationDLLFilename, ApplicationDLLFullPath, sizeof(ApplicationDLLFullPath));

    char CopiedDLLFilename[] = "project21_.dll";
    char CopiedDLLFullPath[WIN32_FILENAME_MAX_COUNT];
    INTERNAL_Win32AddPathToBuildDirectory(&Platform, CopiedDLLFilename, CopiedDLLFullPath, sizeof(CopiedDLLFullPath));

    LARGE_INTEGER PerformanceCounterFrequency;
    QueryPerformanceFrequency(&PerformanceCounterFrequency);
    GlobalPerformanceCounterFrequency = PerformanceCounterFrequency.QuadPart;

    uint64_t LastCycleCount = __rdtsc();

    // force windows to 1ms scheduler granularity so framerate sleep is more accurate
    UINT DesiredWindowsSchedulerTiming = 1; //millisecond
    MMRESULT RequestResult = timeBeginPeriod(DesiredWindowsSchedulerTiming);
    bool32 SleepIsGranular = (RequestResult == TIMERR_NOERROR);

    Win32InitializeXInput();
    Win32InitializeBackbuffer(&GlobalBackbuffer, PROJECT21_WINDOWWIDTH, PROJECT21_WINDOWHEIGHT);

    WNDCLASSW WindowClass = {};
    WindowClass.style = CS_VREDRAW|CS_HREDRAW;                  // UINT        style;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;          // WNDPROC     lpfnWndProc;
    WindowClass.hInstance = Instance;                           // HINSTANCE   hInstance;
    //WindowClass.hIcon = ;                                     // HICON       hIcon;
    WindowClass.lpszClassName = (LPCWSTR)"Project21";           // LPCWSTR     lpszClassName;
    
    if(RegisterClassW(&WindowClass))
    {
        HWND Window = CreateWindowExW
        (
            0,//WS_EX_TOPMOST|WS_EX_LAYERED,    // DWORD dwExStyle,
            WindowClass.lpszClassName,      // LPCWSTR lpClassName,
            (LPCWSTR)"Project21",           // LPCWSTR lpWindowName,
            WS_OVERLAPPEDWINDOW|WS_VISIBLE, // DWORD dwStyle,
            CW_USEDEFAULT,                  // int X,
            CW_USEDEFAULT,                  // int Y,
            PROJECT21_WINDOWWIDTH,          // int nWidth,
            PROJECT21_WINDOWHEIGHT,         // int nHeight,
            0,                              // HWND hWndParent,
            0,                              // HMENU hMenu,
            Instance,                       // HINSTANCE hInstance,
            0);                             // LPVOID lpParam
        if(Window)
        {
#if PROJECT21_INTERNAL
            LPVOID PermanentStorageBaseAddress = (LPVOID)Terabytes((uint64_t)2);
#else   
            LPVOID PermanentStorageBaseAddress = 0;
#endif
            uint32_t MonitorRefreshRate = 60;
            {
                HDC QueryDC = GetDC(Window);
                uint32_t WindowsReportedRefreshRate = GetDeviceCaps(QueryDC, VREFRESH);
                ReleaseDC(Window, QueryDC);
                if(WindowsReportedRefreshRate > 1)
                {
                    MonitorRefreshRate = WindowsReportedRefreshRate;
                }
            }
            float32 GameUpdateHz = (MonitorRefreshRate / 2.0f);
            float32 TargetSecondsElapsedPerFrame = 1.0f / GameUpdateHz;

            win32_sound_output SoundOutput = {};
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.BytesPerSample = sizeof(int16_t)*2;
            SoundOutput.BufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;            
            //TODO: SafetyBytes sanity check - what is a good value here?
            SoundOutput.SafetyBytes = (int32_t)((((float32)SoundOutput.SamplesPerSecond * (float32)SoundOutput.BytesPerSample) / GameUpdateHz) / 2.0f);
            //TODO: move this to audio section, or leave here with memory allocs?
            int16_t *AllocatedAudioBufferMemory = (int16_t *)(VirtualAlloc(0, SoundOutput.BufferSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE));

            Win32InitializeDirectSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.BufferSize);
            Win32ClearSoundBuffer(&SoundOutput);
            GlobalAudioBuffer->Play(0, 0, DSBPLAY_LOOPING);

            application_memory ApplicationMemory = {};
            ApplicationMemory.PermanentStorageSize = Megabytes(64);
            ApplicationMemory.TransientStorageSize = Gigabytes(1);
            Platform.ApplicationTotalMemorySize = ApplicationMemory.PermanentStorageSize + ApplicationMemory.TransientStorageSize;
            Platform.ApplicationMemoryBase = VirtualAlloc(PermanentStorageBaseAddress, Platform.ApplicationTotalMemorySize, MEM_RESERVE|MEM_COMMIT/*|MEM_LARGE_PAGES*/, PAGE_READWRITE);
            ApplicationMemory.PermanentStorage = Platform.ApplicationMemoryBase;
            ApplicationMemory.TransientStorage = ((uint8_t *)(ApplicationMemory.PermanentStorage) + ApplicationMemory.PermanentStorageSize); 
#if PROJECT21_INTERNAL
            ApplicationMemory.INTERNAL_PlatformFreeFileMemory = INTERNAL_PlatformFreeFileMemory;
            ApplicationMemory.INTERNAL_PlatformReadEntireFile = INTERNAL_PlatformReadEntireFile;
            ApplicationMemory.INTERNAL_PlatformWriteEntireFile = INTERNAL_PlatformWriteEntireFile;
#endif

            for(uint32_t ReplayIndex = 0; ReplayIndex < ArrayCount(Platform.ReplayBuffers); ++ReplayIndex)
            {
                win32_replay_buffer *Replay = &Platform.ReplayBuffers[ReplayIndex];
                
                Win32GetRecordedInputFilename(&Platform, ReplayIndex, false, Replay->ReplayFileName, sizeof(Replay->ReplayFileName)); 
                Replay->FileHandle = CreateFile(Replay->ReplayFileName, GENERIC_WRITE|GENERIC_READ, 0, 0, CREATE_ALWAYS, 0, 0);
                
                LARGE_INTEGER FileSize;
                FileSize.QuadPart = Platform.ApplicationTotalMemorySize;
                Replay->MemoryMapHandle = CreateFileMapping(Replay->FileHandle, 0, PAGE_READWRITE, FileSize.HighPart, FileSize.LowPart, 0);
                
                Replay->Memory = MapViewOfFile(Replay->MemoryMapHandle, FILE_MAP_ALL_ACCESS, 0, 0, Platform.ApplicationTotalMemorySize);
                if(Replay->Memory)
                {
                    //should now have a backing store for the 1GB of gamestate we need to insta-write upon 
                    //invocation of loop recording function
                }
                else
                {
                    //TODO: Logging/error handling
                }
            }


            if (AllocatedAudioBufferMemory && ApplicationMemory.PermanentStorage && ApplicationMemory.TransientStorage)
            {
                GlobalRunning = true;
                LARGE_INTEGER LastPerformanceCounter = Win32GetWallClock();
                LARGE_INTEGER FrameFlipWallClock = Win32GetWallClock();

                application_input ApplicationInput[2] = {}; 
                application_input *InputThisFrame = &ApplicationInput[0];
                application_input *InputLastFrame = &ApplicationInput[1];
                InputThisFrame->SecondsToConsumePerUpdate = TargetSecondsElapsedPerFrame;

                DWORD AudioLatencyBytes = 0;
                float32 AudioLatencySeconds = 0.0f;
                bool32 SoundIsValid = false;

#if PROJECT21_INTERNAL
                uint32_t DEBUGAudioCursorIndex = 0;
                INTERNAL_time_marker DEBUGAudioCursors[32] = {0};
#endif
                win32_application Application = Win32LoadApplication(ApplicationDLLFilename, CopiedDLLFilename);

                while(GlobalRunning)
                {
                    FILETIME LatestDLLWriteTime = Win32GetLastWriteTime(ApplicationDLLFilename);
                    if(CompareFileTime(&LatestDLLWriteTime, &Application.LastLibraryUpdateTime) != 0)
                    {
                        WIN32UnloadApplicationLibrary(&Application);
                        Application = Win32LoadApplication(ApplicationDLLFilename, CopiedDLLFilename);
                    }

                    application_input_device *KeyboardThisFrame = GetController(InputThisFrame, 0);
                    application_input_device *KeyboardLastFrame = GetController(InputLastFrame, 0);
                    application_input_device ZeroedInputDevice = {};
                    *KeyboardThisFrame = ZeroedInputDevice;
                    KeyboardThisFrame->IsConnected = true;
                    for(uint32_t ButtonIndex = 0;
                                 ButtonIndex < ArrayCount(KeyboardThisFrame->Buttons);
                                 ++ButtonIndex)
                    {   // EndedDown needs to persist between frames
                        KeyboardThisFrame->Buttons[ButtonIndex].EndedDown = KeyboardLastFrame->Buttons[ButtonIndex].EndedDown;
                    }

                    Win32ProcessWindowsMessages(&Platform, KeyboardThisFrame);
                    
#if PROJECT21_INTERNAL
                    if (!GlobalPause)
                    {
#endif
                        POINT MouseP;
                        GetCursorPos(&MouseP);
                        ScreenToClient(Window, &MouseP);
                        InputThisFrame->MouseX = MouseP.x;
                        InputThisFrame->MouseY = MouseP.y;
                        InputThisFrame->MouseZ = 0; //TODO: Mousewheel
                        Win32ProcessKeyboardButton(&InputThisFrame->MouseButtons[0], GetKeyState(VK_LBUTTON) & (1 << 15));
                        Win32ProcessKeyboardButton(&InputThisFrame->MouseButtons[1], GetKeyState(VK_RBUTTON) & (1 << 15));
                        Win32ProcessKeyboardButton(&InputThisFrame->MouseButtons[2], GetKeyState(VK_MBUTTON) & (1 << 15));
                        Win32ProcessKeyboardButton(&InputThisFrame->MouseButtons[3], GetKeyState(VK_XBUTTON1) & (1 << 15));
                        Win32ProcessKeyboardButton(&InputThisFrame->MouseButtons[4], GetKeyState(VK_XBUTTON2) & (1 << 15));

                        // Controller/Keyboard loop
                        //TODO: Poll more frequently?
                        uint32_t ApplicationMaxControllerCount = XUSER_MAX_COUNT;
                        if(ApplicationMaxControllerCount > (ArrayCount(InputThisFrame->Controllers) - 1))
                        {
                            ApplicationMaxControllerCount = (ArrayCount(InputThisFrame->Controllers) - 1);
                        }

                        for (DWORD XInputControllerIndex = 0;
                            XInputControllerIndex < ApplicationMaxControllerCount;
                            ++XInputControllerIndex)
                        {   
                            DWORD ApplicationControllerIndex = 1 + XInputControllerIndex;
                            application_input_device *OldControllerState = GetController(InputLastFrame, ApplicationControllerIndex);
                            application_input_device *NewControllerState = GetController(InputThisFrame, ApplicationControllerIndex);
                            
                            XINPUT_STATE ControllerState;
                            DWORD XInputGetStateResult = XInputGetState(XInputControllerIndex, &ControllerState);
                            if(XInputGetStateResult == ERROR_SUCCESS)
                            {
                                XINPUT_GAMEPAD *GP = &ControllerState.Gamepad;
                                NewControllerState->IsConnected = true;
                                NewControllerState->IsAnalog = OldControllerState->IsAnalog;

                                if((NewControllerState->LStickAverageX != 0.0f) || (NewControllerState->LStickAverageY != 0.0f))
                                {
                                    NewControllerState->IsAnalog = true;
                                }
                                
                                // Normalize analog inputs
                                NewControllerState->LStickAverageX  = Win32NormalizeXInputStick(GP->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                NewControllerState->LStickAverageY  = Win32NormalizeXInputStick(GP->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                NewControllerState->RStickAverageX  = Win32NormalizeXInputStick(GP->sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
                                NewControllerState->RStickAverageY  = Win32NormalizeXInputStick(GP->sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
                                NewControllerState->LTriggerAverage = Win32NormalizeXInputTrigger(GP->bLeftTrigger,  XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
                                NewControllerState->RTriggerAverage = Win32NormalizeXInputTrigger(GP->bRightTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD);

                                // Overwrite left stick values if Dpad is used
                                if(GP->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                                {
                                    NewControllerState->LStickAverageX = -1.0f;
                                    NewControllerState->IsAnalog = false;
                                }
                                
                                if(GP->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                                {
                                    NewControllerState->LStickAverageX = 1.0f;
                                    NewControllerState->IsAnalog = false;
                                }
                                
                                if(GP->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                                {
                                    NewControllerState->LStickAverageY = 1.0f;
                                    NewControllerState->IsAnalog = false;
                                }

                                if(GP->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                                {
                                    NewControllerState->LStickAverageY = -1.0f;
                                    NewControllerState->IsAnalog = false;
                                }

                                // Convert analog inputs to digital for left stick and triggers
                                float32 AnalogToDigitalThreshold = 0.5f;
                                Win32ProcessXInputButton((NewControllerState->LStickAverageX < -AnalogToDigitalThreshold) ? 1 : 0,
                                                        &OldControllerState->MoveLeft, 1, &NewControllerState->MoveLeft);
                                Win32ProcessXInputButton((NewControllerState->LStickAverageX >  AnalogToDigitalThreshold) ? 1 : 0,
                                                        &OldControllerState->MoveRight, 1, &NewControllerState->MoveRight);
                                Win32ProcessXInputButton((NewControllerState->LStickAverageY < -AnalogToDigitalThreshold) ? 1 : 0,
                                                        &OldControllerState->MoveDown, 1, &NewControllerState->MoveDown);
                                Win32ProcessXInputButton((NewControllerState->LStickAverageY >  AnalogToDigitalThreshold) ? 1 : 0,
                                                        &OldControllerState->MoveUp, 1, &NewControllerState->MoveUp);
                                Win32ProcessXInputButton((NewControllerState->LTriggerAverage > AnalogToDigitalThreshold) ? 1 : 0,
                                                        &OldControllerState->LeftTrigger, 1, &NewControllerState->LeftTrigger);
                                Win32ProcessXInputButton((NewControllerState->RTriggerAverage > AnalogToDigitalThreshold) ? 1 : 0,
                                                        &OldControllerState->RightTrigger, 1, &NewControllerState->RightTrigger);

                                // Process digital buttons
                                Win32ProcessXInputButton(GP->wButtons, &OldControllerState->Start,         XINPUT_GAMEPAD_START,          &NewControllerState->Start);
                                Win32ProcessXInputButton(GP->wButtons, &OldControllerState->Back,          XINPUT_GAMEPAD_BACK,           &NewControllerState->Back);
                                Win32ProcessXInputButton(GP->wButtons, &OldControllerState->LeftShoulder,  XINPUT_GAMEPAD_LEFT_SHOULDER,  &NewControllerState->LeftShoulder);
                                Win32ProcessXInputButton(GP->wButtons, &OldControllerState->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewControllerState->RightShoulder);
                                Win32ProcessXInputButton(GP->wButtons, &OldControllerState->ActionDown,    XINPUT_GAMEPAD_A,              &NewControllerState->ActionDown);
                                Win32ProcessXInputButton(GP->wButtons, &OldControllerState->ActionRight,   XINPUT_GAMEPAD_B,              &NewControllerState->ActionRight);
                                Win32ProcessXInputButton(GP->wButtons, &OldControllerState->ActionLeft,    XINPUT_GAMEPAD_X,              &NewControllerState->ActionLeft);
                                Win32ProcessXInputButton(GP->wButtons, &OldControllerState->ActionUp,      XINPUT_GAMEPAD_Y,              &NewControllerState->ActionUp);
                            }
                            else
                            {
                                //TODO: Logging/UI? - Controller not available / unplugged / error
                                NewControllerState->IsConnected = false;
                            }
                        } // Controller processing

                        application_offscreen_buffer ApplicationOffscreenBuffer = {};
                        ApplicationOffscreenBuffer.Memory = GlobalBackbuffer.Memory;
                        ApplicationOffscreenBuffer.Width = GlobalBackbuffer.Width;
                        ApplicationOffscreenBuffer.Height = GlobalBackbuffer.Height;
                        ApplicationOffscreenBuffer.Pitch = GlobalBackbuffer.Pitch;
                        ApplicationOffscreenBuffer.BytesPerPixel = GlobalBackbuffer.BytesPerPixel;

                        thread_context Thread = {};                        
#if PROJECT21_INTERNAL
                        if(Platform.InputRecordingIndex)
                        {
                            Win32RecordInput(&Platform, InputThisFrame);
                        }

                        if(Platform.InputPlaybackIndex)
                        {
                            Win32PlaybackInput(&Platform, InputThisFrame);
                        }
#endif                        
                        if(Application.Update)
                        {
                            Application.Update(&Thread, &ApplicationMemory, &ApplicationOffscreenBuffer, InputThisFrame);
                        }

                        LARGE_INTEGER AudioWallClock = Win32GetWallClock();
                        DWORD PlayCursor;
                        DWORD WriteCursor;
                        float32 SecondsElapsedSinceFrameFlip = Win32GetSecondsElapsed(FrameFlipWallClock, AudioWallClock);

                        if(GlobalAudioBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                        {
                            if(!SoundIsValid)
                            {
                                SoundOutput.SampleIndex = WriteCursor - SoundOutput.BytesPerSample;
                                SoundIsValid = true;
                            }

                            DWORD ByteToLock = ((SoundOutput.SampleIndex*SoundOutput.BytesPerSample) % SoundOutput.BufferSize);
                            DWORD ExpectedAudioBytesPerFrame =  (DWORD)(((float32)SoundOutput.SamplesPerSecond * (float32)SoundOutput.BytesPerSample) / GameUpdateHz);
                            float32 SecondsUntilFrameFlip = TargetSecondsElapsedPerFrame - SecondsElapsedSinceFrameFlip;
                            DWORD ExpectedBytesUntilFrameFlip = (DWORD)((SecondsUntilFrameFlip/TargetSecondsElapsedPerFrame) * (float32)ExpectedAudioBytesPerFrame);
                            DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedBytesUntilFrameFlip;
                            DWORD SafeWriteCursor = WriteCursor;

                            if(SafeWriteCursor < PlayCursor)
                            {
                                SafeWriteCursor += SoundOutput.BufferSize;
                            }
                            Assert(SafeWriteCursor >= PlayCursor);
                            SafeWriteCursor += SoundOutput.SafetyBytes;
                            bool32 AudioCardIsSlow = (SafeWriteCursor >= ExpectedFrameBoundaryByte);
                            
                            DWORD TargetCursor = 0;
                            if(AudioCardIsSlow)
                            {
                                TargetCursor = WriteCursor + ExpectedAudioBytesPerFrame + SoundOutput.SafetyBytes;
                            }
                            else
                            {
                                TargetCursor = ExpectedFrameBoundaryByte + ExpectedAudioBytesPerFrame;
                            }
                            TargetCursor %= SoundOutput.BufferSize;

                            DWORD BytesToWrite = 0;
                            if (ByteToLock > TargetCursor)
                            {
                                BytesToWrite = SoundOutput.BufferSize - ByteToLock;
                                BytesToWrite += TargetCursor;
                            }
                            else
                            {
                                BytesToWrite = TargetCursor - ByteToLock;
                            }
                            
                            application_sound_output_buffer ApplicationSoundBuffer = {};
                            ApplicationSoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                            ApplicationSoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                            ApplicationSoundBuffer.Memory = AllocatedAudioBufferMemory;
                            if(Application.GetSound)
                            {
                                Application.GetSound(&Thread, &ApplicationMemory, &ApplicationSoundBuffer);
                            }

    #if PROJECT21_INTERNAL
                            INTERNAL_time_marker *Marker = &DEBUGAudioCursors[DEBUGAudioCursorIndex];
                            Marker->OutputPlayCursor = PlayCursor;
                            Marker->OutputWriteCursor = WriteCursor;
                            Marker->OutputByte = ByteToLock;
                            Marker->OutputByteCount = BytesToWrite;
                            Marker->ExpectedFlipCursor = ExpectedFrameBoundaryByte;

                            GlobalAudioBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);
                            DWORD WriteCursor_ = WriteCursor;
                            if(WriteCursor_ < PlayCursor)
                            {
                                WriteCursor_ += SoundOutput.BufferSize;
                            }
                            AudioLatencyBytes = WriteCursor_ - PlayCursor;
                            AudioLatencySeconds = ((float32)(AudioLatencyBytes / SoundOutput.BytesPerSample) / (float32)SoundOutput.SamplesPerSecond);
    #endif
                            Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &ApplicationSoundBuffer);
                        }
                        else
                        {
                            SoundIsValid = false;
                        }

                        LARGE_INTEGER WorkCompleteCounter = Win32GetWallClock();
                        float32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastPerformanceCounter, WorkCompleteCounter);
                        float32 SecondsElapsedForFrame = WorkSecondsElapsed;
                        if(SecondsElapsedForFrame < TargetSecondsElapsedPerFrame)
                        {
                            if(SleepIsGranular)
                            {
                                DWORD SleepyTime = (DWORD)(1000.0f * (TargetSecondsElapsedPerFrame - SecondsElapsedForFrame));
                                if (SleepyTime > 0)
                                {
                                    Sleep(SleepyTime);
                                }
                            }
                        
                            if(Win32GetSecondsElapsed(LastPerformanceCounter, Win32GetWallClock()) < TargetSecondsElapsedPerFrame)
                            {
                                //TODO: MISSED SLEEP!
                                //TODO: Loggin/error handling
                            }

                            while(SecondsElapsedForFrame < TargetSecondsElapsedPerFrame)
                            {
                                SecondsElapsedForFrame = Win32GetSecondsElapsed(LastPerformanceCounter, Win32GetWallClock());
                            }
                        }               
                        else
                        {
                            // TODO: MISSED FRAME RATE!
                            // TODO: Logging/Error handling
                        }

                        LARGE_INTEGER EndPerformanceCounter = Win32GetWallClock();
                        float64 MSPerFrame = 1000.0f*Win32GetSecondsElapsed(LastPerformanceCounter, EndPerformanceCounter);
                        LastPerformanceCounter = EndPerformanceCounter;

                        win32_window_dim Dim = Win32GetWindowDimension(Window);
                        HDC DeviceContext = GetDC(Window);
                        Win32CopyBackbufferToWindow(DeviceContext, Dim.Width, Dim.Height, &GlobalBackbuffer, 0, 0, Dim.Width, Dim.Height);
                        ReleaseDC(Window, DeviceContext);
                        FrameFlipWallClock = Win32GetWallClock();
    #if PROJECT21_INTERNAL                    
                        {
                            if(GlobalAudioBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                            {
                                Assert(DEBUGAudioCursorIndex < ArrayCount(DEBUGAudioCursors));
                                INTERNAL_time_marker *Marker = &DEBUGAudioCursors[DEBUGAudioCursorIndex];
                                Marker->FlipPlayCursor = PlayCursor;
                                Marker->FlipWriteCursor = WriteCursor;
                            }
                        }
    #endif

                        //TODO: Implement swap (and maybe min/max/other useful macros)
                        //TODO: Do these need to be cleared to zero? or something else?
                        application_input *Temp = InputThisFrame;
                        InputThisFrame = InputLastFrame;
                        InputLastFrame = Temp;

                        uint64_t EndCycleCount = __rdtsc();
                        uint64_t CyclesElapsed = EndCycleCount - LastCycleCount;
                        LastCycleCount = EndCycleCount;

                        float64 FPS = 0.0f;
                        float64 MCPF = ((float64)CyclesElapsed / (1000.0f * 1000.0f));
                        char DebugBuffer[256];
                        sprintf_s(DebugBuffer, "%.02fms/f,  %.02ff/s,  %.02fmc/f\n", MSPerFrame, FPS, MCPF);
                        OutputDebugStringA(DebugBuffer);
    #if PROJECT21_INTERNAL
                        ++DEBUGAudioCursorIndex;
                        if(DEBUGAudioCursorIndex == ArrayCount(DEBUGAudioCursors))
                        {
                            DEBUGAudioCursorIndex = 0; 
                        }
                    } // #if PROJECT21_INTERNAL[if(!GlobalPause)] THIS BRACE IS IN THE CORRECT PLACE, INSIDE THE #endif
    #endif

                } // while(GlobalRunning)
            }
            else // Either audio buffer pointer or one of the memory pointers is borked
            {
                //TODO: Logging/Error handling
            }
        } 
        else // Couldn't get a window
        {
            //TODO: Logging/Error handling
        }
    }
    else // Unable to register window class
    {
        // TODO: Logging/Error handling
    }
    
    return EXIT_SUCCESS;
}
