// Todo: @CLEANUP rect sizes/positions
// - let the individual rects (userlist, prompt, history) decide their size
// - don't do that for them in draw_session()
//
// -> Individual rects will always be correct.
//    At the moment they might be bigger than there is screen space available.
//    This can lead to incorrectly drawn borders for example.

internal void
init_drawn_history_text(char *text, const char *username, const char *message)
{
    text[0] = '<';
    u16 username_len = strlen(username);
    memcpy(text+1, username, username_len);
    text[1+username_len] = '>';
    text[2+username_len] = ' ';
    strcpy(text+username_len+3, message);
}

internal void
draw_message(Florilia_Offscreen_Buffer *screen, Font *font, Chat_Message *message, Rect *rect_messages)
{
    s32 margin = font->y_advance/4;

    Rect rect_message = {
        rect_messages->x0 + margin,
        rect_messages->y0 + margin,
        rect_messages->x1,
        rect_messages->y1
    };

    int text_width;


    // time
    struct tm *time_local = localtime(&message->time_posix);
    char time_str[8];
    sprintf(time_str, "[%02d:%02d]", time_local->tm_hour, time_local->tm_min);

    text_width = get_text_width(time_str, font);
    draw_text(screen, font, time_str, -1, rect_message.x0, rect_message.y0);
    rect_message.x0 += text_width;


    // separator
    rect_message.x0 += font->x_advance;


    // username
    draw_text(screen, font, "<", -1, rect_message.x0, rect_message.y0);
    rect_message.x0 += font->x_advance;

    text_width = get_text_width(message->username, font);
    draw_text(screen, font, message->username, -1, rect_message.x0, rect_message.y0);
    rect_message.x0 += text_width;

    draw_text(screen, font, ">", -1, rect_message.x0, rect_message.y0);
    rect_message.x0 += font->x_advance * 2;


    // message
    draw_text(screen, font, message->content, -1, rect_message.x0, rect_message.y0);


    rect_messages->y0 += font->y_advance;
}

internal void
draw_history(Florilia_Offscreen_Buffer *screen, Chat_History *history, Rect rect_messages, Font *font)
{
    s32 border_size = font->y_advance / 8;
    V3 border_color = {{0, 0, 0}};
    draw_border(screen, rect_messages, border_size, border_color);

    u32 base = history->base;
    u32 cnt  = history->cnt;
    if (cnt > 0)
    {
        u32 below = base == 0 ? 0 : base-1;
        u32 above = cnt;

        while (below < base && rect_messages.y0 <= rect_messages.y1)
        {
            Chat_Message *message = &history->messages[below];
            draw_message(screen, font, message, &rect_messages);
            below--;
        }

        while (above > base && rect_messages.y0 <= rect_messages.y1)
        {
            Chat_Message *message = &history->messages[above-1];
            draw_message(screen, font, message, &rect_messages);
            above--;
        }
    }
}

internal void
draw_prompt(Florilia_Offscreen_Buffer *screen, Prompt *prompt, Rect rect, Font *font)
{
    s32 border_size = font->y_advance / 8;
    V3 border_color = {{0, 220, 0}};
    draw_border(screen, rect, border_size, border_color);

    s32 x = rect.x0 + font->y_advance / 2;
    s32 y = rect.y0 + (rect.y1 - rect.y0 + 1 - font->y_advance) / 2; // center
    draw_text(screen, font, prompt->buff, prompt->cursor, x, y);
}

internal void
draw_userlist(Florilia_Offscreen_Buffer *screen, Userlist *userlist, Rect rect, Font *font)
{
    s32 rect_width = rect.x1 - rect.x0  + 1;

    // border
    s32 border_size = font->y_advance / 8;
    V3 border_color = {{0, 0, 0}};
    draw_border(screen, rect, border_size, border_color);

    // title
    s32 title_margin = font->y_advance / 8;

    s32 rect_title_height = font->y_advance + border_size*2 + title_margin*2;
    Rect rect_title = {rect.x0, rect.y1-rect_title_height-1, rect.x1, rect.y1};
    draw_border(screen, rect_title, border_size, border_color);

    const char *title = "Users";
    s32 title_width  = get_text_width(title, font);
    s32 title_x = rect.x0 + (rect_width - title_width)/2; // center
    s32 title_y = rect_title.y0 + title_margin + border_size;
    draw_text(screen, font, title, -1, title_x, title_y);


    // users
    s32 ygap = font->y_advance / 2;
    s32 x = font->y_advance / 2;
    s32 y = title_y - font->y_advance - ygap - 1;

    for (s32 it = 0; it < userlist->cnt_users; it++)
    {
        if (y < 0)
        {
            break;
        }

        draw_text(screen, font, userlist->users[it], -1, x, y);
        y -= font->y_advance +  ygap;
    }
}

