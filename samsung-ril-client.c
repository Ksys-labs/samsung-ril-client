#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <cutils/sockets.h>

#define LOG_TAG "SRS-Client"
#include <cutils/log.h>

#include <secril-client.h>
#include <samsung-ril-socket.h>

int msg_id_g = 0x00;

int msg_id_get(void)
{
	if(msg_id_g == 0xfe)
		msg_id_g = 0x00;

	msg_id_g++;

	return msg_id_g;
}

int srs_send_message(HRilClient client, struct srs_message *message)
{
	int client_fd = *((int *) client->prv);
	fd_set fds;

	struct srs_header header;
	void *data;

	header.length = message->data_len + sizeof(header);
	header.group = SRS_GROUP(message->command);
	header.index = SRS_INDEX(message->command);
	header.msg_id = message->msg_id;

	data = malloc(header.length);
	memset(data, 0, header.length);

	memcpy(data, &header, sizeof(header));
	memcpy((void *) (data + sizeof(header)), message->data, message->data_len);

	FD_ZERO(&fds);
	FD_SET(client_fd, &fds);

	select(client_fd + 1, NULL, &fds, NULL, NULL);
	write(client_fd, data, header.length);

	//FIXME: can we free?
	//FIXME: add checks

	return 0;
}

int srs_send(HRilClient client, unsigned short command, void *data, int data_len, unsigned msg_id)
{
	struct srs_message message;
	int rc;

	LOGE("%s", __func__);

	message.command = command;
	message.data = data;
	message.data_len = data_len;
	message.msg_id = msg_id;

	rc = srs_send_message(client, &message);

	return rc;
}

HRilClient OpenClient_RILD(void)
{
	HRilClient client;
	int *client_fd_p = NULL;

	LOGE("%s", __func__);

	client = malloc(sizeof(struct RilClient));
	client->prv = malloc(sizeof(int));
	client_fd_p = (int *) client->prv;
	*client_fd_p = -1;

	return client;
}

int Connect_RILD(HRilClient client)
{
	int t = 0;
	int fd = -1;
	int *client_fd_p = (int *) client->prv;

	LOGE("%s", __func__);

	while(t < 5) {
		fd = socket_local_client(SRS_SOCKET_NAME, ANDROID_SOCKET_NAMESPACE_RESERVED, SOCK_STREAM);

		if(fd > 0)
			break;

		LOGE("Socket creation to RIL failed: trying another time in a sec");

		t++;
		usleep(300);
	}

	if(fd < 0) {
		LOGE("Socket creation to RIL failed too many times");
		return RIL_CLIENT_ERR_CONNECT;
	}

	*client_fd_p = fd;

	LOGE("Socket creation done and sent HELO");
//	srs_send(client, SRS_CONTROL_GET_HELO, NULL, 0, msg_id_get());

	return RIL_CLIENT_ERR_SUCCESS;
}

int Disconnect_RILD(HRilClient client)
{
	int client_fd = *((int *) client->prv);

	LOGE("%s", __func__);

	//FIXME: send a message telling we are leaving!

	close(client_fd);

	return RIL_CLIENT_ERR_SUCCESS;
}

int CloseClient_RILD(HRilClient client)
{
	int *client_fd_p = (int *) client->prv;

	LOGE("%s", __func__);

	if(client_fd_p != NULL)
		free(client_fd_p);

	free(client);

	return RIL_CLIENT_ERR_SUCCESS;
}

int isConnected_RILD(HRilClient client)
{
	int client_fd = *((int *) client->prv);

	LOGE("%s", __func__);

	if(client_fd < 0) {
		return 0;
	}

	return 1;
}

int SetCallVolume(HRilClient client, SoundType type, int vol_level)
{
	LOGE("Asking call volume");
	return RIL_CLIENT_ERR_SUCCESS;
}

/**
 * Set external sound device path for noise reduction.
 */
int SetCallAudioPath(HRilClient client, AudioPath path)
{
	LOGE("Asking audio path!");
	return RIL_CLIENT_ERR_SUCCESS;
}

/**
 * Set modem clock to master or slave.
 */
int SetCallClockSync(HRilClient client, SoundClockCondition condition)
{
	LOGE("Asking clock sync!");
	unsigned char data = condition;

	srs_send(client, SRS_SND_SET_CALL_CLOCK_SYNC, &data, sizeof(data), msg_id_get());

	return RIL_CLIENT_ERR_SUCCESS;
}
