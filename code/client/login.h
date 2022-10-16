enum Login_Status {
    Login_Status_Default,
    Login_Status_Waiting,
    Login_Status_Failed,
    Login_Status_Success,
    Login_Status_Lost
};

enum Login_Selection {
    Login_Sel_None,
    Login_Sel_Username,
    Login_Sel_Servername
};

typedef struct {
    u32 status; // enum Login_Status
    u16 sel;   // enum Login_Selection
    u16 hot;   // enum Login_Selection

    s32 cursor_username;
    s32 cursor_servername;

    char username[FN_MAX_USERNAME_LEN+1];
    char servername[32];
    char warning[32];
} Login;
