
#include <stdint.h>
#include <stddef.h>

#include <Windows.h>
#include <Xinput.h>
#include <dsound.h>

#define local_persist static
#define global_variable static
#define internal static

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

// rather than link with the entire xinput library, we will grab pointers to the functions instead
// in a rather brilliant move, casey defines macros that, in turn, define function signatures,
// which can be used to both declare the function pointers and crash-preventing stub functions.
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
global_variable uint32_t GlobalAudioSamplesPerSecond;

internal void
Win32InitializeDirectSound(HWND Window, uint32_t SamplesPerSecond, size_t BufferSize)
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
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
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

            LPDIRECTSOUNDBUFFER SecondaryBuffer;
            DSBUFFERDESC BufferDesc = {};
            BufferDesc.dwSize = sizeof(BufferDesc);
            BufferDesc.dwBufferBytes = BufferSize;
            BufferDesc.lpwfxFormat = &WaveFormat;
            if(SUCCEEDED(DS->CreateSoundBuffer(&BufferDesc, &SecondaryBuffer, 0)))
            {
                OutputDebugStringW(L"Secondary Buffer Created Successfully!\n");
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
Win32InitializeXInput(void)
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
Win32RenderGradient(win32_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
    // Renders a blue/green gradient pattern to a buffer. 
    uint8_t *Row = (uint8_t *)Buffer->Memory;
    for(int Y = 0;
        Y < Buffer->Height;
        ++Y)
    {
        uint32_t *Pixel = (uint32_t *)Row;
        for(int X = 0;
            X < Buffer->Width;
            ++X)
        {
            uint8_t Blue = X + XOffset;
            uint8_t Green = Y + YOffset;
            *Pixel++ = ((Green << 8) | Blue);
        }
        Row += Buffer->Pitch;
    }
}

internal void
Win32InitializeBackBuffer(win32_offscreen_buffer *Buffer, int Width, int Height)
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
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

internal void
Win32BackbufferToWindow(HDC DeviceContext, int WindowWidth, int WindowHeight, win32_offscreen_buffer *Buffer, int X, int Y, int Width, int Height)
{
    StretchDIBits(DeviceContext, 
                  0, 0, Buffer->Width, Buffer->Height, 
                  0, 0, Buffer->Width, Buffer->Height, 
                  Buffer->Memory, &Buffer->BitmapInfo, DIB_RGB_COLORS, SRCCOPY);

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
            bool WasDown = (LParam & (1 << 30)) != 0;
            bool IsDown = (LParam & (1 << 31)) == 0;
            if(WasDown != IsDown)
            {
                // TODO: do keyboard stuff
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
    WNDCLASSW WindowClass = {};

    Win32InitializeXInput();
    Win32InitializeBackBuffer(&GlobalBackbuffer, 1280, 720);

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
            WindowClass.lpszClassName,           // LPCWSTR lpClassName,
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
            HDC DeviceContext = GetDC(Window);
            MSG Message;
            GlobalRunning = true;
            GlobalAudioSamplesPerSecond = 48000;
            int tmpXOff = 0;
            int tmpYOff = 0;

            Win32InitializeDirectSound(Window, GlobalAudioSamplesPerSecond, 
                                       GlobalAudioSamplesPerSecond * sizeof(int16_t) * 2);

            // MAIN LOOP
            while(GlobalRunning)
            {
                // Windows message loop
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
                for (DWORD ControllerIndex = 0;
                     ControllerIndex < XUSER_MAX_COUNT;
                     ++ControllerIndex)
                {
                    XINPUT_STATE ControllerState;
                    DWORD GetStateResult = XInputGetState(ControllerIndex, &ControllerState);
                    if(GetStateResult == ERROR_SUCCESS)
                    {
                        XINPUT_GAMEPAD *GP = &ControllerState.Gamepad;
                        bool Up = GP->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        bool Down = GP->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        bool Left = GP->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        bool Right = GP->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                        bool Start = GP->wButtons & XINPUT_GAMEPAD_START;
                        bool Back = GP->wButtons & XINPUT_GAMEPAD_BACK;
                        bool LShoulder = GP->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
                        bool RShoulder = GP->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
                        bool AButton = GP->wButtons & XINPUT_GAMEPAD_A;
                        bool BButton = GP->wButtons & XINPUT_GAMEPAD_B;
                        bool XButton = GP->wButtons & XINPUT_GAMEPAD_X;
                        bool YButton = GP->wButtons & XINPUT_GAMEPAD_Y;
                        int16_t LStickX = GP->sThumbLX;
                        int16_t LStickY = GP->sThumbLY;
                        int16_t RStickX = GP->sThumbRX;
                        int16_t RStickY = GP->sThumbRY;
                
                        if(AButton)
                        {
                            tmpYOff += 2;
                        }

                    }
                    else
                    {
                        //TODO: Logging/UI? - Controller not available / unplugged / error
                    }
                }

                Win32RenderGradient(&GlobalBackbuffer, tmpXOff, tmpYOff);
                win32_window_dim Dim = Win32GetWindowDimension(Window);
                Win32BackbufferToWindow(DeviceContext, Dim.Width, Dim.Height, &GlobalBackbuffer,
                                        0, 0, Dim.Width, Dim.Height);

                tmpXOff += 1;
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
