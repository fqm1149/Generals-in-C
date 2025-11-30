#define _CRT_SECURE_NO_WARNINGS
#include <raylib.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <generals.h>
#include <threads.h>

int fps = 60;

Rectangle map_layer0;
Camera2D camera = { 0 };
extern bool isinmap(Vector2 mouseInWorld);
extern int chosenColumn(float mouse_Y);
extern int chosenLine(float mouse_X);
extern void drawhighlight(int mapx, int mapy,Color color,int islit);
extern void DrawL1Block(Block** mapL1, int x, int y,int player,bool islit);
int MoveOneStep();
void DrawArrow(Move move);
void DrawArrowWithShader(Texture arrow, int x, int y);
void DrawTextAtCenter(char* text, int x, int y, int fontsize, Color color);
Rectangle DrawButtonAtCenter(char* text, int x, int y, int fontsize, Color textcolor, Color button, Color shader);
void DrawCountTable();
void DrawFilledRectangle(int startx, int starty, int width, int height, char* text, int fontsize, Color lineColor, Color Fillcolor, Color textcolor);
int Renderer();
int recv_from_server(void* arg);
int send_to_server(void* arg);
int Control(void* arg);
void GetRank();
char isapplied = 0;
char messageType,currentCMD;
bool NeedToSendData = true;
bool NeedToRefreshPage = true;
bool NeedToRefreshServList;
cnd_t cond;
SetupData setupdata;
Texture tmountain;
Texture tcity;
Texture tcrown;
Color playercolor[8];
Texture arrowup, arrowdown, arrowleft, arrowright;
mtx_t mutex;
int line;
int column;
Move movelist[300] = { 0 };
int movecount = 0;
Block** mapL1;
thrd_t thrd_recv,thrd_send,thrd_ctrl;
int roundn = 1;
int playernum;//从1开始
Block* mapbuffer;
char game_status = DISCONNECTED;
int tryconnect = 1;
char gameReady = 0;
StatisticData statisticData;
int width, height;
Font font;
int rank[8];//rank i 表示第i名是谁
bool running = true;
bool showmap = false;
bool islose = false;
bool rd_var = false;//搜索配套cond使用
ServerInfo serverList[4];
int detected_server_num;
int chosen_serv_num = -1;//从0开始
void SendSignal(char msgtype, char cmd) {
	messageType = msgtype;
	currentCMD = cmd;
	NeedToSendData = true;
	cnd_signal(&cond);
}
int GetServerInfo(const char* buffer) {
	char* token;
	char copy[1024];
	strcpy(copy, buffer);
	// 格式: "GEN_SERVER|端口|服务器名|版本"
	token = strtok(copy, "|");
	if (token && strcmp(token, "GEN_SERVER") == 0) {
		token = strtok(NULL, "|");
		if (token) serverList[detected_server_num].port = atoi(token);
		token = strtok(NULL, "|");
		if (token) strncpy(serverList[detected_server_num].name, token, sizeof(serverList[detected_server_num].name) - 1);
		token = strtok(NULL, "|");
		if (token) serverList[detected_server_num].ver = atoi(token);
		for (int i = 0; i < detected_server_num; i++) if (strcmp(serverList[i].ip, serverList[detected_server_num].ip) == 0) return 0;
		detected_server_num++;
		return 1;
	}
	return 0;
}
int main(void)
{
	// 初始化互斥与条件变量
	mtx_init(&mutex, mtx_plain);
	cnd_init(&cond);

	//初始化WSA
	WSADATA wsadata;
	int wsares = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (wsares != 0) return 1;
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

	thrd_create(&thrd_ctrl, Control, (void*)sock);

	Renderer();
	closesocket(sock);

	// 请求线程优雅退出
	running = false;
	cnd_broadcast(&cond); // 唤醒可能等待的线程

	// 等待线程结束（如果已创建）
	thrd_join(thrd_recv, NULL);
	thrd_join(thrd_send, NULL);
	thrd_join(thrd_ctrl, NULL);

	// 清理战场
	if (game_status!=WAITING_FOR_START && game_status!=WAITING_FOR_END) {
		for (int i = 0; i < line; i++) free(mapL1[i]);
		free(mapL1);
		free(mapbuffer);
	}
	mtx_destroy(&mutex);
	cnd_destroy(&cond);
	WSACleanup();
	printf("cleaned\n");

	return 0;
}

