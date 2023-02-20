#ifndef _SSH_H_
#define _SSH_H_

#include <stdint.h>
#include "pwrmgr.h"

struct sshParameter {
    char username[64];
    char hostname[64];
    uint16_t port;
    char password[64];
    char publickey[64];
	char privatekey[64];
    char cmd[256];
};

void ssh_task(void *pvParameters);

#endif /* _SSH_H_ */