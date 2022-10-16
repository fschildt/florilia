static const char *g_default_warning = "Enter: login, Tab: change selection";

internal void
draw_login(Florilia_Offscreen_Buffer *screen, Login *login, Font *font)
{
    s32 screen_width  = screen->width;
    s32 screen_height = screen->height;

    // background
    V3 bg_color = {{100, 50, 50}};
    Rect bg_rect = {0, 0, screen_width-1, screen_height-1};
    draw_rect(screen, bg_rect, bg_color);


    // settings
    s32 title_height = font->y_advance * 3;
    s32 textin_border_size = font->y_advance / 16;
    s32 textin_margin      = font->y_advance / 4;
    s32 textin_width       = font->y_advance * 16;
    s32 textin_height      = font->y_advance + 2*(textin_border_size + textin_margin);
    s32 textin_desc_ygap = font->y_advance / 4;

    s32 field_width   = textin_width;
    s32 field_height  = textin_height + font->y_advance + textin_desc_ygap;
    s32 field_ygap    = font->y_advance * 1;

    s32 plane_width  = field_width + 100; // Todo: add margin
    s32 plane_height = 3*(field_height+field_ygap) + title_height + font->y_advance;
    s32 plane_x = (screen_width  - plane_width)  / 2;
    s32 plane_y = (screen_height - plane_height) / 2;

    V3 color_active = {{10, 200, 10}};
    V3 color_inactive = {{40, 40, 40}};
    s32 cursor_username;
    s32 cursor_servername;
    V3 color_username;
    V3 color_servername;
    if (login->sel == Login_Sel_Username)
    {
        cursor_username = login->cursor_username;
        cursor_servername = -1;
        color_username = color_active;
        color_servername = color_inactive;
    }
    else
    {
        cursor_username =  -1;
        cursor_servername = login->cursor_servername;
        color_username = color_inactive;
        color_servername = color_active;
    }


    // main plane
    Rect plane_rect = {plane_x, plane_y, plane_x+plane_width-1, plane_y+plane_height-1};
    V3 plane_color = {{200, 150, 100}};
    draw_rect(screen, plane_rect, plane_color);


    // title
    const char *title = "Welcome to Florilia!";
    s32 title_width = get_text_width(title, font);
    s32 title_x = plane_x + (plane_width - title_width) / 2;
    s32 title_y = plane_rect.y1 - 2*font->y_advance + 1;
    draw_text(screen, font, title, -1, title_x, title_y);



    // coordinates
    s32 x = plane_x + (plane_width - field_width) / 2;
    s32 xoff = font->y_advance / 2; // x offset for the text within a 'textin'
    s32 field_y = title_y - font->y_advance - (field_height + field_ygap);
    s32 yoff_desc = textin_height + textin_desc_ygap;


    // username
    draw_text(screen, font, "username", -1, x, field_y + yoff_desc);
    draw_text(screen, font, login->username, cursor_username, x + xoff, field_y + textin_border_size + textin_margin);
    {
        Rect rect = {x, field_y, x + textin_width-1, field_y + textin_height-1};
        draw_border(screen, rect, textin_border_size, color_username);
    }
    field_y -= field_height + field_ygap;


    // servername
    draw_text(screen, font, "servername", -1, x, field_y + yoff_desc);
    draw_text(screen, font, login->servername, cursor_servername, x + xoff, field_y + textin_border_size + textin_margin);
    {
        Rect rect = {x, field_y, x + textin_width-1, field_y + textin_height-1};
        draw_border(screen, rect, textin_border_size, color_servername);
    }
    field_y -= field_height + field_ygap;



    draw_text(screen, font, login->warning, -1, x, field_y);
}

