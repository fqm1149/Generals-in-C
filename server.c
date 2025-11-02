#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <raylib.h>
#include <math.h>
#include "../basicmap/generals.h"
#include <pthread.h>
#include <stdio.h>


pthread_t send_fd, recv_fd;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_c = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_exit = PTHREAD_COND_INITIALIZER;
void MapSender(SOCKET *client_fd);
void MoveReceiver(SOCKET* client_fd);
int MoveOneStep(Move move);
int condition = 0;
int condition_exit = 0;
int line = LINE;
int column = COLUMN;
char isappliedtmp[8];
Block** mapL1;
Block* send_buffer;
Move movebuffer[8] = { 0 };
extern int** generatemap(int x, int y);
bool isconnected = false;
char isapplied[8] = { 0 };
int roundn = 0;

int main(void)
{
	srand((unsigned int)time(NULL));
	WSADATA wsadata;
	int result = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (result != 0) {
		printf("wsastartup failed");
		return 1;
	}
	int opt = 1;
	printf("waiting for connection...\n");
	int** map = generatemap(line, column);
	mapL1 = malloc(line * sizeof(Block*));
	send_buffer = malloc(line * column * sizeof(Block));
	//mapL1¸³Öµ
	for (int i = 0; i < line; i++) {
		mapL1[i] = malloc(column * sizeof(Block));
		for (int j = 0; j < column; j++) {
			mapL1[i][j].type = map[i + 1][j + 1];
			switch (map[i + 1][j + 1]) {
			case -3:
				mapL1[i][j].owner = 1;
				mapL1[i][j].num = 1;
				break;
			case 1:
				mapL1[i][j].owner = 0;
				mapL1[i][j].num = rand() % 15 + 40;
				break;
			default:
				mapL1[i][j].owner = 0;
				mapL1[i][j].num = 0;
				break;
			}
		}
	}

	SOCKET listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	SOCKET client_fd[8];
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	struct sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);
	int addrlen = sizeof(address);
	bind(listen_fd, (struct sockaddr*)&address, &addrlen);
	start:
	listen(listen_fd, 3);
	client_fd[0] = accept(listen_fd, (struct sockaddr*)&address, &addrlen);
	if (client_fd[0] < 0) return 1;
	printf("connected!\n");
	isconnected = true;
	//pthread_mutex_init(&mutex, NULL);
	pthread_create(&send_fd, NULL, MapSender, client_fd);
	pthread_create(&recv_fd, NULL, MoveReceiver, client_fd);
	while (condition_exit == 0) {
		pthread_mutex_lock(&mutex);
		//printf("available\n");
		roundn++;
		for (int i1 = 1; i1 <= line; i1++) for (int j1 = 1; j1 <= column; j1++) {
			if (mapL1[i1 - 1][j1 - 1].type == CROWN) mapL1[i1 - 1][j1 - 1].num++;
			if (mapL1[i1 - 1][j1 - 1].type == CITY && mapL1[i1 - 1][j1 - 1].owner == 1) mapL1[i1 - 1][j1 - 1].num++;
			if (roundn % 25 == 0) if (mapL1[i1 - 1][j1 - 1].owner == 1) mapL1[i1 - 1][j1 - 1].num++;
		}

		if (movebuffer[0].startx != 0) {
			MoveOneStep(movebuffer[0]);
			movebuffer[0].startx = 0;
			isapplied[0]++;
		}

		pthread_mutex_unlock(&mutex);
		pthread_mutex_lock(&mutex_c);
		condition = 1;
		pthread_mutex_unlock(&mutex_c);
		pthread_cond_signal(&cond);
		printf("main thread send signal to sendmap thread\n");
		Sleep(800);
		//printf("available\n");
	}
	printf("client disconnected! Press any key to end game\n");
	system("pause");


	pthread_cancel(send_fd);
	pthread_cancel(recv_fd);

	closesocket(client_fd[0]);
	closesocket(listen_fd);
	listen_fd = INVALID_SOCKET;
	client_fd[0] = INVALID_SOCKET;
	for (int i = 0; i < line; i++) free(mapL1[i]);
	free(mapL1);
	free(send_buffer);
	WSACleanup();
	return 0;
}

void MapSender(SOCKET *client_fd) {

	while(1){
		pthread_mutex_lock(&mutex_c);
		while (condition == 0) {
			pthread_cond_wait(&cond, &mutex_c);
		}
		//printf("sendmap thread get signal and try to send map\n");
		condition = 0;

		pthread_mutex_unlock(&mutex_c);
		//printf("available\n");
		pthread_mutex_lock(&mutex);
		//printf("available\n");
		Block* buffer_ptr = send_buffer;
		for (int i = 0; i < line; i++) {
			memcpy(buffer_ptr, mapL1[i], column * sizeof(Block));
			buffer_ptr += column;
		}
		//printf("available\n");
		int roundt = roundn;
		isappliedtmp[0] = isapplied[0];
		isapplied[0] = 0;
		pthread_mutex_unlock(&mutex);
		//printf("available\n");
		int a = send(client_fd[0], send_buffer, line * column * sizeof(Block), 0);
		//printf("%d", a);
		if (a == -1) {
			condition_exit = 1;
			pthread_cond_broadcast(cond_exit);
		}
		//printf("now about to send\n");
		//send(client_fd[0], send_buffer, line * column * sizeof(Block), 0);
		send(client_fd[0], &isappliedtmp[0], sizeof(char), 0);
		send(client_fd[0], &roundt, sizeof(int), 0);
		printf("map sent successfully!\n");
	}
}

void MoveReceiver(SOCKET* client_fd) {
	Move recvbuffer[8] = {0};
	while (1) {
		recv(client_fd[0], &recvbuffer[0], sizeof(Move), MSG_WAITALL);
		pthread_mutex_lock(&mutex);
		movebuffer[0] = recvbuffer[0];
		pthread_mutex_unlock(&mutex);
	}
}

int MoveOneStep(Move move) {
	int initial_owner = mapL1[move.startx - 1][move.starty - 1].owner;
	int initial_type = mapL1[move.startx - 1][move.starty - 1].type;
	int target_type = mapL1[move.endx - 1][move.endy - 1].type;
	if (initial_owner != 1 || initial_type == MOUNTAIN || target_type == MOUNTAIN) return 1;
	int initial_army = mapL1[move.startx - 1][move.starty - 1].num;
	mapL1[move.startx - 1][move.starty - 1].num = 1;
	if (target_type != CITY) {
		mapL1[move.endx - 1][move.endy - 1].num += initial_army - 1;
		if (initial_army > 1)mapL1[move.endx - 1][move.endy - 1].owner = 1;
	}
	else {
		int city_num = mapL1[move.endx - 1][move.endy - 1].num;
		int city_owner = mapL1[move.endx - 1][move.endy - 1].owner;
		if (city_owner == 1) mapL1[move.endx - 1][move.endy - 1].num += initial_army - 1;
		else {
			if (initial_army - 1 > city_num) { mapL1[move.endx - 1][move.endy - 1].num = initial_army - 1 - city_num; mapL1[move.endx - 1][move.endy - 1].owner = 1; }
			else mapL1[move.endx - 1][move.endy - 1].num = city_num + 1 - initial_army;
		}
	}
	return 0;
}

