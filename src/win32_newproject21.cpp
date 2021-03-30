
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
                    INTERNAL_PlatformFreeFileMemory(Result.Contents);
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

    WIN32_FIND_DATA FindData;
    HANDLE Handle = FindFirstFile(Filename, &FindData);
    if(Handle)
    {
        Result = FindData.ftLastWriteTime;
        FindClose(Handle);
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
        Result.Update = ApplicationUpdateStub;
        Result.GetSound = ApplicationGetSoundStub;
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
    Application->GetSound = ApplicationGetSoundStub;
    Application->Update = ApplicationUpdateStub;
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
    Assert(NewState->EndedDown != IsDown);
    NewState->EndedDown = IsDown;
    ++NewState->HalfTransitionCount;
}

#if PROJECT21_INTERNAL
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
#endif

internal void
Win32ProcessWindowsMessages(application_input_device *Keyboard)
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

internal void
Concatenate(char *SourceA, size_t SourceACount, char *SourceB, size_t SourceBCount, char *Destination, size_t DestinationCount)
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

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
{
    char MyExecutableFilename[MAX_PATH];
    DWORD SizeOfMyExecutableFilename = GetModuleFileName(0, MyExecutableFilename, sizeof(MyExecutableFilename));
    char *LastSlash = MyExecutableFilename;
    for(char *Scanner = MyExecutableFilename; *Scanner; ++Scanner)
    {
        if(*Scanner == '\\')
        {
            LastSlash = Scanner + 1;
        }
    }

    char ApplicationDLLFilename[] = "project21.dll";
    char ApplicationDLLFullPath[MAX_PATH];
    Concatenate(MyExecutableFilename, LastSlash - MyExecutableFilename, ApplicationDLLFilename, sizeof(ApplicationDLLFilename) - 1, ApplicationDLLFullPath, sizeof(ApplicationDLLFullPath));
    char CopiedDLLFilename[] = "project21_.dll";
    char CopiedDLLFullPath[MAX_PATH];
    Concatenate(MyExecutableFilename, LastSlash - MyExecutableFilename, CopiedDLLFilename, sizeof(CopiedDLLFilename) - 1, CopiedDLLFullPath, sizeof(CopiedDLLFullPath));

    LARGE_INTEGER PerformanceCounterFrequency;
    QueryPerformanceFrequency(&PerformanceCounterFrequency);
    GlobalPerformanceCounterFrequency = PerformanceCounterFrequency.QuadPart;

    uint64_t LastCycleCount = __rdtsc();

    // force windows to 1ms scheduler granularity so framerate sleep is more accurate
    UINT DesiredWindowsSchedulerTiming = 1; //millisecond
    MMRESULT RequestResult = timeBeginPeriod(DesiredWindowsSchedulerTiming);
    bool32 SleepIsGranular = (RequestResult == TIMERR_NOERROR);

    Win32InitializeXInput();
    Win32InitializeBackbuffer(&GlobalBackbuffer, 1280, 720);

    WNDCLASSW WindowClass = {};
    WindowClass.style = CS_VREDRAW|CS_HREDRAW;                  // UINT        style;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;          // WNDPROC     lpfnWndProc;
    WindowClass.hInstance = Instance;                           // HINSTANCE   hInstance;
    //WindowClass.hIcon = ;                                     // HICON       hIcon;
    WindowClass.lpszClassName = (LPCWSTR)"Project21";           // LPCWSTR     lpszClassName;

    //TODO: find a reliable way to get this from windows...
#define DEFINEDMonitorRefreshRate 60
#define DEFINEDGameUpdateHz (DEFINEDMonitorRefreshRate / 2)
    float32 TargetSecondsElapsedPerFrame = 1.0f / (float32)DEFINEDGameUpdateHz;
    
    if(RegisterClassW(&WindowClass))
    {
        HWND Window = CreateWindowExW
        (
            0,                              // DWORD dwExStyle,
            WindowClass.lpszClassName,      // LPCWSTR lpClassName,
            (LPCWSTR)"Project21",           // LPCWSTR lpWindowName,
            WS_OVERLAPPEDWINDOW|WS_VISIBLE, // DWORD dwStyle,
            CW_USEDEFAULT,                  // int X,
            CW_USEDEFAULT,                  // int Y,
            CW_USEDEFAULT,                  // int nWidth,
            CW_USEDEFAULT,                  // int nHeight,
            0,                              // HWND hWndParent,
            0,                              // HMENU hMenu,
            Instance,                       // HINSTANCE hInstance,
            0);                             // LPVOID lpParam
        if(Window)
        {
            // temporary/debug variables
            int tmpXOffset = 0;  //graphics test
            int tmpYOffset = 0;  //graphics test

            // sound test: init DirectSound, fill buffer w/ sine wave, and play
            win32_sound_output SoundOutput = {};
            SoundOutput.BytesPerSample = sizeof(int16_t)*2;
            SoundOutput.SamplesPerSecond = 48000;
            //TODO: Get rid of LatencySampleCount!
            SoundOutput.LatencySampleCount = 3 * (SoundOutput.SamplesPerSecond / DEFINEDGameUpdateHz);
            //TODO: SafetyBytes sanity check - what is a good value here?
            SoundOutput.SafetyBytes = ((SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / DEFINEDGameUpdateHz) / 2;
            SoundOutput.BufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;            
            
            //TODO: move this to audio section, or leave here with memory allocs?
            int16_t *AllocatedAudioBufferMemory = (int16_t *)(VirtualAlloc(0, SoundOutput.BufferSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE));

            Win32InitializeDirectSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.BufferSize);
            Win32ClearSoundBuffer(&SoundOutput);
            GlobalAudioBuffer->Play(0, 0, DSBPLAY_LOOPING);


#if PROJECT21_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes((uint64_t)2);
#else   
            LPVOID BaseAddress = 0;
#endif
            application_memory ApplicationMemory = {};
            ApplicationMemory.INTERNAL_PlatformFreeFileMemory = INTERNAL_PlatformFreeFileMemory;
            ApplicationMemory.INTERNAL_PlatformReadEntireFile = INTERNAL_PlatformReadEntireFile;
            ApplicationMemory.INTERNAL_PlatformWriteEntireFile = INTERNAL_PlatformWriteEntireFile;

            ApplicationMemory.PermanentStorageSize = Megabytes(64);
            ApplicationMemory.TransientStorageSize = Gigabytes(1);
            uint64_t TotalMemorySize = ApplicationMemory.PermanentStorageSize + ApplicationMemory.TransientStorageSize;
            ApplicationMemory.PermanentStorage = VirtualAlloc(BaseAddress, TotalMemorySize, 
                                                              MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            ApplicationMemory.TransientStorage = ((uint8_t *)(ApplicationMemory.PermanentStorage) + ApplicationMemory.PermanentStorageSize); 

            if (AllocatedAudioBufferMemory && ApplicationMemory.PermanentStorage && ApplicationMemory.TransientStorage)
            {
                GlobalRunning = true;
                HDC DeviceContext = GetDC(Window);
                LARGE_INTEGER LastPerformanceCounter = Win32GetWallClock();
                LARGE_INTEGER FrameFlipWallClock = Win32GetWallClock();

                application_input ApplicationInput[2] = {}; 
                application_input *InputThisFrame = &ApplicationInput[0];
                application_input *InputLastFrame = &ApplicationInput[1];

                DWORD AudioLatencyBytes = 0;
                float32 AudioLatencySeconds = 0.0f;
                bool32 SoundIsValid = false;

#if PROJECT21_INTERNAL
                uint32_t DEBUGAudioCursorIndex = 0;
                INTERNAL_time_marker DEBUGAudioCursors[DEFINEDGameUpdateHz] = {0};
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

                    Win32ProcessWindowsMessages(KeyboardThisFrame);
                    
#if PROJECT21_INTERNAL
                    if (!GlobalPause)
                    {
#endif
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

                                NewControllerState->LStickAverageX = (GP->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)  ? -1.0f : NewControllerState->LStickAverageX;
                                NewControllerState->LStickAverageX = (GP->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) ?  1.0f : NewControllerState->LStickAverageX;
                                NewControllerState->LStickAverageY = (GP->wButtons & XINPUT_GAMEPAD_DPAD_UP)    ?  1.0f : NewControllerState->LStickAverageY;
                                NewControllerState->LStickAverageY = (GP->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)  ? -1.0f : NewControllerState->LStickAverageY;

                                // Process digital buttons
                                Win32ProcessXInputButton(GP->wButtons, &OldControllerState->Start,         XINPUT_GAMEPAD_START,          &NewControllerState->Start);
                                Win32ProcessXInputButton(GP->wButtons, &OldControllerState->Back,          XINPUT_GAMEPAD_BACK,           &NewControllerState->Back);
                                Win32ProcessXInputButton(GP->wButtons, &OldControllerState->LeftShoulder,  XINPUT_GAMEPAD_LEFT_SHOULDER,  &NewControllerState->LeftShoulder);
                                Win32ProcessXInputButton(GP->wButtons, &OldControllerState->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewControllerState->RightShoulder);
                                Win32ProcessXInputButton(GP->wButtons, &OldControllerState->ActionDown,    XINPUT_GAMEPAD_A,              &NewControllerState->ActionDown);
                                Win32ProcessXInputButton(GP->wButtons, &OldControllerState->ActionRight,   XINPUT_GAMEPAD_B,              &NewControllerState->ActionRight);
                                Win32ProcessXInputButton(GP->wButtons, &OldControllerState->ActionLeft,    XINPUT_GAMEPAD_X,              &NewControllerState->ActionLeft);
                                Win32ProcessXInputButton(GP->wButtons, &OldControllerState->ActionUp,      XINPUT_GAMEPAD_Y,              &NewControllerState->ActionUp);

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
                        Application.Update(&ApplicationMemory, &ApplicationOffscreenBuffer, InputThisFrame);

                        LARGE_INTEGER AudioWallClock = Win32GetWallClock();
                        float64 SecondsSinceFrameFlip = Win32GetSecondsElapsed(FrameFlipWallClock, AudioWallClock);

                        DWORD PlayCursor;
                        DWORD WriteCursor;
                        if(GlobalAudioBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                        {
                        /*
                        How Sound Output Works (we hope)

                        We need a safety value that is the number of samples / ms we think the loop might vary by from frame to frame (e.g. 2ms)

                        When we get to the point where we are ready to write audio, we look to see what the play cursor position is and we will
                        forecast ahead where we think the play cursor will be on the next frame boundary.

                        We will then look to see if the write cursor is before that point by at least the safety value. 
                        If it is, the target fill position is that frame boundary plus one more frame, giving us perfect
                        audio sync in the case of a card that has low enough latency.

                        If the write cursor is _after_ the next fram boundary, then we assume we can never sync the audio perfectly,
                        so we will write one frame's worth of audio plus the safety value.
                        */
                            if(!SoundIsValid)
                            {
                                SoundOutput.SampleIndex = WriteCursor - SoundOutput.BytesPerSample;
                                SoundIsValid = true;
                            }

                            DWORD ByteToLock = ((SoundOutput.SampleIndex*SoundOutput.BytesPerSample) % SoundOutput.BufferSize);
                            
                            DWORD ExpectedAudioBytesPerFrame =  (SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / DEFINEDGameUpdateHz;
                            DWORD ExpectedBytesUntilFrameFlip = (DWORD)(((TargetSecondsElapsedPerFrame - SecondsSinceFrameFlip) / (float32)TargetSecondsElapsedPerFrame) * (float32)ExpectedAudioBytesPerFrame);

                            DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedAudioBytesPerFrame ;
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
                            Application.GetSound(&ApplicationMemory, &ApplicationSoundBuffer);
                        
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
    #if PROJECT21_INTERNAL                    
                        //NOTE: DEBUGAudioCursorIndex - 1 will be wrong on the 0'th Index but for internal testing, whatevs
                        INTERNAL_Win32DrawAudioSync(&GlobalBackbuffer, ArrayCount(DEBUGAudioCursors), DEBUGAudioCursors, DEBUGAudioCursorIndex - 1, &SoundOutput, SecondsElapsedForFrame);
    #endif
                        Win32CopyBackbufferToWindow(DeviceContext, Dim.Width, Dim.Height, &GlobalBackbuffer, 0, 0, Dim.Width, Dim.Height);
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
