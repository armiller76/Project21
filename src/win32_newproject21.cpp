
#include <Windows.h>

LRESULT CALLBACK
MainWindowCallback(HWND Window,
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
        } break;
        case WM_CLOSE:
        {
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
    WindowClass.lpfnWndProc = MainWindowCallback;               // WNDPROC     lpfnWndProc;
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
            for(;;)
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