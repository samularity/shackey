#ifndef _SSH_H_
#define _SSH_H_

#include <libssh/libssh.h>
#include "libssh/priv.h"
#include "pwrmgr.h"

#define EXECTEST

//#define LOGLVL (0xffffU)
#define LOGLVL (0x0U)


int ex_main(doorCMD_t shackstate);

#endif /* _SSH_H_ */