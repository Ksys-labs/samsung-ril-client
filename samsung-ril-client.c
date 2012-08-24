/**
 * Samsung RIL Client (Samsung RIL Socket Client-side implementation)
 *
 * Copyright (C) 2011 Paul Kocialkowski <contact@paulk.fr>
 * Copyright (C) 2012 Alexander Tarasikov <alexander.tarasikov@gmail.com>
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

#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cutils/sockets.h>

#define LOG_TAG "SRS-Client"
#include <cutils/log.h>

#include <secril-client.h>
#include <samsung-ril-socket.h>

/* for compatibility with both pre- and post- JellyBean */
#ifndef ALOGE
	#define ALOGE LOGE
#endif

#ifndef ALOGD
	#define ALOGD LOGD
#endif

int srs_send_message(int *p_client_fd, struct srs_message *message)
{
	int client_fd = *p_client_fd;
	fd_set fds;

	struct srs_header header;
	void *data;

	int rc;

	header.length = message->data_len + sizeof(header);
	header.group = SRS_GROUP(message->command);
	header.index = SRS_INDEX(message->command);

	data = malloc(header.length);
	memset(data, 0, header.length);

	memcpy(data, &header, sizeof(header));
	memcpy((void *) (data + sizeof(header)), message->data, message->data_len);

	FD_ZERO(&fds);
	FD_SET(client_fd, &fds);

	// We can't rely on select RC
	select(client_fd + 1, NULL, &fds, NULL, NULL);

	rc = write(client_fd, data, header.length);

	free(data);

	return rc;
}

int srs_send(int *p_client_fd, unsigned short command, void *data, int data_len)
{
	struct srs_message message;
	int rc;

	ALOGE("%s", __func__);

	message.command = command;
	message.data = data;
	message.data_len = data_len;

	rc = srs_send_message(p_client_fd, &message);

	return rc;
}

int srs_recv_timed(int *p_client_fd, struct srs_message *message, long sec, long usec)
{
	void *raw_data = malloc(SRS_DATA_MAX_SIZE);
	struct srs_header *header;
	int rc;

	int client_fd = *p_client_fd;
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

int srs_recv(int *p_client_fd, struct srs_message *message)
{
	return srs_recv_timed(p_client_fd, message, 0, 0);
}

int srs_ping(int *p_client_fd)
{
	int caffe_w = SRS_CONTROL_CAFFE;
	int caffe_r = 0;	
	int rc = 0;

	struct srs_message message;

	rc = srs_send(p_client_fd, SRS_CONTROL_PING, &caffe_w, sizeof(caffe_w));

	if(rc < 0) {
		return -1;
	}

	rc = srs_recv_timed(p_client_fd, &message, 0, 300);

	if(rc < 0) {
		return -1;
	}

	if(message.data == NULL) 
		return -1;

	caffe_r = *((int *) message.data);

	if(caffe_r == SRS_CONTROL_CAFFE) {
		ALOGD("Caffe is ready!");
		rc = 0;
	} else {
		ALOGE("Caffe went wrong!");
		rc = -1;
	}

	free(message.data);
	return rc;
}

int OpenClient_RILD(void)
{
	int *client_fd_p = NULL;

	ALOGE("%s", __func__);

	signal(SIGPIPE, SIG_IGN);

	client_fd_p = malloc(sizeof(int));
	if (!client_fd_p) {
		ALOGE("%s: failed to allocate memory for the client fd", __func__);
		return 0;
	}
	ALOGE("%s: client fd pointer %x", __func__, client_fd_p);
	*client_fd_p = -1;

	return client_fd_p;
}

int Connect_RILD(int *pfd)
{
	int t = 0;
	int fd = -1;
	int rc;
	int *client_fd_p = pfd;

	ALOGE("%s", __func__);

socket_connect:
	while(t < 5) {
		fd = socket_local_client(SRS_SOCKET_NAME, ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
		ALOGE("%s: fd %d, errno %d, err %s", __func__, fd, errno, strerror(errno));

		if(fd >= 0)
			break;

		ALOGE("Socket creation to RIL failed: trying another time");

		t++;
		usleep(300);
	}

	if(fd < 0) {
		ALOGE("Socket creation to RIL failed too many times");
		return RIL_CLIENT_ERR_CONNECT;
	}

	*client_fd_p = fd;

	ALOGE("Socket creation done, sending ping");

	rc = srs_ping(pfd);

	if(rc < 0) {
		ALOGE("Ping failed!");
		goto socket_connect;
	} else {
		ALOGD("Ping went alright");
	}

	return RIL_CLIENT_ERR_SUCCESS;
}

int Disconnect_RILD(int *p_client_fd)
{
	int client_fd = *p_client_fd;

	ALOGE("%s", __func__);

	close(client_fd);

	return RIL_CLIENT_ERR_SUCCESS;
}

int CloseClient_RILD(int *pfd)
{
	ALOGE("%s", __func__);
	free(pfd);

	return RIL_CLIENT_ERR_SUCCESS;
}

int isConnected_RILD(int *pfd)
{
	int client_fd;
	int rc;

	ALOGE("%s", __func__);
	ALOGE("%s: pfd %x", __func__, pfd);

	client_fd = *pfd;
	if(client_fd < 0) {
		return 0;
	}

	rc = srs_ping(pfd);

	if(rc < 0) {
		ALOGE("Ping failed!");
		close(client_fd);

		return 0;
	} else {
		ALOGD("Ping went alright");
	}

	return 1;
}

int SetCallVolume(int *pfd, SoundType type, int vol_level)
{
	struct srs_snd_call_volume call_volume;

	ALOGD("Asking call volume");

	call_volume.type = (enum srs_snd_type) type;
	call_volume.volume = vol_level;	

	srs_send(pfd, SRS_SND_SET_CALL_VOLUME, (void *) &call_volume, sizeof(call_volume));

	return RIL_CLIENT_ERR_SUCCESS;
}


int SetCallAudioPath(int *pfd, AudioPath path)
{
	srs_send(pfd, SRS_SND_SET_CALL_AUDIO_PATH, (void *) &path, sizeof(enum srs_snd_path));

	ALOGD("Asking audio path");

	return RIL_CLIENT_ERR_SUCCESS;
}

int SetCallClockSync(int *pfd, SoundClockCondition condition)
{
	unsigned char data = condition;

	ALOGD("Asking clock sync");

	srs_send(pfd, SRS_SND_SET_CALL_CLOCK_SYNC, &data, sizeof(data));

	return RIL_CLIENT_ERR_SUCCESS;
}

//the functions below are defined to make the proprietary GPS and NFC
//libraries happy and we'll keep them until these libs are reimplemented
int RegisterUnsolicitedHandler(int *p_client_fd, uint32_t id,
	RilOnUnsolicited handler)
{
	//do nothing for now
	return 0;
}

int RegisterRequestCompleteHandler(int *p_client_fd, uint32_t id,
	RilOnComplete handler) {
	ALOGE("%s: %x %x %x", __func__, p_client_fd, id, handler);
	//do nothing for now
	return 0;
}

int RegisterErrorCallback(int *p_client_fd, RilOnError cb, void *data)
{
	ALOGE("%s: %x %x %x", __func__, p_client_fd, cb, data);
	//do nothing for now
	return 0;
}

int InvokeOemRequestHookRaw(int *p_client_fd, char *data, size_t len)
{
	ALOGE("%s: %x %x %x", __func__, p_client_fd, data, len);
	//do nothing for now
	return 0;
}
