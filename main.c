#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

#include <linux/joystick.h>

void fail(char* where);

int main (int argc, char** argv) {
	/* Warn user of incorrect usage */
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Find a joystick */
	int joystick_fd;
	{
		char dir_buf[500];
		for (int i = 0; i < 30; i++) {
			sprintf(dir_buf, "/dev/input/js%i", i);
			if ((joystick_fd = open(dir_buf, O_RDONLY)) != -1) break;
		}
	}
	if (joystick_fd < 0) fail("during joystick acquisition");

	/* Create socket */
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) fail("on socket creation");

	/* Acquire host IP */
	struct hostent *server = gethostbyname(argv[1]);
	if (!server) fail("on getting server ip");

	/* Create server output struct and connect */
	struct sockaddr_in serv_addr;
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[2]));
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		fail ("when connecting to server");

	/* Buffer for serialized messages */
	const int buf_len = 64;
	char message_buf[buf_len];

	/* Joystick event struct */
	struct js_event event;
	bzero(&event, sizeof(struct js_event));

	/* Identify as a joystick */ 
	bzero(message_buf, buf_len);
	sprintf(message_buf, "id:joystick\n");
	write(sockfd, message_buf, buf_len);

	/* Read joystick messages, and serialize them into text */
	while (read(joystick_fd, &event, sizeof(struct js_event)) == sizeof(struct js_event)) {
		bzero(message_buf, buf_len);
		switch (event.type) {
			case JS_EVENT_BUTTON:
				sprintf(message_buf, "but %i %i\n", event.number, event.value);
				break;
			case JS_EVENT_AXIS:
				sprintf(message_buf, "axs %i %i\n", event.number, event.value);
				break;
			default:
				break;
		}
		if (write(sockfd, message_buf, buf_len) < buf_len)
			fail("on socket write");
	}
	fail("on joystick read");

	close(sockfd);
	close(joystick_fd);

}

/* Check error message and quit */
void fail(char* where) {
	fprintf(stderr, "\e[31mError\e[m %s: %s\n", where, strerror(errno));
	exit(EXIT_FAILURE);
}
