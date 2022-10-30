// Note:
// FN_...   means shared properties
// FNS_...  means package made in server and coming from server
// FNC_...  means package made in client and coming from client


#include "florilia_common.h"


/***********************
 *  shared properties  *
 ***********************/

#define FN_MAX_USERS 512
#define FN_MAX_USERNAME_LEN 15
#define FN_MAX_MSG_LEN 47



/***********************
 *   client packages   *
 ***********************/

#define FNC_LOGIN           0
#define FNC_CHAT_MESSAGE    1

typedef struct {
    u16 type;
    u16 name_len;
} FNC_Login;

typedef struct {
    u16 type;
    u16 message_len;
} FNC_Chat_Message;



/***********************
 *   server packages   *
 ***********************/
 
// types
#define FNS_LOGIN           0
#define FNS_USER_STATUS     1
#define FNS_CHAT_MESSAGE    2

// login status
#define FNS_LOGIN_SUCCESS   0
#define FNS_LOGIN_ERROR     1

// user status
#define FNS_USER_STATUS_ONLINE   0
#define FNS_USER_STATUS_OFFLINE  1

typedef struct {
    u16 type;
    u16 status;
} FNS_Login;

typedef struct {
    u16 type;
    u16 status;
    u16 name_len;
} FNS_User_Status;

typedef struct {
    u16 type;
    u16 sender_len;
    u16 message_len;
    s64 time_posix;
} FNS_Chat_Message;
