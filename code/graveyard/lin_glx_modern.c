
static bool g_glx_context_error_occured = false;

static int glx_context_error_handler(Display *display, XErrorEvent *event)
{
    printf("glx_context_error_handler received error\n");
    g_glx_context_error_occured = true;
    return 0;
}

// Helper to check for extension string presence. Copied from:
// http://www.opengl.org/resources/features/OGLextensions/
static bool gl_extension_supported(const char *extList, const char *extension)
{
    const char *start;
    const char *where, *terminator;

    // Extension names should not have spaces.
    where = strchr(extension, ' ');
    if (where || *extension == '\0')
        return false;

    for (start=extList;;)
    {
        where = strstr(start, extension);
        if (!where)
            break;

        terminator = where + strlen(extension);

        if (where == start || *(where - 1) == ' ')
            if (*terminator == ' ' || *terminator == '\0')
                return true;

        start = terminator;
    }

    return false;
}

static GLXContext create_current_glx_context(Display *display, Window window, GLXFBConfig fbc)
{
    // get glXCreateContextAttribsARB
    const char *glx_extensions = glXQueryExtensionsString(display, DefaultScreen( display));

    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc) glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");

    if (!gl_extension_supported(glx_extensions, "GLX_ARB_create_context") ||
        !glXCreateContextAttribsARB)
    {
        printf("glXCreateContextAttribsARB() not found\n");
        return 0;
    }

    // Install an X error handler so the application won't exit if GL 3.0
    // context allocation fails.
    //
    // Note this error handler is global.  All display connections in all threads
    // of a process use the same error handler, so be sure to guard against other
    // threads issuing X commands while this code is running.
    g_glx_context_error_occured = false;
    int (*oldHandler)(Display*, XErrorEvent*) = XSetErrorHandler(&glx_context_error_handler);

    // create glx context
    GLXContext glx_context = 0;
    int context_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB | GLX_CONTEXT_DEBUG_BIT_ARB,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        None
    };

    glx_context = glXCreateContextAttribsARB(display, fbc, 0, True, context_attribs);

    // check for errors
    XSync(display, False);
    if (g_glx_context_error_occured || !glx_context)
    {
        printf("failed to create glx context\n");
        return 0;
    }
    XSetErrorHandler(oldHandler);

    glXMakeCurrent(display, window, glx_context);
    return glx_context;
}