void DrawArrow(Move move) {
	int x = -65 * line + 130 * move.startx - 130;
	int y = -65 * column + 130 * move.starty - 130;
	if (move.endx == move.startx - 1) DrawArrowWithShader(arrowleft, x, y);
	if (move.endx == move.startx + 1) DrawArrowWithShader(arrowright, x, y);
	if (move.endy == move.starty - 1) DrawArrowWithShader(arrowup, x, y);
	if (move.endy == move.starty + 1) DrawArrowWithShader(arrowdown, x, y);
}
void DrawArrowWithShader(Texture arrow, int x, int y) {
	DrawTexture(arrow, x + 2, y + 2, BLACK);
	DrawTexture(arrow, x + 2, y - 2, BLACK);
	DrawTexture(arrow, x - 2, y + 2, BLACK);
	DrawTexture(arrow, x - 2, y - 2, BLACK);
	DrawTexture(arrow, x, y, WHITE);
}


int Renderer() {
	Image obstacle = LoadImage("obstacle.png");
	Image mountain = LoadImage("mountain.png");
	Image crown = LoadImage("crown.png");
	const int screenWidth = 1440;
	const int screenHeight = 1440;
	SetConfigFlags(FLAG_MSAA_4X_HINT);
	SetTraceLogLevel(LOG_NONE);
	//SetConfigFlags(FLAG_WINDOW_HIGHDPI);
	InitWindow(screenWidth, screenHeight, "Generals_re");
	SetWindowState(FLAG_WINDOW_RESIZABLE);
	SetTargetFPS(fps);
	camera.target = (Vector2){ 0,0 };
	camera.rotation = 0.0f;
	camera.zoom = 0.5f;
	camera.offset = (Vector2){ screenWidth / 2,screenHeight / 2 };
	Color background = { 34,34,34,255 };
	Color map_unlit = { 57,57,57,255 };
	Color GeneralsGreen = { 0,128,128,255 };
	Color selected_WHITE = { 255,255,255,80 };
	tmountain = LoadTextureFromImage(mountain);
	tcity = LoadTextureFromImage(LoadImage("city.png"));
	tcrown = LoadTextureFromImage(crown);
	arrowup = LoadTextureFromImage(LoadImage("arrowup.png"));
	arrowdown = LoadTextureFromImage(LoadImage("arrowdown.png"));
	arrowleft = LoadTextureFromImage(LoadImage("arrowleft.png"));
	arrowright = LoadTextureFromImage(LoadImage("arrowright.png"));
	font = GetFontDefault();

	Texture tobstacle = LoadTextureFromImage(obstacle);
	bool displayHighLight = false;
	bool displaychosen = false;
	bool movemode = false;//UI的移动
	bool moveable = false;//游玩逻辑的移动
	int chosenblock[2] = { -1,-1 };
	int lastchosenblock[2] = { -1,-1 };
	bool mouseOnText = false;
	int framecount = 0;//名字产生修改后60帧发送给server
	bool needToRefreshName = false;
	Rectangle connectButton[4],refreshButton;

	// 主游戏循环
	while (!WindowShouldClose())    //关闭窗口或者按ESC键时返回true
	{
		width = GetScreenWidth();
		height = GetScreenHeight();
		if (!running) break; // 响应全局退出
		if (game_status == START){
			mtx_lock(&mutex);
			memset(&statisticData, 0, sizeof(StatisticData));
			float wheelDelta = GetMouseWheelMove();
			Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);
			if (wheelDelta != 0) {
				NeedToRefreshPage = true;
				float zoom_old = camera.zoom;
				camera.zoom += 0.08f * wheelDelta;
				if (camera.zoom > 3.0f) camera.zoom = 3.0f;
				if (camera.zoom < 0.2f) camera.zoom = 0.2f;
				float zoomDelta = zoom_old / camera.zoom;
				camera.target.x = zoomDelta * camera.target.x + (1 - zoomDelta) * mouseWorldPos.x;
				camera.target.y = zoomDelta * camera.target.y + (1 - zoomDelta) * mouseWorldPos.y;
			}

			if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
				if (isinmap(mouseWorldPos)) {
					camera.offset.x += GetMouseDelta().x;
					camera.offset.y += GetMouseDelta().y;
				}
				if (GetMouseDelta().x > 1 || GetMouseDelta().y > 1) { movemode = true; NeedToRefreshPage = true; }
			}

			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
				float mouseX = GetMousePosition().x;
				float mouseY = GetMousePosition().y;
				if (mouseX >= 20 && mouseX <= 290 && mouseY >= 20 && mouseY <= 90) {

					{ displayHighLight = false; NeedToRefreshPage = true; }
				}
				else {

					if (isinmap(mouseWorldPos)) {
						movemode = false;
						displaychosen = true;
						NeedToRefreshPage = true;
					}
					else {
						displayHighLight = false;
						NeedToRefreshPage = true;
					}

				}
			}

			if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
				displaychosen = false;
				NeedToRefreshPage = true;

				if (!movemode && isinmap(mouseWorldPos)) {
					int x = chosenLine(mouseWorldPos.x), y = chosenColumn(mouseWorldPos.y);
					if (mapL1[x - 1][y - 1].owner == playernum || (moveable && (x - lastchosenblock[0]) * (x - lastchosenblock[0]) + (y - lastchosenblock[1]) * (y - lastchosenblock[1]) == 1)) {
						chosenblock[0] = x;
						chosenblock[1] = y;
						displayHighLight = true;
					}
				}
			}
			if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) if (chosenblock[0] > 0) { chosenblock[0]--; NeedToRefreshPage = true; }
			if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) if (chosenblock[0] < line) { chosenblock[0]++; NeedToRefreshPage = true; }
			if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) if (chosenblock[1] > 0) { chosenblock[1]--; NeedToRefreshPage = true; }
			if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) if (chosenblock[1] < column) { chosenblock[1]++; NeedToRefreshPage = true; }
			if (IsKeyPressed(KEY_Z)) if (movecount > 0) { movecount--; NeedToRefreshPage = true; }

			if (!displayHighLight) {
				chosenblock[0] = -1;
				chosenblock[1] = -1;
				moveable = false;
				lastchosenblock[0] = -1;
				lastchosenblock[1] = -1;
			}

			if (displayHighLight) {
				if (moveable && (chosenblock[0] != lastchosenblock[0] || chosenblock[1] != lastchosenblock[1])) {
					//printf("cal=%d\n", (chosenblock[0] - lastchosenblock[0]) * (chosenblock[0] - lastchosenblock[0]) + (chosenblock[1] - lastchosenblock[1]) * (chosenblock[1] - lastchosenblock[1]));
					if ((chosenblock[0] - lastchosenblock[0]) * (chosenblock[0] - lastchosenblock[0]) + (chosenblock[1] - lastchosenblock[1]) * (chosenblock[1] - lastchosenblock[1]) == 1) {
						movelist[movecount].startx = lastchosenblock[0];
						movelist[movecount].starty = lastchosenblock[1];
						movelist[movecount].endx = chosenblock[0];
						movelist[movecount].endy = chosenblock[1];
						movecount++;
						if (movecount == 1) SendSignal(UPLOAD_MOVE, 0);
					}
					else moveable = false;
				}
				if (mapL1[chosenblock[0] - 1][chosenblock[1] - 1].owner == playernum) moveable = true;
			}
			lastchosenblock[0] = chosenblock[0];
			lastchosenblock[1] = chosenblock[1];

			if (IsWindowResized()) NeedToRefreshPage = true;

			BeginDrawing();
			ClearBackground(background);
			BeginMode2D(camera);

			//layer0:basic map(unlit)
			DrawRectangleRec(map_layer0, map_unlit);

			for (int i = 1; i <= line; i++) for (int j = 1; j <= column; j++) {
				if (mapL1[i - 1][j - 1].type != PLAIN) DrawTexture(tobstacle, i * 130 - 130 - line * 65 + 15, -130 - column * 65 + 15 + j * 130, WHITE);
				DrawL1Block(mapL1, i - 1, j - 1, playernum,false);
				if (mapL1[i - 1][j - 1].owner > 0) {
					statisticData.land[mapL1[i - 1][j - 1].owner - 1]++;
					statisticData.army[mapL1[i - 1][j - 1].owner - 1] += mapL1[i - 1][j - 1].num;
				}
			}
			GetRank();
			if (statisticData.army[playernum - 1] == 0) {
				islose = true;
				game_status = LOSE;
				SendSignal(CLIENT_CMD, CLIENT_LOSE);
			}

			//layer2:arrows
			for (int i = 0; i < movecount; i++) {
				DrawArrow(movelist[i]);
			}

			//layer3:select-block
			if (displayHighLight) drawhighlight(chosenblock[0], chosenblock[1], WHITE, 1);
			if (displaychosen) drawhighlight(chosenLine(mouseWorldPos.x), chosenColumn(mouseWorldPos.y), selected_WHITE, 0);
			EndMode2D();

			//layer4:fixed buttons
			DrawText(TextFormat("Actual FPS: %d", GetFPS()), 20, 120, 35, WHITE);
			DrawRectangle(30, 30, 270, 70, GeneralsGreen);
			DrawRectangle(20, 20, 270, 70, WHITE);
			DrawText(TextFormat("Round %d", roundn), 35, 35, 35, BLACK);
			DrawCountTable();
			mtx_unlock(&mutex);
			EndDrawing();
		}
		if (game_status == DISCONNECTED) {
			if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) { tryconnect = 1; cnd_signal(&cond); }
			BeginDrawing();
			ClearBackground(background);
			DrawTextAtCenter("Disconnected", width / 2, height / 2-220, 35, WHITE);
			refreshButton = DrawButtonAtCenter("Refresh", width / 2, height / 2 - 120, 35, BLACK, WHITE, GeneralsGreen);
			if (CheckCollisionPointRec(GetMousePosition(), refreshButton) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
				NeedToRefreshServList = true;
				rd_var = true;
				cnd_signal(&cond);
				EndDrawing();
				continue;
			}
			for (int i = 0;i < detected_server_num; i++) {
				DrawRectangle(width / 2 - 300, height / 2 - 60 + 120 * i, 600, 120, WHITE);
				DrawText(TextFormat("%s", serverList[i].name), width / 2 - 270, height / 2 - 50 + 120 * i, 50, BLACK);
				DrawText(TextFormat("%s:%d", serverList[i].ip, serverList[i].port), width / 2 - 270, height / 2 + 120 * i, 30, GRAY);
				connectButton[i] = DrawButtonAtCenter("Connect", width / 2 + 250, height / 2 + 120 * i, 35, WHITE, GeneralsGreen, BLACK);
				if (CheckCollisionPointRec(GetMousePosition(), connectButton[i]) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
					chosen_serv_num = i; cnd_signal(&cond);
				}
			}
			EndDrawing();
		}
		if (game_status == WAITING_FOR_END) {
			BeginDrawing();
			ClearBackground(background);
			DrawTextAtCenter("Waiting for current game to end", width / 2, height / 2 - 60, 35, WHITE);
			EndDrawing();
		}
		if (game_status == WAITING_FOR_START) {
			GetRank();
			Rectangle textbox = (Rectangle){ width / 2 - 150,height / 2 - 60,300,50 };
			BeginDrawing();
			ClearBackground(background);
			DrawTextAtCenter("Connected", width / 2, height / 2-90, 35, WHITE);
			if (CheckCollisionPointRec(GetMousePosition(), textbox)) {
				int letterCount = strlen(setupdata.playername[playernum - 1]);
				SetMouseCursor(MOUSE_CURSOR_IBEAM);
				DrawFilledRectangle(textbox.x, textbox.y, textbox.width, textbox.height, setupdata.playername[playernum-1], 35, GeneralsGreen, WHITE, BLACK);
				int key = GetCharPressed();
				while (key > 0)
				{
					if ((key >= 32) && (key <= 125) && letterCount <= 9)
					{
						setupdata.playername[playernum - 1][letterCount] = (char)key;
						setupdata.playername[playernum - 1][letterCount + 1] = '\0'; 
						letterCount++;
						needToRefreshName = true;
						framecount = 0;
					}

					key = GetCharPressed(); 
				}

				if (IsKeyPressed(KEY_BACKSPACE))
				{
					letterCount--;
					if (letterCount < 0) letterCount = 0;
					setupdata.playername[playernum - 1][letterCount] = '\0';
					needToRefreshName = true;
					framecount = 0;
				}
				
			}
			else {
				SetMouseCursor(MOUSE_CURSOR_DEFAULT);
				DrawFilledRectangle((int)textbox.x, (int)textbox.y, (int)textbox.width, (int)textbox.height, setupdata.playername[playernum - 1], 35, DARKGRAY, WHITE, BLACK);
			}
			if (needToRefreshName) {
				framecount++;
				if (framecount >= 60) {
					framecount = 0;
					SendSignal(UPLOAD_NAME, 0);
					needToRefreshName = 0;
				}
			}


			Rectangle readyButton;
			if (gameReady == 0) readyButton = DrawButtonAtCenter(TextFormat("Ready (%d/%d)", setupdata.readynum,setupdata.totalnum), width / 2, height / 2 + 60, 35, BLACK, WHITE, GeneralsGreen);
			else readyButton = DrawButtonAtCenter(TextFormat("Ready (%d/%d)", setupdata.readynum, setupdata.totalnum), width / 2, height / 2 + 60, 35, WHITE, GeneralsGreen, BLACK);
			if (CheckCollisionPointRec(GetMousePosition(), readyButton)) {
				if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
					gameReady ^= 1;
					SendSignal(CLIENT_CMD, gameReady);
				}
				DrawRectangleRec(readyButton, (Color) { 0, 0, 0, 64 });
			}
			DrawCountTable();
			EndDrawing();
		}
		if (game_status == READY) {
			BeginDrawing();
			ClearBackground(background);
			DrawTextAtCenter("Game Starting", width / 2, height / 2 - 30, 35, WHITE);
			EndDrawing();
		}
		if (game_status == LOSE) {
			memset(&statisticData, 0, sizeof(StatisticData));
			mtx_lock(&mutex);
			BeginDrawing();
			ClearBackground(background);
			BeginMode2D(camera);
			
			//layer0:basic map(unlit)
			DrawRectangleRec(map_layer0, map_unlit);

			for (int i = 1; i <= line; i++) for (int j = 1; j <= column; j++) {
				if (mapL1[i - 1][j - 1].type != PLAIN) DrawTexture(tobstacle, i * 130 - 130 - line * 65 + 15, -130 - column * 65 + 15 + j * 130, WHITE);
				if (mapL1[i - 1][j - 1].owner > 0) {
					statisticData.land[mapL1[i - 1][j - 1].owner - 1]++;
					statisticData.army[mapL1[i - 1][j - 1].owner - 1] += mapL1[i - 1][j - 1].num;
				}
			}
			GetRank();
			EndMode2D();
			//layer4:fixed buttons
			DrawText(TextFormat("Actual FPS: %d", GetFPS()), 20, 120, 35, WHITE);
			DrawRectangle(30, 30, 270, 70, GeneralsGreen);
			DrawRectangle(20, 20, 270, 70, WHITE);
			DrawText(TextFormat("Round %d", roundn), 35, 35, 35, BLACK);
			DrawCountTable();
			DrawRectangle(width/2 - 250, height/2 - 175, 500, 350, WHITE);
			DrawTextAtCenter("YOU LOSE!", width / 2, height / 2 - 50, 50, BLACK);
			//Rectangle exitButton = DrawButtonAtCenter("Go Back", width / 2, height / 2 + 50, 35, WHITE, GeneralsGreen, BLACK);
			mtx_unlock(&mutex);
			EndDrawing();

		}
		if (game_status == ENDGAME) {
			displayHighLight = false;
			displaychosen = false;
			movemode = false;
			moveable = false;
			chosenblock[0] = -1;
			chosenblock[1] = -1;
			lastchosenblock[0] = -1;
			lastchosenblock[1] = -1;
			mtx_lock(&mutex);
			memset(&statisticData, 0, sizeof(StatisticData));
			BeginDrawing();
			ClearBackground(background);
			BeginMode2D(camera);

			//layer0:basic map(unlit)
			DrawRectangleRec(map_layer0, map_unlit);

			for (int i = 1; i <= line; i++) for (int j = 1; j <= column; j++) {
				if (mapL1[i - 1][j - 1].type != PLAIN) DrawTexture(tobstacle, i * 130 - 130 - line * 65 + 15, -130 - column * 65 + 15 + j * 130, WHITE);
				DrawL1Block(mapL1, i - 1, j - 1, playernum,true);
				if (mapL1[i - 1][j - 1].owner > 0) {
					statisticData.land[mapL1[i - 1][j - 1].owner - 1]++;
					statisticData.army[mapL1[i - 1][j - 1].owner - 1] += mapL1[i - 1][j - 1].num;
				}
			}
			GetRank();
			EndMode2D();
		

			//layer4:fixed buttons
			DrawText(TextFormat("Actual FPS: %d", GetFPS()), 20, 120, 35, WHITE);
			DrawRectangle(30, 30, 270, 70, GeneralsGreen);
			DrawRectangle(20, 20, 270, 70, WHITE);
			DrawText(TextFormat("Round %d", roundn), 35, 35, 35, BLACK);
			DrawCountTable();
			DrawRectangle(width / 2 - 250, height / 2 - 175, 500, 350, WHITE);
			if (islose)DrawTextAtCenter("YOU LOSE!", width / 2, height / 2 - 50, 50, BLACK);
			else DrawTextAtCenter("YOU WIN!", width / 2, height / 2 - 50, 50, BLACK);
			Rectangle exitButton = DrawButtonAtCenter("Go Back", width / 2, height / 2 + 50, 35, WHITE, GeneralsGreen, BLACK);
			if (CheckCollisionPointRec(GetMousePosition(), exitButton) && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
				game_status = WAITING_FOR_START;
			}
			mtx_unlock(&mutex);
			EndDrawing();
		}

	}

	//关闭窗口
	CloseWindow();
	UnloadTexture(tmountain);
	UnloadTexture(tobstacle);
	UnloadTexture(tcity);
	UnloadImage(mountain);
	UnloadImage(obstacle);
	UnloadTexture(tcrown);
	UnloadImage(crown);
	UnloadTexture(arrowdown);
	UnloadTexture(arrowup);
	UnloadTexture(arrowleft);
	UnloadTexture(arrowright);
	return 0;
}
int recv_from_server(void* arg) {
	SOCKET sock = (SOCKET)arg;
	while (running) {
		char msgType;
		if (recv(sock, &msgType, 1, MSG_WAITALL) <= 0) { game_status = DISCONNECTED; }
		if (msgType == MAP_DATA)
		{
			//printf("trying to download map...\n");
			recv(sock, mapbuffer, line * column * sizeof(Block), MSG_WAITALL);
			char isappliedtmp;
			int roundntmp;
			recv(sock, &isappliedtmp, sizeof(char), MSG_WAITALL);
			recv(sock, &roundntmp, sizeof(int), MSG_WAITALL);
			mtx_lock(&mutex);
			//printf("download success!\n");
			NeedToRefreshPage = true;
			for (int i = 0; i < line; i++) for (int j = 0; j < column; j++) mapL1[i][j] = mapbuffer[i * column + j];
			roundn = roundntmp;
			if (isappliedtmp > 0) {
				movecount -= isappliedtmp;
				for (int i = 0; i < movecount; i++) {
					movelist[i] = movelist[i + isappliedtmp];
				}
				SendSignal(UPLOAD_MOVE,0);
			}
			mtx_unlock(&mutex);
		}
		if (msgType == SERVER_CMD) {
			char serverCmd;
			recv(sock, &serverCmd, 1, MSG_WAITALL);
			printf("received message %d \n", (int)msgType);
			switch (serverCmd) {
			case GAME_START:
				game_status = START;
				gameReady = 0;
				islose = false;
				break;
			case GAME_LOSE:
				game_status = LOSE;
				break;
			case SHOW_MAP:
				game_status = ENDGAME;
				break;
			case GAME_READY:
				game_status = READY;
				line = setupdata.mapx;
				column = setupdata.mapy;
				map_layer0 = (Rectangle){ -line * 65,-column * 65,line * 130,column * 130 };
				if (line <= 10 && column <= 10) camera.zoom = 1.0f;
				mapL1 = malloc(line * sizeof(Block*));
				for (int i = 0; i < line; i++) {
					mapL1[i] = malloc(column * sizeof(Block));
				}
				mapbuffer = malloc(line * column * sizeof(Block));
				break;
			case WAIT_FOR_END:
				game_status = WAITING_FOR_END;
				break;
			case JOIN:
				game_status = WAITING_FOR_START;
			default:
				break;
			}
		}
		if (msgType == SETUP_DATA) {
			SetupData tmpsetup;
			recv(sock, &tmpsetup, sizeof(SetupData), MSG_WAITALL);
			mtx_lock(&mutex);
			setupdata = tmpsetup;
			memcpy(playercolor, setupdata.playercolor, 8 * sizeof(Color));
			playernum = setupdata.clientnum + 1;
			mtx_unlock(&mutex);
		}
	}
	return 0;
}
int send_to_server(void* arg) {
	SOCKET sock = (SOCKET)arg;
	while (running) {
		mtx_lock(&mutex);
		while (NeedToSendData == false && running) {
			cnd_wait(&cond, &mutex);
		}
		if (!running) { mtx_unlock(&mutex); break; }
		NeedToSendData = false;
		printf("try to send %d\n", (int)messageType);
		if (messageType == UPLOAD_MOVE && movecount > 0) {
			Move tmpmove = movelist[0];
			while ((mapL1[tmpmove.startx - 1][tmpmove.starty - 1].owner != playernum || mapL1[tmpmove.endx - 1][tmpmove.endy - 1].type == MOUNTAIN) && movecount > 0) {
				movecount--;
				for (int i = 0; i < movecount; i++) movelist[i] = movelist[i + 1];
				tmpmove = movelist[0];
			}
			int tmpcnt = movecount;
			mtx_unlock(&mutex);
			if (tmpcnt > 0) {
				char msgType = UPLOAD_MOVE;
				send(sock, &msgType, 1, 0);
				send(sock, &tmpmove, sizeof(Move), 0);
			}
		}
		else if (messageType == CLIENT_CMD) {
			char msgTpe = messageType;
			char tmpCMD = currentCMD;
			mtx_unlock(&mutex);
			send(sock, &messageType, 1, 0);
			send(sock, &currentCMD, 1, 0);
		}
		else if(messageType==UPLOAD_NAME){
			char msgTpe = messageType;
			char tmpname[20];
			strcpy(tmpname, setupdata.playername[playernum - 1]);
			mtx_unlock(&mutex);
			send(sock, &messageType, 1, 0);
			send(sock, tmpname, 20, 0);
		}
		else 	mtx_unlock(&mutex);

	}
	return 0;
}
int Control(void* arg) {
	SOCKET sock = (SOCKET)arg;
	struct sockaddr_in serv_addr, listen_addr, chosen_addr;
	SOCKET listen_fd;
	socklen_t addr_len = sizeof(listen_addr);
	char buffer[1024];
	int reuse_enable = 1;

	// 创建UDP socket
	if ((listen_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}

	// 设置地址重用
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
		&reuse_enable, sizeof(reuse_enable)) < 0) {
		perror("setsockopt reuseaddr failed");
	}

	
	struct timeval tv;
	tv.tv_sec = 2000;
	tv.tv_usec = 0;
	if (setsockopt(listen_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))<0) {
		perror("setsockopt timeval failed");
	}

	memset(&listen_addr, 0, sizeof(listen_addr));
	listen_addr.sin_family = AF_INET;
	listen_addr.sin_port = htons(BROADCAST_PORT);
	listen_addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(listen_fd, (struct sockaddr*)&listen_addr,
		sizeof(listen_addr)) < 0) {
		perror("bind failed");
		close(listen_fd);
		exit(EXIT_FAILURE);
	}

	printf("Client listening for broadcasts on port %d...\n", BROADCAST_PORT);

	while (chosen_serv_num < 0 && running) {
		memset(buffer, 0, sizeof(buffer));
		if (NeedToRefreshServList) { detected_server_num = 0; NeedToRefreshServList = 0; }
		// 接收广播消息
		int len = recvfrom(listen_fd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&serv_addr, &addr_len);

		if (len > 0) {
			buffer[len] = '\0';
			inet_ntop(AF_INET, &(serv_addr.sin_addr), serverList[detected_server_num].ip, INET_ADDRSTRLEN);
			GetServerInfo(buffer);
			printf("now serverlist num is %d\n", detected_server_num);
		}
		else printf("received nothing\n");

		//if (NeedToRefreshServList) { detected_server_num = 0; NeedToRefreshServList = 0; }
		mtx_lock(&mutex);
		while (rd_var == false && chosen_serv_num<0 && running) cnd_wait(&cond, &mutex);
		rd_var = false;
		mtx_unlock(&mutex);
	}

	closesocket(listen_fd);
	chosen_addr.sin_family = AF_INET;
	chosen_addr.sin_port = htons((unsigned short)serverList[chosen_serv_num].port);
	inet_pton(AF_INET, serverList[chosen_serv_num].ip, &chosen_addr.sin_addr);
	connect(sock, (struct sockaddr*)&chosen_addr, sizeof(chosen_addr));
	if (!running) return 0;
	game_status = WAITING_FOR_START;
	thrd_create(&thrd_send, send_to_server, (void*)sock);
	thrd_create(&thrd_recv, recv_from_server, (void*)sock);
	return 0;
}

