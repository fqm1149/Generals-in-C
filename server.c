#define _CRT_SECURE_NO_WARNINGS
#include <stdlib.h>
#include <raylib.h>
#include <math.h>
#include "../basicmap/generals.h"
#include <stdio.h>
#include <errno.h>
#include <windows.h>
#include <threads.h>

thrd_t thrd_send, thrd_recv, thrd_game_logic, thrd_broadcast;
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
void Renderer();
bool running = 1;
int roundTime = 800;
bool NeedToSendData = false;
HINSTANCE bot_file[8];
int botcount = 0;
thrd_t bot_thrd[8];
int condition_exit = 0;
int line = LINE;
int column = COLUMN;
char isappliedtmp[8];
int playercount,alivecount,waitcount;
Block** mapL1;
Block* send_buffer;
Move movebuffer[8] = { 0 };
SOCKET listen_fd,broadcast_fd;
extern int** generatemap(int x, int y,int player_num);
char isapplied[8] = { 0 };
int roundn = 0;
int version = 251129;
struct sockaddr_in address,broadcast_addr;
int addrlen;
int port = PORT;
char botpath[MAX_PATH];
SetupData setupdata = { 0 };
char messageType, currentCMD,game_status;
char serverName[30];
SOCKET wait_fd[8];
Font font;
char bot_cmd[8];
bool NeedToFreeMap = false;

