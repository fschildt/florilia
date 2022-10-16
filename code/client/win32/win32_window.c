#define WIN32_SERVICE_WINDOW_CREATED     (WM_USER + 0)
#define WIN32_SERVICE_WINDOW_NOT_CREATED (WM_USER + 1)
#define WIN32_CREATE_WINDOW              (WM_USER + 2)
#define WIN32_DESTROY_WINDOW             (WM_USER + 3)

static HWND  g_service_window;
static DWORD g_main_thread_id;

internal void
init_pfd(PIXELFORMATDESCRIPTOR *pfd)
{
    memset(pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
    pfd->nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd->nVersion = 1;
    pfd->dwFlags = PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
    pfd->iPixelType = PFD_TYPE_RGBA;
    pfd->cColorBits = 32;
    pfd->cDepthBits = 24;
    pfd->cStencilBits = 8;
    pfd->iLayerType = PFD_MAIN_PLANE;
}

internal LRESULT
window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    LRESULT result = 0;
    switch (message)
    {
        case WM_CLOSE:
        {
            PostThreadMessage(g_main_thread_id, message, (WPARAM)window, l_param);
        }
        break;

        case WM_CHAR:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_QUIT:
        case WM_SIZE:
        {
            PostThreadMessage(g_main_thread_id, message, w_param, l_param);
        }
        break;

        default:
        {
            result = DefWindowProcA(window, message, w_param, l_param);
        }
        break;
    }

    return result;
}

internal HWND
create_window(const char *name, int width, int height)
{
    const char *classname = "win32_window_class";

    WNDCLASSEX window_class = {};
    window_class.style = CS_OWNDC;
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = &window_proc;
    window_class.hInstance = GetModuleHandle(NULL);
    window_class.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    window_class.lpszClassName = classname;
    if (RegisterClassExA(&window_class) == 0)
    {
        printf("RegisterClassEx() failed\n");
        return 0;
    }

    HWND window = CreateWindowExA(0,
                                  classname,
                                  name,
                                  WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  width,
                                  height,
                                  0,
                                  0,
                                  window_class.hInstance,
                                  0);
    if (!window)
    {
        printf("CreateWindowEx() failed\n");
        return 0;
    }
    return window;
}

LRESULT
win32_service_window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
	LRESULT result = 0;
	switch (message)
	{
        case WIN32_CREATE_WINDOW:
        {
            Win32_Window_Settings *settings = (Win32_Window_Settings*)w_param;
            return (LRESULT)create_window(settings->name, settings->width, settings->height);
        }
        break;

        case WIN32_DESTROY_WINDOW:
        {
            DestroyWindow(window);
        }
        break;

        default:
        {
            result = DefWindowProc(window, message, w_param, l_param);
        }
        break;
	}
	return result;
}

internal bool
create_service_window()
{
    WNDCLASSEX service_window_class = {};
    service_window_class.style = CS_OWNDC;
    service_window_class.cbSize = sizeof(service_window_class);
    service_window_class.lpfnWndProc = &win32_service_window_proc;
    service_window_class.hInstance = GetModuleHandle(NULL);
    service_window_class.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    service_window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    service_window_class.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    service_window_class.lpszClassName = "win32_service_window_class";
    if (RegisterClassEx(&service_window_class) == 0)
    {
        printf("RegisterClassEx() failed\n");
        return 0;
    }

    const char *service_window_name = "win32_service_window";
    HWND service_window = CreateWindowEx(0,
                                         service_window_class.lpszClassName,
                                         service_window_name,
                                         0,
                                         CW_USEDEFAULT,
                                         CW_USEDEFAULT,
                                         CW_USEDEFAULT,
                                         CW_USEDEFAULT,
                                         0,
                                         0,
                                         service_window_class.hInstance,
                                         0);
    if (!service_window)
    {
        printf("CreateWindowEx() failed\n");
        return 0;
    }

    HDC dc = GetDC(service_window);
    if (!dc)
    {
        printf("GetDC() failed\n");
        return false;
    }

    PIXELFORMATDESCRIPTOR pfd;
    init_pfd(&pfd);

    int pixel_format = ChoosePixelFormat(dc, &pfd);
    if (pixel_format == 0)
    {
        printf("ChoosePixelFormat failed\n");
        return false;
    }

    BOOL pixel_format_set = SetPixelFormat(dc, pixel_format, &pfd);
    if (pixel_format_set == FALSE)
    {
        printf("SetPixelFormat() failed\n");
        return false;
    }

    g_service_window = service_window;
    return true;
}

DWORD WINAPI win32_service_window_thread(LPVOID lpParameter)
{
    if (!create_service_window())
    {
        PostThreadMessage(g_main_thread_id, WIN32_SERVICE_WINDOW_NOT_CREATED, 0, 0);
        return 0;
    }

    PostThreadMessage(g_main_thread_id, WIN32_SERVICE_WINDOW_CREATED, 0, 0);

    for (;;)
    {
        // receive messages for all windows (created by this thread)
        MSG message;
        BOOL recvd = GetMessageA(&message, 0, 0, 0);
        if (recvd == -1)
        {
            // handle error
            printf("GetMessage failed\n");
            return 0;
        }

        TranslateMessage(&message);
        DispatchMessage(&message);
    }
}

