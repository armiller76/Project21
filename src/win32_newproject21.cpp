
#include <math.h> // currenly only using sinf - can I do better / as well?
#include <stdint.h>
#include <stddef.h>

#define local_persist static
#define global_variable static
#define internal static

#define Pi32 3.14159265359f

typedef float float32;
typedef double float64;
typedef int32_t bool32;

#include "project21.cpp"

#include <malloc.h>
#include <Windows.h>
#include <Xinput.h>
#include <dsound.h>

#include "win32_newproject21.h"

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

global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalAudioBuffer;

internal void
Win32InitDirectSound(HWND Window, uint32_t SamplesPerSecond, size_t BufferSize)
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
            BufferDesc.dwFlags = 0;
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
Win32InitXInput(void)
{
    HMODULE XInputDLL = LoadLibraryW(XINPUT_DLL_W);
    if (XInputDLL)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputDLL, "XInputGetState");
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputDLL, "XInputSetState");
    }

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

internal void
Win32InitBackBuffer(win32_offscreen_buffer *Buffer, int Width, int Height)
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

    // check on this from day 003 https://youtu.be/GAi_nTx1zG8
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
Win32BackbufferToWindow(HDC DeviceContext, int WindowWidth, int WindowHeight, win32_offscreen_buffer *Buffer, int X, int Y, int Width, int Height)
{
    StretchDIBits(DeviceContext, 
                  0, 0, Buffer->Width, Buffer->Height, 
                  0, 0, Buffer->Width, Buffer->Height, 
                  Buffer->Memory, &Buffer->BitmapInfo, DIB_RGB_COLORS, SRCCOPY);

}

