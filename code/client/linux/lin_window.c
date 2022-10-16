static Display *g_display;

internal b32
lin_connect_to_x()
{
    const char *display_name = ":0.0";
    g_display = XOpenDisplay(display_name);
    if (!g_display)
    {
        printf("XOpenDisplay(%s) failed\n", display_name);
        return false;
    }

    return true;
}


internal b32
lin_recreate_offscreen_buffer(Lin_Offscreen_Buffer *offscreen_buffer, u32 width, u32 height)
{
    u32 bytes_per_pixel = 4;
    u32 pixels_size = width * height * bytes_per_pixel;

    if (!offscreen_buffer->pixels)
    {
        glEnable(GL_TEXTURE_2D);

        u32 texture_id;
        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &texture_id);
        glBindTexture(GL_TEXTURE_2D, texture_id);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        offscreen_buffer->gl_texture_id = texture_id;
    }

    u32 *pixels = realloc(offscreen_buffer->pixels, pixels_size);
    if (!pixels)
    {
        printf("offscreen buffer pixel realloc failed\n");
        return false;
    }

    glViewport(0, 0, width, height);

    offscreen_buffer->width  = width;
    offscreen_buffer->height = height;
    offscreen_buffer->pixels = pixels;
    return true;
}

// Todo: use modern glX functions
internal b32
lin_create_window(Lin_Window *window, const char *name, u32 width, u32 height)
{
    if (!g_display)
    {
        if (!lin_connect_to_x())
            return 0;
    }
    Display *display = g_display;

    int screen_number = XDefaultScreen(display);
    Window root_window_id = XRootWindow(display, screen_number);
    if (!root_window_id)
    {
        printf("XDefaultRootWindow(display) failed\n");
        XCloseDisplay(display);
        return false;
    }

    // get visual info
    int depth = 24;
    int va[] = {
        GLX_RGBA, 1,
        GLX_DOUBLEBUFFER, 1,
        GLX_RED_SIZE,   8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE,  8,
        GLX_ALPHA_SIZE, 8,
        GLX_DEPTH_SIZE, depth,
        None
    };
    XVisualInfo* vinfo = glXChooseVisual(display, screen_number, va);
    if (!vinfo)
    {
        printf("glXChooseVisual failed\n");
        return false;
    }

    // set window attribs
    long swa_mask = CWEventMask | CWColormap | CWBackPixmap |CWBorderPixel; 

    long swa_event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask;
    Colormap colormap = XCreateColormap(display, root_window_id, vinfo->visual, AllocNone);

    XSetWindowAttributes swa = {};
    swa.background_pixmap = None;
    swa.border_pixel = 0;
    swa.event_mask = swa_event_mask;
    swa.colormap = colormap;

    Window window_id = XCreateWindow(display, root_window_id, 0, 0, width, height, 0, depth, InputOutput, vinfo->visual, swa_mask, &swa);
    XStoreName(display, window_id, name);
    XMapWindow(display, window_id);



    // glx context
    Bool direct = True;
    GLXContext glx_context = glXCreateContext(display, vinfo, 0, direct);
    if (glx_context == 0)
    {
        printf("glXCreateContext failed\n");
        return false;
    }
    Bool made_current = glXMakeCurrent(display, window_id, glx_context);
    if (made_current == False)
    {
        printf("glXMakeCurrent failed\n");
        return 0;
    }


    g_display = display;
    window->xid = window_id;
    window->event_mask = swa_event_mask;
    window->glx_context = glx_context;
    if (!lin_recreate_offscreen_buffer(&window->offscreen_buffer, width, height))
    {
        return 0;
    }
    return true;
}

internal void
lin_destroy_window(Lin_Window *window)
{
    XDestroyWindow(g_display, window->xid);
}


#define ADD_KEY_EVENT(_c) \
    event->type  = FLORILIA_EVENT_KEY; \
    event->key.c = _c; \
    *event_count += 1;

internal b32
lin_process_window_events(Lin_Window *window, Florilia_Event *events, u32 *event_count)
{
    u32 event_count_max = *event_count;
    *event_count = 0;

    XEvent xevent;
    while (*event_count < event_count_max && 
           XCheckWindowEvent(g_display, window->xid, window->event_mask, &xevent) == True)
    {
        Florilia_Event *event = events + *event_count;

        // Todo: These keys could be down/set before the application starts.
        //       That invalidates this code. Can Xlib just give us the correct characters?
        static b32 is_caps;
        static b32 is_shift_l;
        static b32 is_shift_r;
        b32 is_uppercase = ( is_caps && !(is_shift_l || is_shift_r)) ||
                            (!is_caps &&  (is_shift_l || is_shift_r));

        switch (xevent.type)
        {
        case ConfigureNotify:
        {
            Lin_Offscreen_Buffer *offscreen_buffer = &window->offscreen_buffer;

            u32 width  = xevent.xconfigure.width;
            u32 height = xevent.xconfigure.height;
            if (width  != offscreen_buffer->width ||
                height != offscreen_buffer->height)
            {
                lin_recreate_offscreen_buffer(offscreen_buffer, width, height);
            }
        }
        break;

        case DestroyNotify:
        {
            return false;
        }
        break;

        // Note: keysyms defined in X11/keysymdef.h.
        case KeyPress:
        {
            int index = is_uppercase ? 1 : 0;
            KeySym keysym = XLookupKeysym(&xevent.xkey, index);
            if (keysym >= 32 && keysym <= 126) { ADD_KEY_EVENT(keysym); }
            else if (keysym == XK_Tab)         { ADD_KEY_EVENT('\t');   }
            else if (keysym == XK_Return)      { ADD_KEY_EVENT('\r');   }
            else if (keysym == XK_BackSpace)   { ADD_KEY_EVENT(8);      }
            else if (keysym == XK_Delete)      { ADD_KEY_EVENT(127);    } 
            else if (keysym == XK_Left)        { ADD_KEY_EVENT(FLORILIA_KEY_LEFT);  }
            else if (keysym == XK_Right)       { ADD_KEY_EVENT(FLORILIA_KEY_RIGHT); }
            else if (keysym == XK_Up)          { ADD_KEY_EVENT(FLORILIA_KEY_UP);    }
            else if (keysym == XK_Down)        { ADD_KEY_EVENT(FLORILIA_KEY_DOWN);  }
            else if (keysym == XK_Shift_L)     is_shift_l = true;
            else if (keysym == XK_Shift_R)     is_shift_r = true;
            else if (keysym == XK_Caps_Lock)   is_caps = true;
        }
        break;

        case KeyRelease:
        {
            int index = 0;
            KeySym keysym = XLookupKeysym(&xevent.xkey, index);

            if (keysym == XK_Shift_L)        is_shift_l = false;
            else if (keysym == XK_Shift_R)   is_shift_r = false;
            else if (keysym == XK_Caps_Lock) is_caps = false;
        }
        break;

        case GraphicsExpose:
        {
            printf("graphics exposure happened\n");
        }
        break;
        }
    }
    return true;
}
#undef ADD_KEY_EVENT

internal void
lin_show_offscreen_buffer(Lin_Window *window)
{
    Lin_Offscreen_Buffer *offscreen_buffer = &window->offscreen_buffer;
    u32 width   = offscreen_buffer->width;
    u32 height  = offscreen_buffer->height;
    u32 *pixels = offscreen_buffer->pixels;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, window->offscreen_buffer.gl_texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f,  1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);

    glXSwapBuffers(g_display, window->xid);
}

