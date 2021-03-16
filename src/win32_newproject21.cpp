
#include <Windows.h>

#define local_persist static
#define global_variable static
#define internal static


global_variable bool Running; //TODO: Placeholder, see day 003 https://youtu.be/GAi_nTx1zG8
global_variable BITMAPINFO GlobalBitmapInfo;
global_variable void *GlobalBitmapMemory;
global_variable HBITMAP GlobalBitmapHandle;
global_variable HDC BitmapDeviceContext;

internal void
Win32ResizeDIBSection(int Width, int Height)
{
    if(GlobalBitmapHandle)
    {
        DeleteObject(GlobalBitmapHandle);
    }    
    else
    {
        BitmapDeviceContext = CreateCompatibleDC(0);
    }
    // check on this from day 003 https://youtu.be/GAi_nTx1zG8
    GlobalBitmapInfo.bmiHeader.biSize = sizeof(GlobalBitmapInfo.bmiHeader);
    GlobalBitmapInfo.bmiHeader.biWidth = Width;
    GlobalBitmapInfo.bmiHeader.biHeight = Height;
    GlobalBitmapInfo.bmiHeader.biPlanes = 1;
    GlobalBitmapInfo.bmiHeader.biBitCount = 32;
    GlobalBitmapInfo.bmiHeader.biCompression = BI_RGB;

    GlobalBitmapHandle = CreateDIBSection(BitmapDeviceContext, &GlobalBitmapInfo, DIB_RGB_COLORS, &GlobalBitmapMemory, 0, 0);
}

internal void
Win32UpdateWindow(HDC DeviceContext, int X, int Y, int Width, int Height)
{
    StretchDIBits(DeviceContext, 
                X, Y, Width, Height, 
                X, Y, Width, Height, 
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
            HDC DeviceContext = BeginPaint(Window, &Paint);

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
            while(Running)
            {
                BOOL MessageResult = GetMessageW(&Message, 0, 0, 0);
                if (MessageResult > 0)
                {
                    TranslateMessage(&Message);
                    DispatchMessageW(&Message);
                }
                else
                {
                    break;
                }
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

/*
typedef struct tagWNDCLASSW {
    // UINT        style;
    // WNDPROC     lpfnWndProc;
    // int         cbClsExtra;
    // int         cbWndExtra;
    // HINSTANCE   hInstance;
    // HICON       hIcon;
    // HCURSOR     hCursor;
    // HBRUSH      hbrBackground;
    // LPCWSTR     lpszMenuName;
    // LPCWSTR     lpszClassName;
} WNDCLASSW, *PWNDCLASSW, NEAR *NPWNDCLASSW, FAR *LPWNDCLASSW;
*/