void DrawTextAtCenter(char* text, int x, int y, int fontsize, Color color);
void DrawFilledRectangle(int startx, int starty, int width, int height, char* text, int fontsize, Color lineColor, Color fillColor, Color textcolor);
Rectangle DrawButtonAtCenter(char* text, int x, int y, int fontsize, Color textColor, Color buttonColor, Color shaderColor) {
	Vector2 textsize = MeasureTextEx(font, text, fontsize, TEXT_SPACING);
	Rectangle button = { x - textsize.x / 2 - 30,y - textsize.y / 2 - 20,textsize.x + 60,textsize.y + 40 };
	Rectangle shader = { x - textsize.x / 2 - 25,y - textsize.y / 2 - 15,textsize.x + 60,textsize.y + 40 };
	DrawRectangleRec(shader, shaderColor);
	DrawRectangleRec(button, buttonColor);
	DrawTextAtCenter(text, x, y, fontsize, textColor);
	return button;
}
extern WCHAR* GetBotDLL();
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
void BroadCastInfo() {
	while (running != 0) {
		char buffer[1024];
		snprintf(buffer, sizeof(buffer), "GEN_SERVER|%d|%s|%d",port,serverName,version);
		sendto(broadcast_fd, buffer, strlen(buffer), 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
		printf("Broadcasted: %s\n", buffer);
		Sleep(5000);
	}
}
int main(void)
{
	strcpy(serverName, "Local Server");
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
	address.sin_port = htons(port);
	addrlen = sizeof(address);
	while(bind(listen_fd, (struct sockaddr*)&address, addrlen) == SOCKET_ERROR) {
		printf("bind failed:%d\n",WSAGetLastError());
		Sleep(1000);
	}
	listen(listen_fd, 3);
	setupdata.mapx = line;
	setupdata.mapy = column;

	thrd_create(&thrd_recv, recv_from_client, client_fd);
	thrd_create(&thrd_send, send_to_client, client_fd);

	broadcast_fd = socket(AF_INET, SOCK_DGRAM, 0);
	setsockopt(broadcast_fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
	memset(&broadcast_addr, 0, sizeof(broadcast_addr));
	broadcast_addr.sin_family = AF_INET;
	broadcast_addr.sin_port = htons(BROADCAST_PORT);
	broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST;

	thrd_create(&thrd_broadcast, BroadCastInfo, NULL);



	Renderer();
	printf("sending exit signal\n");
	for (int i = 0; i < botcount; i++) {
		bot_cmd[i] = BOT_EXIT;
	}
	SendSignal(SERVER_CMD, SERVER_OFF);
	Sleep(100);
	// 请求线程退出
	running = false;
	cnd_signal(&cond);
	cnd_signal(&cond_exit);

	// wait for threads
	thrd_join(thrd_recv, &result);
	thrd_join(thrd_game_logic, &result);
	thrd_join(thrd_send, &result);
	for (int i = 0; i < botcount; i++) {
		thrd_join(bot_thrd[i],&result);
		FreeLibrary(bot_file[i]);
	}

	//printf("some client disconnected! Press any key to end game\n(Don't close the terminal window directly)\n");


	for (int i = 0; i < playercount;i++) closesocket(client_fd[i]);
	closesocket(listen_fd);
	
	if(NeedToFreeMap){
		for (int i = 0; i < line; i++) free(mapL1[i]);
		free(mapL1);
		free(send_buffer);
	}
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
		//printf("try to send %d\n", (int)messageType);
		char temp_messageType = messageType;
		char temp_currentCMD = currentCMD;
		mtx_unlock(&mutex_c);
		if (temp_messageType == BOT_ADD) {
			bot_file[botcount] = LoadLibrary(GetBotDLL());
			if (bot_file[botcount] == NULL) {
				DWORD error = GetLastError();
				fprintf(stderr, "Failed to load DLL %s, error: %lu\n", botpath, error);
				continue;
			}
			int (*bot_function)(BotData *botdata) = (int(*)(BotData *botdata))GetProcAddress(bot_file[botcount], "bot_function");
			if (bot_function == NULL) {
				DWORD error = GetLastError();
				fprintf(stderr, "Failed to get function address from %s, error: %lu\n", botpath, error);
				FreeLibrary(bot_file[botcount]);  // 清理资源
				continue;
			}
			BotData botdata;
			botdata.cmd = &bot_cmd[botcount];
			botdata.port = port;
			thrd_create(&bot_thrd[botcount++], bot_function, &botdata);
		}
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
				game_status = READY;
				srand((unsigned int)time(NULL));
				int** map = generatemap(line, column, playercount);
				NeedToFreeMap = true;
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
				thrd_create(&thrd_game_logic, logic_process, NULL);
				}
			if (temp_currentCMD == SHOW_MAP) {
				game_status = WAITING_FOR_START;
				for (int i = 0; i < line; i++) free(mapL1[i]);
				free(mapL1);
				free(send_buffer);
				NeedToFreeMap = false;
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
	if (initial_owner != move.launcher) {
		printf("invalid move!type1\n"); return 1;
	}
	if (initial_type == MOUNTAIN || target_type == MOUNTAIN) {
		printf("invalid move!type2\n"); return 1;
	}
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
			if (mapL1[i1 - 1][j1 - 1].type == CITY && mapL1[i1 - 1][j1 - 1].owner > 0) mapL1[i1 - 1][j1 - 1].num++;
			if (roundn % 25 == 0) if (mapL1[i1 - 1][j1 - 1].owner > 0) mapL1[i1 - 1][j1 - 1].num++;
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
		Sleep(roundTime);
	}
	return 0;
}
void OwnerReplace(int loser, int winner) {
	for (int i1 = 1; i1 <= line; i1++) for (int j1 = 1; j1 <= column; j1++) {
		if (mapL1[i1 - 1][j1 - 1].owner == loser) {
			mapL1[i1 - 1][j1 - 1].owner = winner;
			mapL1[i1 - 1][j1 - 1].num = (mapL1[i1 - 1][j1 - 1].num + 1) / 2;
		}
	}
}
void Renderer() {
	int screenWidth = 1440;
	int screenHeight = 1440;
	float cursorTimer = 0.0f;
	bool cursorVisible = true;
	SetConfigFlags(FLAG_MSAA_4X_HINT);
	SetTraceLogLevel(LOG_NONE);
	InitWindow(screenWidth, screenHeight, "Generals_server");
	SetWindowState(FLAG_WINDOW_RESIZABLE);
	SetTargetFPS(60);
	Color background = { 34,34,34,255 };
	Color GeneralsGreen = { 0,128,128,255 };
	// 输入框相关变量
	int inputLine = line, inputColumn = column, inputRoundTime = roundTime;
	char bufLine[8], bufColumn[8], bufRoundTime[8];
	snprintf(bufLine, sizeof(bufLine), "%d", inputLine);
	snprintf(bufColumn, sizeof(bufColumn), "%d", inputColumn);
	snprintf(bufRoundTime, sizeof(bufRoundTime), "%d", inputRoundTime);

	int focus = 0; // 0: none, 1: line, 2: column, 3: roundtime
	int focusRealTime = 0;
	if (font.texture.id == 0) font = GetFontDefault();
	while (!WindowShouldClose()) {
		screenWidth = GetScreenWidth();
		screenHeight = GetScreenHeight();
		BeginDrawing();
		cursorTimer += GetFrameTime();
		if (cursorTimer >= 0.5f) {
			cursorVisible = !cursorVisible;
			cursorTimer = 0.0f;
		}
		ClearBackground(background);

		if (game_status == WAITING_FOR_START) {
			// 显示已连接玩家
			Vector2 mouse = GetMousePosition();
			int boxWidth = 400, boxHeight = 80;
			int startY = screenHeight / 2 - (playercount * boxHeight) / 2;
			for (int i = 0; i < playercount; i++) {
				int x = (screenWidth - boxWidth) / 2;
				int y = startY + i * boxHeight;
				DrawFilledRectangle(x, y, boxWidth, boxHeight, setupdata.playername[i], 35, BLACK, setupdata.playercolor[i], WHITE);
			}

			// 输入框布局
			int inputBoxW = 300, inputBoxH = 70, inputGap = 40;
			int inputStartY = startY + playercount * boxHeight + 80;
			int inputStartX = (screenWidth - (inputBoxW * 3 + inputGap * 2)) / 2;

			if (CheckCollisionPointRec(mouse, (Rectangle) { inputStartX, inputStartY, inputBoxW, inputBoxH })) focusRealTime = 1;
			else if (CheckCollisionPointRec(mouse, (Rectangle) { inputStartX + inputBoxW + inputGap, inputStartY, inputBoxW, inputBoxH })) focusRealTime = 2;
			else if (CheckCollisionPointRec(mouse, (Rectangle) { inputStartX + (inputBoxW + inputGap) * 2, inputStartY, inputBoxW, inputBoxH })) focusRealTime = 3;
			else focusRealTime = 0;

			if (focusRealTime) SetMouseCursor(MOUSE_CURSOR_IBEAM);
			else SetMouseCursor(MOUSE_CURSOR_DEFAULT);

			// line输入框
			DrawRectangle(inputStartX, inputStartY, inputBoxW, inputBoxH, WHITE);
			DrawRectangleLinesEx((Rectangle) { inputStartX, inputStartY, inputBoxW, inputBoxH }, 2, focusRealTime == 1 ? GeneralsGreen : BLACK);
			DrawText("Line", inputStartX + 10, inputStartY + 8, 35, GRAY);
			DrawText(bufLine, inputStartX + 20+MeasureText("Line",35), inputStartY + 8, 35, BLACK);
			if (focus == 1 && cursorVisible) {
				int tx = inputStartX + 20 + MeasureText("Line", 35) + MeasureText(bufLine, 35);
				int ty = inputStartY + 8;
				DrawRectangle(tx, ty, 2, 28, BLACK);
			}
			// column输入框
			DrawRectangle(inputStartX + inputBoxW + inputGap, inputStartY, inputBoxW, inputBoxH, WHITE);
			DrawRectangleLinesEx((Rectangle) { inputStartX + inputBoxW + inputGap, inputStartY, inputBoxW, inputBoxH }, 2, focusRealTime == 2 ? GeneralsGreen : BLACK);
			DrawText("Column", inputStartX + inputBoxW + inputGap + 10, inputStartY + 8, 35, GRAY);
			DrawText(bufColumn, inputStartX + inputBoxW + inputGap + 20+MeasureText("Column",35), inputStartY + 8, 35, BLACK);
			if (focus == 2 && cursorVisible) {
				int tx = inputStartX + inputBoxW + inputGap + 20 + MeasureText("Column",35) + MeasureText(bufColumn, 35);
				int ty = inputStartY + 8;
				DrawRectangle(tx, ty, 2, 28, BLACK);
			}
			// round time输入框
			DrawRectangle(inputStartX + (inputBoxW + inputGap) * 2, inputStartY, inputBoxW, inputBoxH, WHITE);
			DrawRectangleLinesEx((Rectangle) { inputStartX + (inputBoxW + inputGap) * 2, inputStartY, inputBoxW, inputBoxH },2, focusRealTime == 3 ? GeneralsGreen : BLACK);
			DrawText("Round(ms)", inputStartX + (inputBoxW + inputGap) * 2 + 10, inputStartY + 8, 35, GRAY);
			DrawText(bufRoundTime, inputStartX + (inputBoxW + inputGap) * 2 + 20+MeasureText("Round(ms)",35), inputStartY + 8,35, BLACK);
			if (focus == 3 && cursorVisible) {
				int tx = inputStartX + (inputBoxW + inputGap) * 2 + 20 + MeasureText("Round(ms)", 35) + MeasureText(bufRoundTime, 35);
				int ty = inputStartY + 8;
				DrawRectangle(tx, ty, 2, 28, BLACK);
			}
			// 输入框焦点切换与内容编辑
			if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
				if (CheckCollisionPointRec(mouse, (Rectangle) { inputStartX, inputStartY, inputBoxW, inputBoxH })) focus = 1;
				else if (CheckCollisionPointRec(mouse, (Rectangle) { inputStartX + inputBoxW + inputGap, inputStartY, inputBoxW, inputBoxH })) focus = 2;
				else if (CheckCollisionPointRec(mouse, (Rectangle) { inputStartX + (inputBoxW + inputGap) * 2, inputStartY, inputBoxW, inputBoxH })) focus = 3;
				else focus = 0;
			}
			int key = GetCharPressed();
			if (focus == 1 && key >= '0' && key <= '9' && strlen(bufLine) < 6) {
				int len = strlen(bufLine);
				bufLine[len] = (char)key;
				bufLine[len + 1] = '\0';
			}
			if (focus == 2 && key >= '0' && key <= '9' && strlen(bufColumn) < 6) {
				int len = strlen(bufColumn);
				bufColumn[len] = (char)key;
				bufColumn[len + 1] = '\0';
			}
			if (focus == 3 && key >= '0' && key <= '9' && strlen(bufRoundTime) < 6) {
				int len = strlen(bufRoundTime);
				bufRoundTime[len] = (char)key;
				bufRoundTime[len + 1] = '\0';
			}
			if (IsKeyPressed(KEY_BACKSPACE)) {
				if (focus == 1 && strlen(bufLine) > 0) bufLine[strlen(bufLine) - 1] = '\0';
				if (focus == 2 && strlen(bufColumn) > 0) bufColumn[strlen(bufColumn) - 1] = '\0';
				if (focus == 3 && strlen(bufRoundTime) > 0) bufRoundTime[strlen(bufRoundTime) - 1] = '\0';
			}
			// 回车应用
			if (IsKeyPressed(KEY_ENTER)) {
				if (focus == 1 && strlen(bufLine) > 0) {
					line = atoi(bufLine); setupdata.mapx = line;
					SendSignal(SETUP_DATA, 0);
				}
				if (focus == 2 && strlen(bufColumn) > 0) {
					column = atoi(bufColumn);
					setupdata.mapy = column;
					SendSignal(SETUP_DATA, 0);
				}
				if (focus == 3 && strlen(bufRoundTime) > 0) { inputRoundTime = atoi(bufRoundTime); 
				roundTime = inputRoundTime;
				}
				focus = 0;
			}
			Rectangle button_add_bot = DrawButtonAtCenter("Add Bot", screenWidth / 2, inputStartY + 300, 35, BLACK, WHITE, GeneralsGreen);
			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, button_add_bot)) {
				SendSignal(BOT_ADD, 0);
			}




		}

		if (game_status == READY) {
			DrawTextAtCenter("All Ready, Starting...", screenWidth / 2, screenHeight / 2, 35, WHITE);
		}

		if (game_status == START) {
			// gaming横向居中纵向靠顶
			int boxW = 400, boxH = 80;
			int x = (screenWidth - boxW) / 2, y = 40;
			DrawRectangle(x, y, boxW, boxH, (Color) { 255, 255, 255, 255 });
			DrawRectangleLines(x, y, boxW, boxH, (Color) { 180, 180, 180, 255 });
			DrawText("Gaming", x + (boxW - MeasureText("Gaming", 48)) / 2, y + 16, 48, (Color) { 40, 40, 40, 255 });

			// 等待加入
			if (waitcount > 0) {
				int boxW2 = 400, boxH2 = 60;
				int x2 = (screenWidth - boxW2) / 2, y2 = y + boxH + 30;
				DrawRectangle(x2, y2, boxW2, boxH2, (Color) { 255, 255, 255, 255 });
				DrawRectangleLines(x2, y2, boxW2, boxH2, (Color) { 180, 180, 180, 255 });
				char buf[64];
				snprintf(buf, sizeof(buf), "Waiting to join: %d", waitcount);
				DrawText(buf, x2 + (boxW2 - MeasureText(buf, 32)) / 2, y2 + 14, 32, (Color) { 40, 40, 40, 255 });
			}
		}

		EndDrawing();
	}
	CloseWindow();
}
void DrawTextAtCenter(char* text, int x, int y, int fontsize, Color color) {
	Vector2 textsize = MeasureTextEx(font, text, fontsize, TEXT_SPACING);
	DrawTextEx(font, text, (Vector2) { (int)(x - textsize.x / 2), (int)(y - textsize.y / 2) }, fontsize, TEXT_SPACING, color);
}
void DrawFilledRectangle(int startx, int starty, int width, int height, char* text, int fontsize, Color lineColor, Color fillColor, Color textcolor) {
	Rectangle background = { startx,starty,width,height };
	DrawRectangleRec(background, fillColor);
	DrawRectangleLinesEx(background, TABLE_LINE_WIDTH, lineColor);
	Vector2 textsize = MeasureTextEx(font, text, fontsize, TEXT_SPACING);
	while (textsize.x > width) {
		fontsize--;
		textsize = MeasureTextEx(font, text, fontsize, TEXT_SPACING);
	}
	DrawTextAtCenter(text, startx + width / 2, starty + height / 2, fontsize, textcolor);
}