void request_session(Login *login, Network_Manager *netman)
{
    char *username   = login->username;
    char *servername = login->servername;
    u16 username_len = strlen(username);

    // parse servername (address and port)
    char address[64];
    u16 port = 0;

    char *colon = servername;
    while (*colon != ':' && *colon != '\0')
    {
        colon++;
    }

    if (*colon == ':')
    {
        memcpy(address, servername, colon-servername);
        address[colon-servername] = '\0';
        port = atoi(colon+1);
    }

    if (!port)
    {
        login->status = Login_Status_Failed;
        strcpy(login->warning, "servername format invalid");
        return;
    }


    // connect
    netman->netid = g_platform->connect(address, port);
    if (netman->netid == -1)
    {
        strcpy(login->warning, "could not connect to server");
        login->status = Login_Status_Failed;
        return;
    }


    // send login package
    u32 desc_size = sizeof(FNC_Login);
    u32 package_size = desc_size + username_len;

    u8 package[package_size];
    FNC_Login *desc = (FNC_Login*)package;

    desc->type = FNC_LOGIN;
    desc->name_len = username_len;
    memcpy(package + desc_size, username, username_len);

    netman_send(netman, package, package_size);


    // update state
    login->status = Login_Status_Waiting;
}

s32 login_process_netman_buff(Login *login, Network_Manager *netman)
{
    if (netman->bytes_filled < sizeof(u16))
    {
        return 0;
    }

    u16 type = *(u16*)netman->mem;
    if (type == FNS_LOGIN)
    {
        u32 desc_size = sizeof(FNS_Login);
        if (netman->bytes_filled < desc_size)
        {
            return 0;
        }

        FNS_Login *s_login = (FNS_Login*)netman->mem;
        if (s_login->status == FNS_LOGIN_SUCCESS)
        {
            login->status = Login_Status_Success;
        }
        else
        {
            strcpy(login->warning, "server refused login");
        }
        
        netman_release_size(netman, desc_size);
        return desc_size;
    }
    else
    {
        printf("login error: invalid message received from server\n");
        return 0;
    }
}

internal void
login_process_event(Login *login, Florilia_Event *event, Network_Manager *netman)
{
    // reset warning
    if (login->status == Login_Status_Failed ||
        login->status == Login_Status_Lost)
    {
        login->status = Login_Status_Default;
        strcpy(login->warning, g_default_warning);
    }

    // ignore if waiting
    if (login->status == Login_Status_Waiting)
    {
        return;
    }

    if (event->type == FLORILIA_EVENT_KEY)
    {
        char c = event->key.c;
        if (login->sel == Login_Sel_Username)
        {
            if (c == '\r')
            {
                request_session(login, netman);
            }
            else if (c == '\t')
            {
                login->sel = Login_Sel_Servername;
            }
            else if (c == FLORILIA_KEY_LEFT)
            {
                move_cursor_left(login->username, &login->cursor_username);
            }
            else if (c == FLORILIA_KEY_RIGHT)
            {
                move_cursor_right(login->username, &login->cursor_username);
            }
            else
            {
                edit_text(login->username, sizeof(login->username), c, &login->cursor_username);
            }
        }
        else
        {
            if (c == '\r')
            {
                request_session(login, netman);
            }
            else if (c == '\t')
            {
                login->sel = Login_Sel_Username;
            }
            else if (c == FLORILIA_KEY_LEFT)
            {
                move_cursor_left(login->servername, &login->cursor_servername);
            }
            else if (c == FLORILIA_KEY_RIGHT)
            {
                move_cursor_right(login->servername, &login->cursor_servername);
            }
            else
            {
                edit_text(login->servername, sizeof(login->servername), c, &login->cursor_servername);
            }
        }
    }
}

internal void
init_login(Login *login, Memory_Arena *arena)
{
    login->status = Login_Status_Default;
    login->sel = Login_Sel_Username;
    login->hot = Login_Sel_None;

    login->username[0] = 0;
    strcpy(login->servername, "127.0.0.1:1905");
    strcpy(login->warning, g_default_warning);

    login->cursor_username   = 0;
    login->cursor_servername = strlen(login->servername);
}