internal void
draw_session(Florilia_Offscreen_Buffer *screen, Session *session, Font *font)
{
    V3 bg_color = {{200, 150, 100}};
    Rect bg_rect = {0, 0, screen->width-1, screen->height-1};
    draw_rect(screen, bg_rect, bg_color);


    s32 userlist_width = font->y_advance * 10;
    s32 prompt_height  = font->y_advance * 2;
    s32 right_side_width = screen->width - userlist_width;


    Rect rect_userlist = {0, 0, userlist_width-1, screen->height-1};
    Rect rect_prompt   = {userlist_width, 0, userlist_width + right_side_width-1, prompt_height-1};
    Rect rect_history  = {userlist_width, prompt_height, userlist_width + right_side_width-1, screen->height - 1};

    draw_userlist(screen, &session->userlist, rect_userlist, font);
    draw_prompt(screen, &session->prompt, rect_prompt, font);
    draw_history(screen, &session->history, rect_history, font);
}

internal void
add_message(Chat_History *history, char *name, u16 name_len, char *msg, u16 msg_len, s64 time_posix)
{
    // reserve message
    u32 index;
    u32 cnt  = history->cnt;
    u32 base = history->base;
    u32 max_cnt = history->cnt_max;
    if (cnt == 0)
    {
        index = 0;
        history->cnt += 1;
    }
    else if (cnt < max_cnt)
    {
        index = cnt;
        history->cnt += 1;
    }
    else
    {
        index = base;
        if (index == max_cnt-1) history->base = 0;
        else                    history->base += 1;
    }


    // update data
    Chat_Message *message = &history->messages[index];

    memcpy(message->username, name, name_len);
    message->username[name_len] = '\0';

    memcpy(message->content, msg, msg_len);
    message->content[msg_len] = '\0';

    message->time_posix = time_posix;
} 

internal void
remove_user(Userlist *userlist, const char *name, u16 name_len)
{
    u16 cnt_users = userlist->cnt_users;
    if (cnt_users == 0)
    {
        return;
    }

    for (u16 it = 0; it < cnt_users; it++)
    {
        char *user = &userlist->users[it][0];
        u16 user_size = sizeof(userlist->users[0]);
        u16 user_len = strlen(user);

        if (user_len == name_len && memcmp(user, name, name_len) == 0)
        {
            u16 users_left = cnt_users - it;
            memmove(user, user+user_size, users_left*user_size);
            memset(user + users_left*user_size, 0, user_size);

            userlist->cnt_users -= 1;
            break;
        }
    }

    // debug print
    char username[32];
    memcpy(username, name, name_len);
    username[name_len] = '\0';
    printf("session: removed username %s\n", username);
}

internal void
add_user(Userlist *userlist, const char *name, u16 name_len)
{
    // server should not allow this case
    if (userlist->cnt_users == userlist->cnt_users_max)
    {
        printf("session: userlist full when trying to add %s (ignoring)\n", name);
        return;
    }

    char *dest = &userlist->users[userlist->cnt_users][0];
    memcpy(dest, name, name_len);
    dest[name_len] = '\0';

    userlist->cnt_users += 1;
    printf("session: added username %s\n", dest);
}