// Note: special case when debugging
// stepping over glXChooseFBConfig or glXGetFBConfigs cause
// _dl_catch_exception in libc
// stepping more leads to
// ?? () from /usr/lib/libnvidia-glcore.so.510.68.02
//
// Originally I used glXChooseFBConfig, then tried to fix it with glXGetFBConfigs
// and now I leave it at that for the time being
//
// You can still debug by setting breakpoint later and continuing (c) until
// that breakpoint is hit
static bool get_framebuffer_config(Display *display, GLXFBConfig *config)
{
    int screen = XDefaultScreen(display);

    int fbcs_count;
    GLXFBConfig *fbcs = glXGetFBConfigs(display, screen, &fbcs_count);
    if (!fbcs)
    {
        printf("no framebuffer configs received\n");
        return 0;
    }

    int best_fbc = -1, worst_fbc = -1, best_num_samp = -1, worst_num_samp = 999;
    for (int i = 0; i < fbcs_count; i++)
    {
        int fbc_attribs[11];
        glXGetFBConfigAttrib(display, fbcs[i], GLX_X_RENDERABLE,  &fbc_attribs[0]);
        glXGetFBConfigAttrib(display, fbcs[i], GLX_DRAWABLE_TYPE, &fbc_attribs[1]);
        glXGetFBConfigAttrib(display, fbcs[i], GLX_RENDER_TYPE,   &fbc_attribs[2]);
        glXGetFBConfigAttrib(display, fbcs[i], GLX_X_VISUAL_TYPE, &fbc_attribs[3]);
        glXGetFBConfigAttrib(display, fbcs[i], GLX_RED_SIZE,      &fbc_attribs[4]);
        glXGetFBConfigAttrib(display, fbcs[i], GLX_GREEN_SIZE,    &fbc_attribs[5]);
        glXGetFBConfigAttrib(display, fbcs[i], GLX_BLUE_SIZE,     &fbc_attribs[6]);
        glXGetFBConfigAttrib(display, fbcs[i], GLX_ALPHA_SIZE,    &fbc_attribs[7]);
        glXGetFBConfigAttrib(display, fbcs[i], GLX_DEPTH_SIZE,    &fbc_attribs[8]);
        glXGetFBConfigAttrib(display, fbcs[i], GLX_STENCIL_SIZE,  &fbc_attribs[9]);
        glXGetFBConfigAttrib(display, fbcs[i], GLX_DOUBLEBUFFER,  &fbc_attribs[10]);

        if (fbc_attribs[0] == True &&
            fbc_attribs[1] & GLX_WINDOW_BIT &&
            fbc_attribs[2] & GLX_RGBA_BIT &&
            fbc_attribs[3] == GLX_TRUE_COLOR &&
            fbc_attribs[4] == 8 &&
            fbc_attribs[5] == 8 &&
            fbc_attribs[6] == 8 &&
            fbc_attribs[7] == 8 &&
            fbc_attribs[8] == 24 &&
            fbc_attribs[9] == 8 &&
            fbc_attribs[10] == True)
        {
            XVisualInfo *visual_info = glXGetVisualFromFBConfig(display, fbcs[i]);
            if (visual_info)
            {
                int samp_buf, samples;
                glXGetFBConfigAttrib(display, fbcs[i], GLX_SAMPLE_BUFFERS, &samp_buf);
                glXGetFBConfigAttrib(display, fbcs[i], GLX_SAMPLES       , &samples);

                if (best_fbc < 0 || (samp_buf && samples) > best_num_samp)
                {
                    best_fbc = i;
                    best_num_samp = samples;
                }
                if (worst_fbc < 0 || !samp_buf || samples < worst_num_samp)
                {
                    worst_fbc = i;
                    worst_num_samp = samples;
                }
            }
            XFree(visual_info);
        }
    }

    if (best_fbc == -1)
    {
        return false;
    }

    *config = fbcs[best_fbc];
    XFree(fbcs);

    return true;
}

struct Platform_Window* platform_create_window(const char *name, int width, int height)
{
    Display *display = XOpenDisplay(0);
    if (!display)
    {
        printf("XOpenDisplay(0) failed\n");
        return 0;
    }

    Window root_window = XDefaultRootWindow(display);
    if (!root_window)
    {
        printf("XDefaultRootWindow(display) failed\n");
        XCloseDisplay(display);
        return 0;
    }

    // glx version 1.3 required (for framebuffer configs and modern context creation)
    int glx_major, glx_minor;
    if (!glXQueryVersion(display, &glx_major, &glx_minor) ||
        (glx_major == 1 && glx_minor < 3) ||
        (glx_major < 1))
    {
        printf("invalid glx version\n");
        return 0;
    }

    GLXFBConfig fbc;
    if (!get_framebuffer_config(display, &fbc))
    {
        printf("no framebuffer config found\n");
        return 0;
    }

    // set window attributes
    XVisualInfo *visual_info = glXGetVisualFromFBConfig(display, fbc);
    Colormap colormap = XCreateColormap(display, root_window, visual_info->visual, AllocNone);
    long event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask;
    int attribute_mask = CWEventMask | CWColormap;

    XSetWindowAttributes attributes = {};
    attributes.colormap = colormap;
    attributes.event_mask = event_mask;

    // create window
    Window window = XCreateWindow(display, root_window, 0, 0, width, height, 0, visual_info->depth, InputOutput, visual_info->visual, attribute_mask, &attributes);
    XFree(visual_info);
    XStoreName(display, window, name);
    XMapWindow(display, window);

    GLXContext glx_context = create_current_glx_context(display, window, fbc);
    if (!glx_context)
    {
        return 0;
    }

    // create platform window
    struct Platform_Window *platform_window = (struct Platform_Window*)malloc(sizeof(struct Platform_Window));
    if (!platform_window)
    {
        printf("malloc failed\n");
        XCloseDisplay(display);
        return 0;
    }

    platform_window->display = display;
    platform_window->window = window;

    return platform_window;
}
