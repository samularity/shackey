
#include "ssh.h"
/* ssh Client Example

	 This example code is in the Public Domain (or CC0 licensed, at your option.)

	 Unless required by applicable law or agreed to in writing, this
	 software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	 CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "netdb.h" // gethostbyname

#include "libssh2_config.h"
#include <libssh2.h>
#include <libssh2_sftp.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "ssh.h"
#include "esp_timer.h"

static const char *TAG = "SSH";

extern EventGroupHandle_t xEventGroup;
extern int TASK_FINISH_BIT;


static int waitsocket(int socket_fd, LIBSSH2_SESSION *session)
{
	struct timeval timeout;
	int rc;
	fd_set fd;
	fd_set *writefd = NULL;
	fd_set *readfd = NULL;
	int dir;

	timeout.tv_sec = 10;
	timeout.tv_usec = 0;

	FD_ZERO(&fd);

	FD_SET(socket_fd, &fd);

	/* now make sure we wait in the correct direction */
	dir = libssh2_session_block_directions(session);

	if(dir & LIBSSH2_SESSION_BLOCK_INBOUND)
		readfd = &fd;

	if(dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
		writefd = &fd;

	rc = select(socket_fd + 1, readfd, writefd, NULL, &timeout);

	return rc;
}

#define BUFSIZE 3200

void ssh_task(void *pvParameters)
{
	sshParameter *cfg = (sshParameter *)pvParameters;
	ESP_LOGI(TAG, "SSH task_parameter:\n\t%s@%s:%u\n\tcmd:%s", cfg->username,cfg->hostname,cfg->port, cfg->cmd);

	// SSH Staff
	int sock;
	struct sockaddr_in sin;
	LIBSSH2_SESSION *session;
	LIBSSH2_CHANNEL *channel;

	ESP_LOGI(TAG, "libssh2_version is %s", LIBSSH2_VERSION);
	

	int rc = libssh2_init(0);


	if(rc) {
		ESP_LOGE(TAG, "libssh2 initialization failed (%d)", rc);
		while(1) { vTaskDelay(1); }
	}

	ESP_LOGI(TAG, "CONFIG_SSH_HOST=%s", cfg->hostname);
	ESP_LOGI(TAG, "CONFIG_SSH_PORT=%d", cfg->port );
	sin.sin_family = AF_INET;
	//sin.sin_port = htons(22);
	sin.sin_port = htons(cfg->port );
	sin.sin_addr.s_addr = inet_addr(cfg->hostname);
	ESP_LOGI(TAG, "sin.sin_addr.s_addr=%lx", sin.sin_addr.s_addr);
	if (sin.sin_addr.s_addr == 0xffffffff) {
		struct hostent *hp;
		hp = gethostbyname(cfg->hostname);
		if (hp == NULL) {
			ESP_LOGE(TAG, "gethostbyname fail %s", cfg->hostname);
			while(1) { vTaskDelay(1); }
		}
		struct ip4_addr *ip4_addr;
		ip4_addr = (struct ip4_addr *)hp->h_addr;
		sin.sin_addr.s_addr = ip4_addr->addr;
		ESP_LOGI(TAG, "sin.sin_addr.s_addr=%lx", sin.sin_addr.s_addr);
	}


	sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock == -1) {
		ESP_LOGE(TAG, "failed to create socket!");
		while(1) { vTaskDelay(1); }
	}


	if(connect(sock, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in)) != 0) {
		ESP_LOGE(TAG, "failed to connect!");
		while(1) { vTaskDelay(1); }
	}


	/* Create a session instance */
	session = libssh2_session_init();
	if(!session) {
		ESP_LOGE(TAG, "failed to session init");
		while(1) { vTaskDelay(1); }
	}

	/* ... start it up. This will trade welcome banners, exchange keys,
	 * and setup crypto, compression, and MAC layers
	 */
	rc = libssh2_session_handshake(session, sock);
	if(rc) {
		ESP_LOGE(TAG, "Failure establishing SSH session: %d", rc);
		while(1) { vTaskDelay(1); }
	}

	// We could authenticate via password 
	if(libssh2_userauth_password(session, "root", "")) {
		ESP_LOGE(TAG, "Authentication by password failed.");
		ESP_LOGE(TAG, "Authentication username : [%s].", "root");
		while(1) { vTaskDelay(1); }
	}

