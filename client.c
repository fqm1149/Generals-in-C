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
thrd_t recv_fd,send_fd,ctrl_fd;
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
void SendSignal(char msgtype, char cmd) {
	messageType = msgtype;
	currentCMD = cmd;
	NeedToSendData = true;
	cnd_signal(&cond);
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
	thrd_create(&ctrl_fd, Control, (void*)sock);

	Renderer();
	closesocket(sock);

	// 请求线程优雅退出
	running = false;
	cnd_signal(&cond); // 唤醒可能等待的线程

	// 等待线程结束（如果已创建）
	thrd_join(recv_fd, NULL);
	thrd_join(send_fd, NULL);
	thrd_join(ctrl_fd, NULL);

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
			DrawTextAtCenter("Disconnected", width / 2, height / 2-60, 35, WHITE);
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
			BeginDrawing();
			ClearBackground(background);
			DrawTextAtCenter("Connected", width / 2, height / 2-60, 35, WHITE);
			Rectangle readyButton;
			if (gameReady == 0) readyButton = DrawButtonAtCenter(TextFormat("Ready (%d/%d)", setupdata.readynum,setupdata.totalnum), width / 2, height / 2 + 30, 35, BLACK, WHITE, GeneralsGreen);
			else readyButton = DrawButtonAtCenter(TextFormat("Ready (%d/%d)", setupdata.readynum, setupdata.totalnum), width / 2, height / 2 + 30, 35, WHITE, GeneralsGreen, BLACK);
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
		}else 	mtx_unlock(&mutex);

	}
	return 0;
}
int Control(void* arg) {
	SOCKET sock = (SOCKET)arg;
	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
	while (running) {
		mtx_lock(&mutex);
		while (tryconnect == 0 && running) cnd_wait(&cond, &mutex);
		mtx_unlock(&mutex);
		if (!running) break;
		if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) >= 0) break;
	}
	if (!running) return 0;
	game_status = WAITING_FOR_START;
	thrd_create(&send_fd, send_to_server, (void*)sock);
	thrd_create(&recv_fd, recv_from_server, (void*)sock);
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