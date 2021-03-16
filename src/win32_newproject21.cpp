
#include <stdint.h>
#include <stddef.h>
#include <Windows.h>

#define local_persist static
#define global_variable static
#define internal static




global_variable bool Running; 
global_variable BITMAPINFO GlobalBitmapInfo;
global_variable void *GlobalBitmapMemory;
global_variable int GlobalBitmapWidth;
global_variable int GlobalBitmapHeight;
global_variable int GlobalBytesPerPixel;

internal void
RenderGradient(int XOffset, int YOffset)
{
    uint8_t *Row = (uint8_t *)GlobalBitmapMemory;
    int Pitch = GlobalBitmapWidth * GlobalBytesPerPixel;
    for(int Y = 0;
        Y < GlobalBitmapHeight;
        ++Y)
    {
        uint32_t *Pixel = (uint32_t *)Row;
        for(int X = 0;
            X < GlobalBitmapWidth;
            ++X)
        {
            *Pixel++ = ((Y + YOffset) << 8) | (X + XOffset);
        }
        Row += Pitch;
    }
}

internal void
Win32ResizeDIBSection(int Width, int Height)
{
    if(GlobalBitmapMemory)
    {
        VirtualFree(GlobalBitmapMemory, 0, MEM_RELEASE); // MEM_DECOMMIT might also be appropriate?
    }

    GlobalBitmapWidth = Width;
    GlobalBitmapHeight = Height;
    GlobalBytesPerPixel = 4;

    // check on this from day 003 https://youtu.be/GAi_nTx1zG8
    GlobalBitmapInfo.bmiHeader.biSize = sizeof(GlobalBitmapInfo.bmiHeader);
    GlobalBitmapInfo.bmiHeader.biWidth = GlobalBitmapWidth;
    GlobalBitmapInfo.bmiHeader.biHeight = -GlobalBitmapHeight; // negative height specifies top-down DIB (top-left origin)
    GlobalBitmapInfo.bmiHeader.biPlanes = 1;
    GlobalBitmapInfo.bmiHeader.biBitCount = 32;
    GlobalBitmapInfo.bmiHeader.biCompression = BI_RGB;

    SIZE_T BitmapMemorySize = GlobalBitmapWidth * GlobalBitmapHeight * GlobalBytesPerPixel;
    GlobalBitmapMemory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

    RenderGradient(0, 0);
}

internal void
Win32UpdateWindow(HDC DeviceContext, RECT *ClientRect, int X, int Y, int Width, int Height)
{
    int ClientWidth = ClientRect->right - ClientRect->left;
    int WindowHeight = ClientRect->bottom - ClientRect->top;
    StretchDIBits(DeviceContext, 
                  0, 0, GlobalBitmapWidth, GlobalBitmapHeight, 
                  0, 0, ClientWidth, WindowHeight, 
                  GlobalBitmapMemory,
                  &GlobalBitmapInfo, 
                  DIB_RGB_COLORS, SRCCOPY);

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
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            int Width = ClientRect.right - ClientRect.left;
            int Height = ClientRect.bottom - ClientRect.top;
            Win32ResizeDIBSection(Width, Height); 
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
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            HDC DeviceContext = BeginPaint(Window, &Paint);
            Win32UpdateWindow(DeviceContext, &ClientRect, 
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
    WindowClass.style = CS_OWNDC|CS_VREDRAW|CS_HREDRAW;         // UINT        style;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;               // WNDPROC     lpfnWndProc;
    WindowClass.hInstance = Instance;                           // HINSTANCE   hInstance;
//    WindowClass.hIcon = ;                                     // HICON       hIcon;
    WindowClass.lpszClassName = (LPCWSTR)"Project21";           // LPCWSTR     lpszClassName;

    if(RegisterClassW(&WindowClass))
    {
        HWND WindowHandle = CreateWindowExW(
                                0,                              // _In_ DWORD dwExStyle,
                                WindowClass.lpszClassName,      // _In_opt_ LPCWSTR lpClassName,
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

                RenderGradient(tmpXOff, tmpYOff);
                HDC DeviceContext = GetDC(WindowHandle);
                RECT ClientRect;
                GetClientRect(WindowHandle, &ClientRect);
                Win32UpdateWindow(DeviceContext, &ClientRect, 0, 0, ClientRect.right - ClientRect.left, ClientRect.bottom - ClientRect.top);
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
