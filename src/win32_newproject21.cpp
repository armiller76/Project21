
#include <stdint.h>
#include <stddef.h>
#include <Windows.h>

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
};

struct win32_window_dim
{
    int Width;
    int Height;
};

global_variable bool Running;
global_variable win32_offscreen_buffer GlobalBackbuffer;

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
    int Pitch = Buffer->Width * Buffer->BytesPerPixel;
    for(int Y = 0;
        Y < Buffer->Height;
        ++Y)
    {
        uint32_t *Pixel = (uint32_t *)Row;
        for(int X = 0;
            X < Buffer->Width;
            ++X)
        {
            *Pixel++ = ((Y + YOffset) << 8) | (X + XOffset);
        }
        Row += Pitch;
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
                  0, 0, WindowWidth, WindowHeight, 
                  0, 0, Buffer->Width, Buffer->Height, 
                  Buffer->Memory, &Buffer->BitmapInfo, DIB_RGB_COLORS, SRCCOPY);

}

LRESULT CALLBACK
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
            Running = false;
        } break;
        case WM_CLOSE:
        {
            //TODO: Placeholder, see day 003 https://youtu.be/GAi_nTx1zG8
            Running = false;
        } break;
        case WM_ACTIVATEAPP:
        {
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
    WNDCLASSW Window = {};

    Win32InitializeBackBuffer(&GlobalBackbuffer, 1280, 720); 

    Window.style = CS_VREDRAW|CS_HREDRAW;                  // UINT        style;
    Window.lpfnWndProc = Win32MainWindowCallback;          // WNDPROC     lpfnWndProc;
    Window.hInstance = Instance;                           // HINSTANCE   hInstance;
//    Window.hIcon = ;                                     // HICON       hIcon;
    Window.lpszClassName = (LPCWSTR)"Project21";           // LPCWSTR     lpszClassName;

    if(RegisterClassW(&Window))
    {
        HWND WindowHandle = CreateWindowExW(
                                0,                              // _In_ DWORD dwExStyle,
                                Window.lpszClassName,      // _In_opt_ LPCWSTR lpClassName,
                                (LPCWSTR)"Project21",           // _In_opt_ LPCWSTR lpWindowName,
                                WS_OVERLAPPEDWINDOW|WS_VISIBLE, // _In_ DWORD dwStyle,
                                CW_USEDEFAULT,                  // _In_ int X,
                                CW_USEDEFAULT,                  // _In_ int Y,
                                CW_USEDEFAULT,                  // _In_ int nWidth,
                                CW_USEDEFAULT,                  // _In_ int nHeight,
                                0,                              // _In_opt_ HWND hWndParent,
                                0,                              // _In_opt_ HMENU hMenu,
                                Instance,                       // _In_opt_ HINSTANCE hInstance,
                                0);                             // _In_opt_ LPVOID lpParam
        if(WindowHandle)
        {
            MSG Message;
            Running = true;
            int tmpXOff = 0;
            int tmpYOff = 0;

            while(Running)
            {
                while(PeekMessageW(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if(Message.message == WM_QUIT)
                    {
                        Running = false;
                    }
                    TranslateMessage(&Message);
                    DispatchMessageW(&Message);
                }

                Win32RenderGradient(&GlobalBackbuffer, tmpXOff, tmpYOff);
                HDC DeviceContext = GetDC(WindowHandle);
                win32_window_dim Dim = Win32GetWindowDimension(WindowHandle);
                Win32BackbufferToWindow(DeviceContext, Dim.Width, Dim.Height, &GlobalBackbuffer,
                                    0, 0, Dim.Width, Dim.Height);
                ReleaseDC(WindowHandle, DeviceContext);
                ++tmpXOff;
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
