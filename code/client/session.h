typedef struct {
    char username[32];
    char content[256];
} Chat_Message;

typedef struct {
    u32 cnt_max;
    u32 cnt;
    u32 base;
    Chat_Message *messages;
} Chat_History;

typedef struct {
    u32 cnt_users_max;
    u32 cnt_users;
    char (*users)[32];
} Userlist;

typedef struct {
    s32 cursor;
    char *buff;
} Prompt;

typedef struct {
    Userlist userlist;
    Chat_History history;
    Prompt prompt;
} Session;