internal void
win32_show_offscreen_buffer(Win32_Window *window)
{
    Win32_Offscreen_Buffer *buff = &window->offscreen_buffer;
    StretchDIBits(window->dc,
                  0, 0, buff->width, buff->height,
                  0, 0, buff->width, buff->height,
                  buff->pixels,
                  &buff->bitmap_info,
                  DIB_RGB_COLORS,
                  SRCCOPY);

    SwapBuffers(window->dc);
}

internal b32
win32_recreate_offscreen_buffer(Win32_Offscreen_Buffer *buff, s32 width, s32 height)
{
    s32 bytes_per_pixel = 4;
    s32 pixels_size = width * height * bytes_per_pixel;

    u32 *pixels = realloc(buff->pixels, pixels_size);

    BITMAPINFO *bitmap_info = &buff->bitmap_info;
    memset(bitmap_info->bmiColors, 0, sizeof(bitmap_info->bmiColors));
    bitmap_info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info->bmiHeader.biWidth  = width;
    bitmap_info->bmiHeader.biHeight = height;
    bitmap_info->bmiHeader.biPlanes = 1;
    bitmap_info->bmiHeader.biBitCount = 32;
    bitmap_info->bmiHeader.biCompression = BI_RGB;
    bitmap_info->bmiHeader.biSizeImage = 0;
    bitmap_info->bmiHeader.biXPelsPerMeter = 0;
    bitmap_info->bmiHeader.biYPelsPerMeter = 0;
    bitmap_info->bmiHeader.biClrUsed = 0;
    bitmap_info->bmiHeader.biClrImportant = 0;

    buff->width  = width;
    buff->height = height;
    buff->pixels = pixels;
    return true;
}

#define ADD_KEY_EVENT(_c) \
    event->type = FLORILIA_EVENT_KEY; \
    event->key.c = _c; \
    *event_count += 1;

internal bool
win32_process_window_events(Win32_Window *window, Florilia_Event *events, u32 *event_count)
{
    u32 event_count_max = *event_count;
    *event_count = 0;

    MSG message;
    while (*event_count < event_count_max && PeekMessage(&message, 0, 0, 0, PM_REMOVE))
    {
        Florilia_Event *event = events + *event_count;

        switch (message.message)
        {
            case WM_SIZE:
            {
                UINT width = LOWORD(message.lParam);
                UINT height = HIWORD(message.lParam);
                win32_recreate_offscreen_buffer(&window->offscreen_buffer, width, height);
            }
            break;

            case WM_CHAR:
            {
                if (message.wParam < 0 || message.wParam > 127)
                    break;

                char c = (char)message.wParam;
                ADD_KEY_EVENT(c);
            }
            break;

            case WM_KEYDOWN:
            {
                switch (message.wParam)
                {
                case VK_LEFT:   { ADD_KEY_EVENT(FLORILIA_KEY_LEFT);   } break;
                case VK_RIGHT:  { ADD_KEY_EVENT(FLORILIA_KEY_RIGHT);  } break;
                case VK_UP:     { ADD_KEY_EVENT(FLORILIA_KEY_UP);     } break;
                case VK_DOWN:   { ADD_KEY_EVENT(FLORILIA_KEY_DOWN);   } break;
                case VK_DELETE: { ADD_KEY_EVENT(127 /* in ascii */);  } break;
                }
            }
            break;

            case WM_CLOSE:
            case WM_DESTROY:
            {
                return false;
            }
            break;

            default:
            {
                return true;
            }
            break;
        }
    }

    return true;
}
#undef ADD_KEY_EVENT

internal bool
win32_create_window(Win32_Window *window, const char *name, int width, int height)
{
    // SendMessage returns after the request has been processed
    Win32_Window_Settings settings = {name, width, height};
    HWND handle = (HWND)SendMessage(g_service_window, WIN32_CREATE_WINDOW, (WPARAM)&settings, 0);
    if (!handle)
        return 0;

    // We are back in our thread
    HDC dc = GetDC(handle);
    if (!dc)
    {
        printf("GetDC failed\n");
        return 0;
    }

    PIXELFORMATDESCRIPTOR pfd;
    init_pfd(&pfd);

    int pixel_format = ChoosePixelFormat(dc, &pfd);
    if (pixel_format == 0)
    {
        printf("ChoosePixelFormat failed\n");
        return false;
    }

    BOOL pixel_format_set = SetPixelFormat(dc, pixel_format, &pfd);
    if (pixel_format_set == FALSE)
    {
        printf("SetPixelFormat() failed\n");
        return false;
    }

    window->window = handle;
    window->dc = dc;
    return window;
}

internal bool
win32_init_windowing()
{
    g_main_thread_id = GetCurrentThreadId();

    DWORD tid;
    HANDLE thread = CreateThread(0, 0, win32_service_window_thread, 0, 0, &tid);
    if (!thread)
    {
        printf("error: CreateThread(...) failed\n");
        return false;
    }

    // wait until service window is ready
    for (;;)
    {
        MSG message;
        GetMessageA(&message, 0, 0, 0);

        if (message.message == WIN32_SERVICE_WINDOW_CREATED)
            return true;
        else
            return false;
    }
}
