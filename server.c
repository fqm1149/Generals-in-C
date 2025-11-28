#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <raylib.h>
#include <math.h>
#include "../basicmap/generals.h"
#include <stdio.h>
#include <errno.h>
#include <windows.h>
#include <threads.h>

thrd_t send_fd, recv_fd, logic_fd;
mtx_t mutex;//数据读写保护（游戏逻辑部分）
mtx_t mutex_netIO;//确保发送过程不受干扰 或者说更改重要大文件时不会产生发送动作
mtx_t mutex_c;//确保进程间的通信完整
mtx_t mutex_e;
cnd_t cond;
cnd_t cond_exit;
int send_to_client(void* arg);
int recv_from_client(void* arg);
int logic_process(void* arg);
int MoveOneStep(Move move);
void OwnerReplace(int loser, int winner);

bool running = 1;

bool NeedToSendData = false;
int condition_exit = 0;
int line = LINE;
int column = COLUMN;
char isappliedtmp[8];
int playercount,alivecount,waitcount;
Block** mapL1;
Block* send_buffer;
Move movebuffer[8] = { 0 };
SOCKET listen_fd;
extern int** generatemap(int x, int y,int player_num);
char isapplied[8] = { 0 };
int roundn = 0;
struct sockaddr_in address;
int addrlen;
SetupData setupdata = { 0 };
char messageType, currentCMD,game_status;
SOCKET wait_fd[8];
void SendWait(SOCKET fd) {
	char wms = SERVER_CMD;
	char wcmd = WAIT_FOR_END;
	send(fd, &wms, 1, NULL);
	send(fd, &wcmd, 1, NULL);
}
void SendJoin(SOCKET fd) {
	char wms = SERVER_CMD;
	char wcmd = JOIN;
	send(fd, &wms, 1, NULL);
	send(fd, &wcmd, 1, NULL);
}
void SendSignal(char msgtype, char cmd) {
	mtx_lock(&mutex_c);
	messageType = msgtype;
	currentCMD = cmd;
	NeedToSendData = true;
	cnd_signal(&cond);
	mtx_unlock(&mutex_c);
}
void LazyColorSetup() {
	setupdata.playercolor[0] = (Color){ 240,50,230,255 };
	setupdata.playercolor[1] = (Color){ 39,146,255,255 };
	setupdata.playercolor[2] = (Color){ 128,0,128,255 };
	setupdata.playercolor[3] = (Color){ 250,140,1,255 };
}
int main(void)
{
	mtx_init(&mutex, mtx_plain);
	mtx_init(&mutex_c,mtx_plain);
	mtx_init(&mutex_e,mtx_plain);
	mtx_init(&mutex_netIO, mtx_plain);
	cnd_init(&cond);
	cnd_init(&cond_exit);
	LazyColorSetup();
	WSADATA wsadata;
	int result = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (result != 0) {
		printf("wsastartup failed");
		return 1;
	}
	printf("setting up...\n");
	game_status = WAITING_FOR_START;
	//初始化监听
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	while (listen_fd == INVALID_SOCKET) {
		printf("listenfd creation failed:%d\n", WSAGetLastError());
	}
	SOCKET client_fd[8];
	int opt = 1;
	while (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == SOCKET_ERROR) {
		printf("setsockopt failed:%d\n", WSAGetLastError());
		Sleep(1000);
	}
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(PORT);
	addrlen = sizeof(address);
	while(bind(listen_fd, (struct sockaddr*)&address, addrlen) == SOCKET_ERROR) {
		printf("bind failed:%d\n",WSAGetLastError());
		Sleep(1000);
	}
	listen(listen_fd, 3);
	setupdata.mapx = line;
	setupdata.mapy = column;

	thrd_create(&recv_fd, recv_from_client, client_fd);
	thrd_create(&send_fd, send_to_client, client_fd);

	// 等待退出信号
	mtx_lock(&mutex_e);
	while (condition_exit == 0) cnd_wait(&cond_exit, &mutex_e);
	mtx_unlock(&mutex_e);
	printf("sending exit signal\n");
	// 请求线程退出
	running = false;
	cnd_signal(&cond);
	cnd_signal(&cond_exit);

	// wait for threads
	thrd_join(recv_fd, &result);
	thrd_join(logic_fd, &result);
	thrd_join(send_fd, &result);

	printf("some client disconnected! Press any key to end game\n(Don't close the terminal window directly)\n");
	system("pause");


	for (int i = 0; i < playercount;i++) closesocket(client_fd[i]);
	closesocket(listen_fd);
	for (int i = 0; i < line; i++) free(mapL1[i]);
	free(mapL1);
	free(send_buffer);
	mtx_destroy(&mutex);
	mtx_destroy(&mutex_c);
	mtx_destroy(&mutex_e);
	cnd_destroy(&cond);
	cnd_destroy(&cond_exit);
	WSACleanup();
	return 0;
}