internal void
Win32ProcessXInputButton(DWORD XInputButtonState, button_state *OldState, DWORD ButtonBit, button_state *NewState)
{
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit);
    NewState->HalfTransitionCount = (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
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
            uint32_t VKCode = WParam;
            bool32 WasDown = ((LParam & (1 << 30)) != 0);
            bool32 IsDown = ((LParam & (1 << 31)) == 0);
            if(WasDown != IsDown)
            {
                if(VKCode == 'W')
                {
                }
                else if(VKCode == 'A')
                {
                }
                else if(VKCode == 'S')
                {
                }
                else if(VKCode == 'D')
                {
                }
                else if(VKCode == 'Q')
                {
                }
                else if(VKCode == 'E')
                {
                }
                else if(VKCode == VK_UP)
                {
                }
                else if(VKCode == VK_LEFT)
                {
                }
                else if(VKCode == VK_DOWN)
                {
                }
                else if(VKCode == VK_RIGHT)
                {
                }
                else if(VKCode == VK_ESCAPE)
                {
                    OutputDebugStringA("ESCAPE: ");
                    if(IsDown)
                    {
                        OutputDebugStringA("IsDown ");
                    }
                    if(WasDown)
                    {
                        OutputDebugStringA("WasDown");
                    }
                    OutputDebugStringA("\n");
                }
                else if(VKCode == VK_SPACE)
                {
                }
            }
            bool32 AltKeyWasDown = (LParam & (1 << 29));
            if((VKCode == VK_F4) && AltKeyWasDown)
            {
                GlobalRunning = false;
            }
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            win32_window_dim Dim = Win32GetWindowDimension(Window);
            HDC DeviceContext = BeginPaint(Window, &Paint);
            Win32BackbufferToWindow(DeviceContext, Dim.Width, Dim.Height, 
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
    LARGE_INTEGER PerfCountFrequency_;
    QueryPerformanceFrequency(&PerfCountFrequency_);
    int64_t PerfCountFrequency = PerfCountFrequency_.QuadPart;

    uint64_t LastCycleCount = __rdtsc();

    WNDCLASSW WindowClass = {};

    Win32InitXInput();
    Win32InitBackBuffer(&GlobalBackbuffer, 1280, 720);

    WindowClass.style = CS_VREDRAW|CS_HREDRAW;                  // UINT        style;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;          // WNDPROC     lpfnWndProc;
    WindowClass.hInstance = Instance;                           // HINSTANCE   hInstance;
    //WindowClass.hIcon = ;                                     // HICON       hIcon;
    WindowClass.lpszClassName = (LPCWSTR)"Project21";           // LPCWSTR     lpszClassName;

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
            0                               // LPVOID lpParam
        );    
        if(Window)
        {
            // temporary/debug variables
            int tmpXOffset = 0;  //graphics test
            int tmpYOffset = 0;  //graphics test

            // sound test: init DirectSound, fill buffer w/ sine wave, and play
            win32_sound_output SoundOutput = {};
            SoundOutput.BytesPerSample = sizeof(int16_t)*2;
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
            SoundOutput.BufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;            
            Win32InitDirectSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.BufferSize);
            Win32ClearSoundBuffer(&SoundOutput);
            GlobalAudioBuffer->Play(0, 0, DSBPLAY_LOOPING);

            int16_t *AudioCopyBuffer = (int16_t *)(VirtualAlloc(0, 48000*2*sizeof(int16_t), 
                                        MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE));

            application_input ApplicationInput[2] = {}; 
            application_input *NewAppInput = &ApplicationInput[0];
            application_input *OldAppInput = &ApplicationInput[1];

            // MAIN LOOP
            GlobalRunning = true;
            HDC DeviceContext = GetDC(Window);
            LARGE_INTEGER LastPerfCount;
            QueryPerformanceCounter(&LastPerfCount);

            while(GlobalRunning)
            {
                // Windows message loop
                MSG Message;    
                while(PeekMessageW(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if(Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }
                    TranslateMessage(&Message);
                    DispatchMessageW(&Message);
                }

                // Controller/Keyboard loop
                //TODO: Poll more frequently?
                int32_t MaxControllerCount = XUSER_MAX_COUNT;
                if(MaxControllerCount > ArrayCount(NewAppInput->Controllers))
                {
                    MaxControllerCount = ArrayCount(NewAppInput->Controllers);
                }

                for (DWORD ControllerIndex = 0;
                     ControllerIndex < MaxControllerCount;
                     ++ControllerIndex)
                {
                    input_controller *OldController = &OldAppInput->Controllers[ControllerIndex];
                    input_controller *NewController = &NewAppInput->Controllers[ControllerIndex];

                    XINPUT_STATE ControllerState;
                    if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        //TODO: Deadzone processing
                        
                        XINPUT_GAMEPAD *GP = &ControllerState.Gamepad;
                        bool32 Up = GP->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        bool32 Down = GP->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        bool32 Left = GP->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        bool32 Right = GP->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                        int16_t LStickX = GP->sThumbLX;
                        int16_t LStickY = GP->sThumbLY;
                        int16_t RStickX = GP->sThumbRX;
                        int16_t RStickY = GP->sThumbRY;

                        NewController->IsAnalog = true;
                        NewController->LEndX = OldController->LEndX;
                        NewController->LEndY = OldController->LEndY;
                        NewController->REndX = OldController->REndX;
                        NewController->REndY = OldController->REndY;

                        float32 LX;
                        if(GP->sThumbLX < 0)
                        {
                            LX = (float32)GP->sThumbLX / 32768.0f;
                        }
                        else
                        {
                            LX = (float32)GP->sThumbLX / 32767.0f;
                        }
                        NewController->LMinX = NewController->LMaxX = NewController->LEndX = LX;

                        float32 LY;
                        if(GP->sThumbLY < 0)
                        {
                            LY = (float32)GP->sThumbLY / 32768.0f;
                        }
                        else
                        {
                            LY = (float32)GP->sThumbLY / 32767.0f;
                        }
                        NewController->LMinY = NewController->LMaxY = NewController->LEndY = LY;

                        float32 RX;
                        if(GP->sThumbRX < 0)
                        {
                            RX = (float32)GP->sThumbRX / 32768.0f;
                        }
                        else
                        {
                            RX = (float32)GP->sThumbRX / 32767.0f;
                        }
                        NewController->RMinX = NewController->RMaxX = NewController->REndX = RX;

                        float32 RY;
                        if(GP->sThumbRY < 0)
                        {
                            RY = (float32)GP->sThumbRY / 32768.0f;
                        }
                        else
                        {
                            RY = (float32)GP->sThumbRY / 32767.0f;
                        }
                        NewController->RMinY = NewController->RMaxY = NewController->REndY = RY;

                        Win32ProcessXInputButton(GP->wButtons, &OldController->Start,         XINPUT_GAMEPAD_START,          &NewController->Start);
                        Win32ProcessXInputButton(GP->wButtons, &OldController->Back,          XINPUT_GAMEPAD_BACK,           &NewController->Back);
                        Win32ProcessXInputButton(GP->wButtons, &OldController->LeftShoulder,  XINPUT_GAMEPAD_LEFT_SHOULDER,  &NewController->LeftShoulder);
                        Win32ProcessXInputButton(GP->wButtons, &OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);
                        Win32ProcessXInputButton(GP->wButtons, &OldController->Down,          XINPUT_GAMEPAD_A,              &NewController->Down);
                        Win32ProcessXInputButton(GP->wButtons, &OldController->Right,         XINPUT_GAMEPAD_B,              &NewController->Right);
                        Win32ProcessXInputButton(GP->wButtons, &OldController->Left,          XINPUT_GAMEPAD_X,              &NewController->Left);
                        Win32ProcessXInputButton(GP->wButtons, &OldController->Up,            XINPUT_GAMEPAD_Y,              &NewController->Up);
                    }
                    else
                    {
                        //TODO: Logging/UI? - Controller not available / unplugged / error
                    }
                }

                application_offscreen_buffer ApplicationOffscreenBuffer = {};
                ApplicationOffscreenBuffer.Memory = GlobalBackbuffer.Memory;
                ApplicationOffscreenBuffer.Width = GlobalBackbuffer.Width;
                ApplicationOffscreenBuffer.Height = GlobalBackbuffer.Height;
                ApplicationOffscreenBuffer.Pitch = GlobalBackbuffer.Pitch;

                //TODO: this is fairly okayish test code, but there is zero syncronization with the
                // rest of the application. that needs to be worked out.
                bool32 SoundIsValid = false;
                DWORD PlayCursor = 0;
                DWORD WriteCursor = 0;
                DWORD TargetCursor = 0;
                DWORD ByteToLock = 0;
                DWORD BytesToWrite = 0;
                if(SUCCEEDED(GlobalAudioBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
                {
                    ByteToLock = ((SoundOutput.SampleIndex*SoundOutput.BytesPerSample) % SoundOutput.BufferSize);
                    TargetCursor = (PlayCursor + (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) % SoundOutput.BufferSize;
                    if (ByteToLock > TargetCursor)
                    {
                        BytesToWrite = SoundOutput.BufferSize - ByteToLock;
                        BytesToWrite += TargetCursor;
                    }
                    else
                    {
                        BytesToWrite = TargetCursor - ByteToLock;
                    }
                    SoundIsValid = true;
                }

                application_sound_output_buffer ApplicationSoundBuffer = {};
                ApplicationSoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                ApplicationSoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
                ApplicationSoundBuffer.Memory = AudioCopyBuffer;
                
                ApplicationUpdate(&ApplicationOffscreenBuffer, &ApplicationSoundBuffer, NewAppInput);
                if(SoundIsValid)
                {
                    Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &ApplicationSoundBuffer);
                }

                win32_window_dim Dim = Win32GetWindowDimension(Window);
                Win32BackbufferToWindow(DeviceContext, Dim.Width, Dim.Height, &GlobalBackbuffer,
                                        0, 0, Dim.Width, Dim.Height);

                //TODO: Implement swap (and maybe min/max/other useful macros)
                //TODO: Do these need to be cleared to zero? or something else?
                application_input *Temp = NewAppInput;
                NewAppInput = OldAppInput;
                OldAppInput = Temp;
                
                uint64_t EndCycleCount = __rdtsc();
                LARGE_INTEGER EndPerfCount;
                QueryPerformanceCounter(&EndPerfCount);

                uint64_t CyclesElapsed = EndCycleCount - LastCycleCount;
                int64_t PerfCountElapsed = EndPerfCount.QuadPart - LastPerfCount.QuadPart;
                int32_t mSecLastFrame = (int32_t)((1000*PerfCountElapsed) / PerfCountFrequency);
                int32_t MCyclesLastFrame = (int32_t)(CyclesElapsed / (1000 * 1000));
                //char DebugBuffer[256];
                //wsprintfW((LPWSTR)DebugBuffer, L"Last frame: %dms, %d Mcycles\n", mSecLastFrame, MCyclesLastFrame);
                //OutputDebugStringW((LPWSTR)DebugBuffer);

                LastPerfCount = EndPerfCount;
                LastCycleCount = EndCycleCount;
            }
        }
        else
        {
            //TODO: Logging/Error handling
        }
        
    }
    else
    {
        // TODO: Logging/Error handling
    }
    
    return EXIT_SUCCESS;
}