internal void
session_process_event(Session *session, Florilia_Event *event, Network_Manager *netman)
{
    if (event->type == FLORILIA_EVENT_KEY)
    {
        Prompt *prompt = &session->prompt;
        char c = event->key.c;

        if (c == '\r')
        {
            // prepare package
            const char *msg = prompt->buff;
            u16 msg_len = strlen(msg);

            u32 desc_size = sizeof(FNC_Chat_Message);
            u32 package_size = desc_size + msg_len;

            u8 package[package_size];
            FNC_Chat_Message *desc = (FNC_Chat_Message*)package;

            desc->type = FNC_CHAT_MESSAGE;
            desc->message_len = msg_len;
            memcpy(package+desc_size, msg, msg_len);


            // send package
            netman_send(netman, package, package_size);


            // reset prompt
            prompt->buff[0] = '\0';
            prompt->cursor = 0;
        }
        else if ((c >= ' ' && c <= '~') ||
                 c == 8 || c == 127)
        {
            edit_text(prompt->buff, FN_MAX_MSG_LEN+1, event->key.c, &prompt->cursor);
        }
        else if (c == FLORILIA_KEY_LEFT)
        {
            move_cursor_left(prompt->buff, &prompt->cursor);
        }
        else if (c == FLORILIA_KEY_RIGHT)
        {
            move_cursor_right(prompt->buff, &prompt->cursor);
        }
    }
}

internal s32
session_process_netman_buff(Session *session, Network_Manager *netman)
{
    if (netman->bytes_filled < sizeof(u16))
    {
        return 0;
    }

    u16 type = *(u16*)netman->mem;
    if (type == FNS_CHAT_MESSAGE)
    {
        // check package validity
        u32 max_username_len = sizeof(session->history.messages[0].username)-1;
        u32 max_content_len  = sizeof(session->history.messages[0].content)-1;

        FNS_Chat_Message *desc = (FNS_Chat_Message*)netman->mem;
        u32 desc_size = sizeof(FNS_Chat_Message);

        if (netman->bytes_filled < desc_size ||
            desc->sender_len > max_username_len ||
            desc->message_len > max_content_len ||
            netman->bytes_filled < desc_size + desc->message_len + desc->sender_len)
        {
            printf("session error: FNS_Chat_Message invalid\n");
            return 0;
        }


        // add message
        char *name = (char*)netman->mem + desc_size;
        char *msg  = name + desc->sender_len;
        u16 name_len = desc->sender_len;
        u16 msg_len = desc->message_len;
        s64 time_posix = desc->time_posix;


        add_message(&session->history, name, name_len, msg, msg_len, time_posix);

        s32 size_processed = desc_size + desc->message_len + desc->sender_len;
        netman_release_size(netman, size_processed);
        return size_processed;
    }
    else if (type == FNS_USER_STATUS)
    {
        FNS_User_Status *desc = (FNS_User_Status*)netman->mem;
        u32 desc_size = sizeof(FNS_User_Status);

        if (netman->bytes_filled < desc_size ||
            netman->bytes_filled < desc_size + desc->name_len) {
            printf("session error: invalid message for FNS_USER_CONNECT\n");
            return 0;
        }

        if (desc->status == FNS_USER_STATUS_ONLINE)
        {
            const char *name = (const char*)(desc+1);
            add_user(&session->userlist, name, desc->name_len);
        }
        else if (type == FNS_USER_STATUS_OFFLINE)
        {
            const char *name = (const char*)(desc+1);
            remove_user(&session->userlist, name, desc->name_len);
        }

        s32 size_processed = desc_size + desc->name_len;
        netman_release_size(netman, size_processed);
        return size_processed;
    }
    return 0;
}

internal void
reset_session(Session *session)
{
    Prompt *prompt = &session->prompt;
    prompt->cursor = 0;
    prompt->buff[0] = '\0';

    Userlist *userlist = &session->userlist;
    userlist->cnt_users = 0;

    Chat_History *history = &session->history;
    history->cnt  = 0;
    history->base = 0;
}

internal void
init_session(Session *session, Memory_Arena *arena)
{
    Prompt *prompt = &session->prompt;
    prompt->buff = push_size(arena, FN_MAX_MSG_LEN+1);

    Userlist *userlist = &session->userlist;
    userlist->cnt_users_max = FN_MAX_USERS;
    userlist->users = push_size(arena, FN_MAX_USERS*sizeof(userlist->users[0]));

    Chat_History *history = &session->history;
    history->messages = push_array(arena, Chat_Message, 512);
    history->cnt_max = 512;
}
