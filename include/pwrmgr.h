#ifndef _PWRMGR_H_
#define _PWRMGR_H_

typedef enum
{
  CMD_UNKNOWN,
  CMD_OPEN,
  CMD_CLOSE,
} doorCMD_t; 

doorCMD_t wakeSelector();
void enterDeepsleep();
void printflashinfo();

#endif /* _PWRMGR_H_ */