int send_to_client(void* arg) {

	SOCKET* client_fd = (SOCKET*)arg;
	while (running != 0) {
		mtx_lock(&mutex_c);
		while (NeedToSendData == 0 && running != 0) {
			cnd_wait(&cond, &mutex_c);
		}
		if (running == 0) { mtx_unlock(&mutex_c); break; }
		NeedToSendData = 0;
		printf("try to send %d\n", (int)messageType);
		char temp_messageType = messageType;
		char temp_currentCMD = currentCMD;
		mtx_unlock(&mutex_c);
		if (temp_messageType == MAP_DATA) {
			//缓冲区赋值
			mtx_lock(&mutex);

			Block* buffer_ptr = send_buffer;
			for (int i = 0; i < line; i++) {
				memcpy(buffer_ptr, mapL1[i], column * sizeof(Block));
				buffer_ptr += column;
			}
			for (int i = 0; i < playercount; i++) {
				isappliedtmp[i] = isapplied[i];
				isapplied[i] = 0;
			}
			int roundt = roundn;

			mtx_unlock(&mutex);
			//发送
			mtx_lock(&mutex_netIO);
			for (int i = 0; i < playercount; i++) {
				send(client_fd[i], &temp_messageType, 1, 0);
				send(client_fd[i], send_buffer, line * column * sizeof(Block), 0);
				send(client_fd[i], &isappliedtmp[i], sizeof(char), 0);
				send(client_fd[i], &roundt, sizeof(int), 0);
			}
			mtx_unlock(&mutex_netIO);
			printf("map sent successfully!\n");
		}
		if (temp_messageType == SETUP_DATA) {
			mtx_lock(&mutex_netIO);
			for (int i = 0; i < playercount; i++) {
				setupdata.clientnum = i;
				send(client_fd[i], &temp_messageType, 1, 0);
				send(client_fd[i], &setupdata, sizeof(SetupData), 0);
			}
			mtx_unlock(&mutex_netIO);
		}
		if (temp_messageType == SERVER_CMD) {
			mtx_lock(&mutex_netIO);
			for (int i = 0; i < playercount; i++) {
				send(client_fd[i], &temp_messageType, 1, 0);
				send(client_fd[i], &temp_currentCMD, 1, 0);
			}
			
			if (temp_currentCMD == GAME_READY) {
				srand((unsigned int)time(NULL));
				int** map = generatemap(line, column, playercount);
				mapL1 = malloc(line * sizeof(Block*));
				send_buffer = malloc(line * column * sizeof(Block));
				for (int i = 0, pc = 1; i < line; i++) {
					mapL1[i] = malloc(column * sizeof(Block));
					for (int j = 0; j < column; j++) {
						mapL1[i][j].type = map[i + 1][j + 1];
						switch (map[i + 1][j + 1]) {
						case -3:
							mapL1[i][j].owner = pc;
							mapL1[i][j].num = 1;
							pc++;
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
			}
			mtx_unlock(&mutex_netIO);
			if (temp_currentCMD == GAME_START) {
				setupdata.readynum = 0;
				alivecount = playercount;
				game_status = START;
				thrd_create(&logic_fd, logic_process, NULL);
				}
			if (temp_currentCMD == SHOW_MAP) {
				game_status = WAITING_FOR_START;
				for (int i = 0; i < line; i++) free(mapL1[i]);
				free(mapL1);
				free(send_buffer);
				setupdata.readynum = 0;
				roundn = 0;
				for (int i = 0; i < waitcount; i++) {
					SendJoin(wait_fd[i]);
					strcpy(setupdata.playername[playercount], "Anonymous");
					client_fd[playercount++] = wait_fd[i];
				}
				waitcount = 0;
				setupdata.totalnum = playercount;
				SendSignal(SETUP_DATA, 0);
			}
		}
	}
	return 0;
}

int recv_from_client(void* arg) {
	SOCKET* client_fd = (SOCKET*)arg;
	Move recvbuffer_move[8] = {0};
	fd_set readfds;
	SOCKET maxfd;
	FD_ZERO(&readfds);
	FD_SET(listen_fd, &readfds);
	maxfd = listen_fd;
	struct timeval timeout;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	while (running != 0) {
		fd_set tmpfds = readfds;
		int activity = select((int)maxfd + 1, &tmpfds, NULL, NULL, &timeout);
		if (activity <= 0) continue;
		if (FD_ISSET(listen_fd, &tmpfds)) {
			SOCKET newfd = accept(listen_fd, (struct sockaddr*)&address, &addrlen);
			if (newfd == INVALID_SOCKET) {
				printf("accept failed:%d\n", WSAGetLastError());
			} else if (game_status==WAITING_FOR_START) {
				printf("new connection!\n");
				client_fd[playercount] = newfd;
				FD_SET(newfd, &readfds);
				if (maxfd < newfd) maxfd = newfd;
				strcpy(setupdata.playername[playercount], "Anonymous");
				playercount++;
				setupdata.totalnum = playercount;
				SendSignal(SETUP_DATA, 0);
			}
			else {
				printf("new connection! But need to wait\n");
				wait_fd[waitcount++] = newfd;
				FD_SET(newfd, &readfds);
				if (maxfd < newfd) maxfd = newfd;
				SendWait(newfd);
			}
		}

		for (int i = 0; i < playercount; i++) {
			if (FD_ISSET(client_fd[i], &tmpfds)) {
				char msgType;
				int ret = recv(client_fd[i], &msgType, 1, MSG_WAITALL);
				if (ret <= 0) {
					printf("client %d disconnected (recv=%d, err=%d)\n", i, ret, WSAGetLastError());
					mtx_lock(&mutex_netIO);
					closesocket(client_fd[i]);
					FD_CLR(client_fd[i], &readfds);
					// 从数组中移除该 client 并调整 playercount
					if (game_status == START) {
						OwnerReplace(i + 1, -1);
					}
					Color tmpcolor = setupdata.playercolor[i];
					for (int k = i; k < playercount - 1; k++) {
						client_fd[k] = client_fd[k + 1];
						setupdata.playercolor[k] = setupdata.playercolor[k + 1];
						memcpy(setupdata.playername[k], setupdata.playername[k + 1], 20);
						if (game_status == START) OwnerReplace(k + 2, k + 1);
					}
					setupdata.playercolor[playercount - 1] = tmpcolor;
					playercount--;
					if (playercount == 0) {
						condition_exit = 1;
						cnd_signal(&cond_exit);
						break;
					}
					setupdata.totalnum = playercount;
					// 重新计算 maxfd
					maxfd = listen_fd;
					for (int k = 0; k < playercount; k++) if (client_fd[k] > maxfd) maxfd = client_fd[k];
					if (game_status == START) {
						alivecount--;
						printf("detect discnnted,alivecount now: %d\n", alivecount);
						if (alivecount <= 1) {
							mtx_unlock(&mutex_netIO); 
							SendSignal(SERVER_CMD, SHOW_MAP);
							i--; continue;
						}
					}
					mtx_unlock(&mutex_netIO);
					SendSignal(SETUP_DATA, 0);
					i--; // 已移位，保持索引正确
					continue;
				}
				printf("received message %d from client %d\n", (int)msgType, i);
				if (msgType == UPLOAD_MOVE){
					recv(client_fd[i], &recvbuffer_move[i], sizeof(Move), MSG_WAITALL);
					mtx_lock(&mutex);
					movebuffer[i] = recvbuffer_move[i];
					movebuffer[i].launcher = i + 1;
					mtx_unlock(&mutex);
				}
				if (msgType == CLIENT_CMD) {
					char clientCmd;
					recv(client_fd[i], &clientCmd, 1, MSG_WAITALL);
					if (clientCmd == CLIENT_READY) {
						setupdata.readynum++;
						if (setupdata.readynum == playercount && playercount > 1) {
							SendSignal(SERVER_CMD, GAME_READY);
							Sleep(1500);
							SendSignal(SERVER_CMD, GAME_START);
						}
						else {
							SendSignal(SETUP_DATA, 0);
						}
					}
					if (clientCmd == CLIENT_CANCEL) {
						setupdata.readynum--;
						SendSignal(SETUP_DATA, 0);
					}
					if (clientCmd == CLIENT_LOSE) {
						alivecount--;
						if (alivecount <= 1) {
							SendSignal(SERVER_CMD, SHOW_MAP);
						}
					}
				}
				if (msgType == UPLOAD_NAME) {
					char tmpname[20];
					recv(client_fd[i], tmpname, 20, MSG_WAITALL);
					mtx_lock(&mutex);
					strcpy(setupdata.playername[i], tmpname);
					mtx_unlock(&mutex);
					SendSignal(SETUP_DATA, 0);
				}
			}
		}
		for (int i = 0; i < waitcount; i++) {
			if (FD_ISSET(wait_fd[i], &tmpfds)) {
				char msgType;
				int ret = recv(wait_fd[i], &msgType, 1, MSG_WAITALL);
				if (ret <= 0) {
					printf("waiting client %d disconnected (recv=%d, err=%d)\n", i, ret, WSAGetLastError());
					closesocket(wait_fd[i]);
					FD_CLR(wait_fd[i], &readfds);
					// 从数组中移除该 waiting client 并调整 playercount
					waitcount--;
					maxfd = listen_fd;
					for (int k = 0; k < playercount; k++) if (client_fd[k] > maxfd) maxfd = client_fd[k];
					for (int k = 0; k < waitcount; k++) if (wait_fd[k] > maxfd) maxfd = wait_fd[k];
					i--;
				}
			}
		}
	}
	return 0;
}

int MoveOneStep(Move move) {
	int initial_owner = mapL1[move.startx - 1][move.starty - 1].owner;
	int initial_type = mapL1[move.startx - 1][move.starty - 1].type;
	int target_type = mapL1[move.endx - 1][move.endy - 1].type;
	int target_owner = mapL1[move.endx - 1][move.endy - 1].owner;
	if (initial_owner != move.launcher || initial_type == MOUNTAIN || target_type == MOUNTAIN) return 1;
	int initial_army = mapL1[move.startx - 1][move.starty - 1].num;
	mapL1[move.startx - 1][move.starty - 1].num = 1;
	if (target_owner == move.launcher) mapL1[move.endx - 1][move.endy - 1].num += initial_army - 1;
	else if (target_owner == 0) {
		if (target_type != CITY) {
			mapL1[move.endx - 1][move.endy - 1].num += initial_army - 1;
			if (initial_army > 1)mapL1[move.endx - 1][move.endy - 1].owner = move.launcher;
		}
		else {
			int city_num = mapL1[move.endx - 1][move.endy - 1].num;
			int city_owner = mapL1[move.endx - 1][move.endy - 1].owner;
			if (initial_army - 1 > city_num) { 
				mapL1[move.endx - 1][move.endy - 1].num = initial_army - 1 - city_num; 
				mapL1[move.endx - 1][move.endy - 1].owner = move.launcher; 
			}
			else mapL1[move.endx - 1][move.endy - 1].num = city_num + 1 - initial_army;
		}
	}
	else {
		int target_num= mapL1[move.endx - 1][move.endy - 1].num;
		if (initial_army - 1 > target_num) {
			mapL1[move.endx - 1][move.endy - 1].num = initial_army - 1 - target_num;
			int killowner = mapL1[move.endx - 1][move.endy - 1].owner;
			mapL1[move.endx - 1][move.endy - 1].owner = move.launcher;
			if (target_type == CROWN) {
				mapL1[move.endx - 1][move.endy - 1].type = CITY;
				OwnerReplace(killowner, move.launcher);
			}
		}
		else mapL1[move.endx - 1][move.endy - 1].num = target_num + 1 - initial_army;
	}
	return 0;
}

int logic_process(void* arg) {
	(void)arg;
	while (condition_exit == 0 && running != 0 && game_status == START) {
		mtx_lock(&mutex);
		//轮数增加
		roundn++;
		//增兵
		for (int i1 = 1; i1 <= line; i1++) for (int j1 = 1; j1 <= column; j1++) {
			if (mapL1[i1 - 1][j1 - 1].type == CROWN) mapL1[i1 - 1][j1 - 1].num++;
			if (mapL1[i1 - 1][j1 - 1].type == CITY && mapL1[i1 - 1][j1 - 1].owner != 0) mapL1[i1 - 1][j1 - 1].num++;
			if (roundn % 25 == 0) if (mapL1[i1 - 1][j1 - 1].owner != 0) mapL1[i1 - 1][j1 - 1].num++;
		}
		//处理Move
		for (int i = 0; i < playercount; i++) {
			if (movebuffer[i].startx != 0) {
				MoveOneStep(movebuffer[i]);
				movebuffer[i].startx = 0;
				isapplied[i]++;
			}
		}
		
		mtx_unlock(&mutex);
		SendSignal(MAP_DATA, 0);
		Sleep(500);
	}
	return 0;
}
void OwnerReplace(int loser, int winner) {
	for (int i1 = 1; i1 <= line; i1++) for (int j1 = 1; j1 <= column; j1++) {
		if (mapL1[i1 - 1][j1 - 1].owner == loser) mapL1[i1 - 1][j1 - 1].owner = winner;
	}
}