void DrawTextAtCenter(char* text,int x,int y,int fontsize,Color color) {
	Vector2 textsize = MeasureTextEx(font, text, fontsize, TEXT_SPACING);
	DrawTextEx(font, text, (Vector2) { (int)(x - textsize.x / 2), (int)(y - textsize.y / 2) }, fontsize, TEXT_SPACING, color);
}
Rectangle DrawButtonAtCenter(char* text, int x, int y, int fontsize, Color textColor, Color buttonColor, Color shaderColor) {
	Vector2 textsize = MeasureTextEx(font, text, fontsize, TEXT_SPACING);
	Rectangle button = { x - textsize.x / 2 - 30,y - textsize.y / 2 - 20,textsize.x + 60,textsize.y + 40 };
	Rectangle shader = { x - textsize.x / 2 - 25,y - textsize.y / 2 - 15,textsize.x + 60,textsize.y + 40 };
	DrawRectangleRec(shader, shaderColor);
	DrawRectangleRec(button, buttonColor);
	DrawTextAtCenter(text, x, y, fontsize, textColor);
	return button;
}
void DrawCountTable() {
	DrawFilledRectangle(width - TABLE_LAND_WIDTH - TABLE_ARMY_WIDTH - TABLE_PLAYER_WIDTH, 0, TABLE_PLAYER_WIDTH, TABLE_HEIGHT, "Player", TABLE_FONTSIZE, BLACK, WHITE, BLACK);
	DrawFilledRectangle(width - TABLE_LAND_WIDTH - TABLE_ARMY_WIDTH, 0, TABLE_ARMY_WIDTH, TABLE_HEIGHT, "Army", TABLE_FONTSIZE, BLACK, WHITE, BLACK);
	DrawFilledRectangle(width - TABLE_LAND_WIDTH, 0, TABLE_LAND_WIDTH, TABLE_HEIGHT, "Land", TABLE_FONTSIZE, BLACK, WHITE, BLACK);
	for (int i = 1; i <= setupdata.totalnum; i++) {
		DrawFilledRectangle(width - TABLE_LAND_WIDTH - TABLE_ARMY_WIDTH - TABLE_PLAYER_WIDTH, TABLE_HEIGHT * i, TABLE_PLAYER_WIDTH, TABLE_HEIGHT, setupdata.playername[rank[i-1] - 1], TABLE_FONTSIZE, BLACK, setupdata.playercolor[rank[i-1] - 1], WHITE);
		DrawFilledRectangle(width - TABLE_LAND_WIDTH - TABLE_ARMY_WIDTH, TABLE_HEIGHT * i, TABLE_ARMY_WIDTH, TABLE_HEIGHT, TextFormat("%d", statisticData.army[rank[i-1] - 1]), TABLE_FONTSIZE, BLACK, WHITE, BLACK);
		DrawFilledRectangle(width - TABLE_LAND_WIDTH, TABLE_HEIGHT * i, TABLE_LAND_WIDTH, TABLE_HEIGHT, TextFormat("%d", statisticData.land[rank[i-1] - 1]), TABLE_FONTSIZE, BLACK, WHITE, BLACK);
	}
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
void GetRank() {
	for (int i = 0; i < 8; i++) rank[i] = i + 1;
	for (int i = 0; i < 8; i++) for (int j = i + 1; j < 8; j++) if (statisticData.army[rank[i]-1] < statisticData.army[rank[j]-1] || (statisticData.army[rank[i]-1] == statisticData.army[rank[j]-1] && statisticData.land[rank[i]-1] < statisticData.land[rank[j]-1])) {
		int tmp = rank[i];
		rank[i] = rank[j];
		rank[j] = tmp;
	}
}