/*
	if(libssh2_userauth_publickey_fromfile(session, cfg->username , cfg->publickey, cfg->privatekey, NULL)) {
		ESP_LOGE(TAG, "Authentication by privatekey failed.");
		ESP_LOGE(TAG, "Authentication username : [%s].", cfg->username );
		while(1) { vTaskDelay(1); }
	}
*/

	ESP_LOGI(TAG, "auth ok");
	libssh2_trace(session, LIBSSH2_TRACE_SOCKET);

	/* Exec non-blocking on the remove host */
	while((channel = libssh2_channel_open_session(session)) == NULL &&
		libssh2_session_last_error(session, NULL, NULL, 0) ==
		LIBSSH2_ERROR_EAGAIN) {
		waitsocket(sock, session);
	}
		ESP_LOGI(TAG, "channel ok");
	if(channel == NULL) {
		ESP_LOGE(TAG, "libssh2_channel_open_session failed.");
		while(1) { vTaskDelay(1); }
	}

	while((rc = libssh2_channel_exec(channel, cfg->cmd)) == LIBSSH2_ERROR_EAGAIN)
	waitsocket(sock, session);
	if(rc != 0) {
		ESP_LOGE(TAG, "libssh2_channel_exec failed: %d", rc);
		while(1) { vTaskDelay(1); }
	ESP_LOGI(TAG, "exec ok");
	}

/*

	if ( '\0' == cfg->cmd[0]) {
		ESP_LOGI(TAG, "exec");
		while((rc = libssh2_channel_exec(channel, cfg->cmd)) == LIBSSH2_ERROR_EAGAIN)
		waitsocket(sock, session);
		if(rc != 0) {
			ESP_LOGE(TAG, "libssh2_channel_exec failed: %d", rc);
			while(1) { vTaskDelay(1); }
		}
	}
	else{ //no cmd just open a shell
		rc = libssh2_channel_shell (channel);
		if (rc < 0) {
			ESP_LOGE(TAG, "ssh_channel_request_shell failed: %d", rc);
			while(1) { vTaskDelay(1); }
		}
	}

	ESP_LOGI(TAG, "request ok");
	int bytecount = 0;
	for(;;) {
		// loop until we block 
		int rc;
		do {
			char buffer[128];
			rc = libssh2_channel_read(channel, buffer, sizeof(buffer) );
			if(rc > 0) {
				int i;
				bytecount += rc;
				//fprintf(stderr, "We read:\n");
				for(i = 0; i < rc; ++i)
					//fputc(buffer[i], stderr);
					fputc(buffer[i], stdout);
				//fprintf(stderr, "\n");
			}
			else if(rc < 0) {
					/ no need to output this for the EAGAIN case 
					ESP_LOGI(TAG, "libssh2_channel_read returned %d", rc);
					//while(1) { vTaskDelay(1); }
			}
			if (rc==0){
				break;
			}
		}
		while(rc > 0);

		// this is due to blocking that would occur otherwise so we loop on  this condition 
		if(rc == LIBSSH2_ERROR_EAGAIN) {
			waitsocket(sock, session);
		}
		else
			break;
	} // end for
	printf("\n");

*/

	int exitcode = 127;
	char *exitsignal = (char *)"none";
	while((rc = libssh2_channel_close(channel)) == LIBSSH2_ERROR_EAGAIN)
	waitsocket(sock, session);
	if(rc == 0) {
		exitcode = libssh2_channel_get_exit_status(channel);
		libssh2_channel_get_exit_signal(channel, &exitsignal,
										NULL, NULL, NULL, NULL, NULL);
	} else {
		ESP_LOGE(TAG, "libssh2_channel_close failed: %d", rc);
		while(1) { vTaskDelay(1); }
	}
/*
	if(exitsignal)
		ESP_LOGI(TAG, "EXIT: %d, SIGNAL: %s, bytecount: %d", exitcode, exitsignal, bytecount);
		//ESP_LOGI(TAG, "Got signal: %s", exitsignal);

	else
		ESP_LOGI(TAG, "EXIT: %d, bytecount: %d", exitcode, bytecount);
*/
	libssh2_channel_free(channel);
	channel = NULL;

	// Close a session
	libssh2_session_disconnect(session, "Normal Shutdown, Thank you for playing");
	libssh2_session_free(session);

	// Close socket
	close(sock);
	ESP_LOGI(TAG, "[%s] done", cfg->cmd );

	libssh2_exit();

	xEventGroupSetBits( xEventGroup, TASK_FINISH_BIT );
	vTaskDelete( NULL );
}