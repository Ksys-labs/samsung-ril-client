/**
 * Samsung RIL Client (Samsung RIL Socket Client-side implementation)
 *
 * Copyright (C) 2011 Paul Kocialkowski <contact@paulk.fr>
 *
 * samsung-ril-client is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * samsung-ril-client is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with samsung-ril-client.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

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

int srs_send_message(HRilClient client, struct srs_message *message)
{
	int client_fd = *((int *) client->prv);
	fd_set fds;

	struct srs_header header;
	void *data;

	header.length = message->data_len + sizeof(header);
	header.group = SRS_GROUP(message->command);
	header.index = SRS_INDEX(message->command);

	data = malloc(header.length);
	memset(data, 0, header.length);

	memcpy(data, &header, sizeof(header));
	memcpy((void *) (data + sizeof(header)), message->data, message->data_len);

	FD_ZERO(&fds);
	FD_SET(client_fd, &fds);

	select(client_fd + 1, NULL, &fds, NULL, NULL);
	write(client_fd, data, header.length);

	free(data);

	return 0;
}

int srs_send(HRilClient client, unsigned short command, void *data, int data_len)
{
	struct srs_message message;
	int rc;

	LOGE("%s", __func__);

	message.command = command;
	message.data = data;
	message.data_len = data_len;

	rc = srs_send_message(client, &message);

	return rc;
}

int srs_recv_timed(HRilClient client, struct srs_message *message, long sec, long usec)
{
	void *raw_data = malloc(SRS_DATA_MAX_SIZE);
	struct srs_header *header;
	int rc;

	int client_fd = *((int *) client->prv);
	struct timeval timeout;
	fd_set fds;

	FD_ZERO(&fds);
	FD_SET(client_fd, &fds);

	timeout.tv_sec = sec;
	timeout.tv_usec = usec;

	select(client_fd + 1, &fds, NULL, NULL, &timeout);

	rc = read(client_fd, raw_data, SRS_DATA_MAX_SIZE);
	if(rc < sizeof(struct srs_header)) {
		return -1;
	}

	header = raw_data;

	message->command = SRS_COMMAND(header);
	message->data_len = header->length - sizeof(struct srs_header);
	message->data = malloc(message->data_len);

	memcpy(message->data, raw_data + sizeof(struct srs_header), message->data_len);

	free(raw_data);

	return 0;
}

int srs_recv(HRilClient client, struct srs_message *message)
{
	return srs_recv_timed(client, message, 0, 0);
}

int srs_ping(HRilClient client)
{
	int caffe_w = SRS_CONTROL_CAFFE;
	int caffe_r = 0;	
	int rc = 0;

	struct srs_message message;

	srs_send(client, SRS_CONTROL_PING, &caffe_w, sizeof(caffe_w));
	srs_recv_timed(client, &message, 0, 300);

	if(message.data == NULL) 
		return -1;

	caffe_r = *((int *) message.data);

	if(caffe_r == SRS_CONTROL_CAFFE) {
		LOGD("Caffe is ready!");
		rc = 0;
	} else {
		LOGE("Caffe went wrong!");
		rc = -1;
	}

	free(message.data);
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
	int rc;
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

	LOGE("Socket creation done, sending ping");

	rc = srs_ping(client);

	if(rc < 0) {
		LOGE("Ping failed!");
	} else {
		LOGD("Ping went alright");
	}

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

	struct srs_snd_call_volume call_volume;

	call_volume.type = (enum srs_snd_type) type;
	call_volume.volume = vol_level;	

	srs_send(client, SRS_SND_SET_CALL_VOLUME, (void *) &call_volume, sizeof(call_volume));

	return RIL_CLIENT_ERR_SUCCESS;
}


int SetCallAudioPath(HRilClient client, AudioPath path)
{
	LOGE("Asking audio path!");

	srs_send(client, SRS_SND_SET_CALL_AUDIO_PATH, (void *) &path, sizeof(enum srs_snd_path));

	return RIL_CLIENT_ERR_SUCCESS;
}

int SetCallClockSync(HRilClient client, SoundClockCondition condition)
{
	LOGE("Asking clock sync!");
	unsigned char data = condition;

	srs_send(client, SRS_SND_SET_CALL_CLOCK_SYNC, &data, sizeof(data));

	return RIL_CLIENT_ERR_SUCCESS;
}
