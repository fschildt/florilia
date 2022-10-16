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

    /*
     * Create fake gl context, so I can create window with modern opengl context.
     * Doing this with the service window prevents creating another 'fake' window.
     */
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

	HGLRC glrc = wglCreateContext(dc);
    if (!glrc)
    {
        printf("wglCreateContext() failed\n");
        return false;
    }

    BOOL made_current = wglMakeCurrent(dc, glrc);
    if (made_current == FALSE)
    {
        printf("wglMakeCurrent() failed\n");
        return false;
    }

    // TODO: check if extensions are actually supported
    // "WGL_ARB_pixel_format"
    // "WGL_ARB_create_context"

    // TODO: check all return values indicating invalid function from wglGetProcAddress
    // msdn: 0 https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-wglgetprocaddress
    // khronos: (void*){0,1,2,3,-1} https://www.khronos.org/opengl/wiki/Load_OpenGL_Functions#Windows_2
    wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
    wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    if (!wglChoosePixelFormatARB || !wglCreateContextAttribsARB)
    {
        printf("wgl functions to create context not received\n");
        return false;
    }

    g_service_window = service_window;
    return true;
}

Win32_Window* platform_create_window(const char *name, int width, int height)
{
    Win32_Window *window = (Win32_Window*)malloc(sizeof(Platform_Window));
    if (!window)
    {
        printf("out of memory to create window\n");
        return 0;
    }

    // Note: SendMessage returns when execution is finished, so settings can be on stack
    Win32_Window_Settings settings = {name, width, height};
    HWND handle = (HWND)SendMessage(g_service_window, WIN32_CREATE_WINDOW, (WPARAM)&settings, 0);
    if (!handle)
        return 0;

    HDC dc = GetDC(handle);
    if (!dc)
    {
        printf("GetDC failed\n");
        return 0;
    }

    const int pixel_format_attribs[] =
    {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB, 32,
        WGL_DEPTH_BITS_ARB, 24,
        WGL_STENCIL_BITS_ARB, 8,
        0, // End
    };

    int pixel_format;
    UINT cnt_pixel_formats;
    if (wglChoosePixelFormatARB(dc, pixel_format_attribs, NULL, 1, &pixel_format, &cnt_pixel_formats) == FALSE)
    {
        printf("wglChoosePixelFormat return false\n");
        return 0;
    }

    PIXELFORMATDESCRIPTOR pfd;
    init_pfd(&pfd);

    if (SetPixelFormat(dc, pixel_format, &pfd) == FALSE)
    {
        printf("SetPixelFormat return false\n");
        return 0;
    }

    int context_attribs[] = 
    {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    HGLRC glrc = wglCreateContextAttribsARB(dc, 0, context_attribs);
    if (!glrc)
    {
        printf("wglCreateContextAttribsARB failed\n");
        return 0;
    }

    if (wglMakeCurrent(dc, glrc) == FALSE)
    {
        printf("wglMakeCurrent failed\n");
        return 0;
    }

    window->window = handle;
    window->dc = dc;
    window->glrc = glrc;
    return window;
}

bool platform_init_windowing()
{
    g_main_thread_id = GetCurrentThreadId();
    g_opengl_module = LoadLibraryA("opengl32.dll");
    if (!g_opengl_module)
    {
        printf("can't open opengl32.dll\n");
        return false;
    }

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
