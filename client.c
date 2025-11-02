#include <raylib.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <generals.h>
#include <pthread.h>



int fps = 60;

extern int** generatemap(int x,int y);
extern bool isinmap(Vector2 mouseInWorld);
extern int chosenColumn(float mouse_Y);
extern int chosenLine(float mouse_X);
extern void drawhighlight(int mapx, int mapy,Color color,int islit);
extern void DrawL1Block(Block** mapL1, int x, int y, int player);
int MoveOneStep();
void DrawArrow(Move move);
void DrawArrowWithShader(Texture arrow, int x, int y);
int Renderer();
int MapDownload(SOCKET sock);
int MoveUpload(SOCKET sock);
char isapplied = 0;
bool NeedToUploadMove = true;
bool NeedToRefreshPage = true;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

Texture tmountain;
Texture tcity;
Texture tcrown;
Color Cplayer1 = { 39,146,255,255 };
Texture arrowup, arrowdown, arrowleft, arrowright;
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
int line = LINE;
int column = COLUMN;
Move movelist[300] = { 0 };
int movecount = 0;
Block** mapL1;
pthread_t recv_fd,send_fd;
int roundn = 1;
int playernum=1;
Block* mapbuffer;

int main(void)
{
	

	WSADATA wsadata;
	int wsares = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (wsares != 0) return 1;
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);
	inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
	printf("waiting for server to accept...\n");
	connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	printf("error:%d\n", WSAGetLastError());



	mapL1 = malloc(line * sizeof(Block*));
	for (int i = 0; i < line; i++) {
		mapL1[i] = malloc(column * sizeof(Block));
	}
	mapbuffer = malloc(line * column * sizeof(Block));
	pthread_create(&recv_fd, NULL, MapDownload, sock);
	pthread_create(&send_fd, NULL, MoveUpload, sock);

	Renderer();

	for (int i = 0; i < line; i++) free(mapL1[i]);
	free(mapL1);
	pthread_cancel(recv_fd);
	pthread_cancel(send_fd);
	pthread_mutex_destroy(&mutex);
	closesocket(sock);
	WSACleanup();
	printf("cleaned\n");

	return 0;
}

void DrawArrow(Move move) {
	int x = -65 * line + 130 * move.startx - 130;
	int y = -65 * column + 130 * move.starty - 130;
	if (move.endx == move.startx - 1) DrawArrowWithShader(arrowleft, x, y);
	if (move.endx == move.startx + 1) DrawArrowWithShader(arrowright, x, y);
	if (move.endy == move.starty - 1) DrawArrowWithShader(arrowup,  x, y);
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
	Camera2D camera = { 0 };
	camera.target = (Vector2){ 0,0 };
	camera.rotation = 0.0f;
	camera.zoom = 0.5f;
	if (line <= 10 && column <= 10) camera.zoom = 1.0f;
	camera.offset = (Vector2){ screenWidth / 2,screenHeight / 2 };
	Color background = { 34,34,34,255 };
	Color map_unlit = { 57,57,57,255 };
	Color GeneralsGreen = { 0,128,128,255 };
	Color selected_WHITE = { 255,255,255,80 };
	Rectangle map_layer0 = { -line * 65,-column * 65,line * 130,column * 130 };
	tmountain = LoadTextureFromImage(mountain);
	tcity = LoadTextureFromImage(LoadImage("city.png"));
	tcrown = LoadTextureFromImage(crown);
	arrowup = LoadTextureFromImage(LoadImage("arrowup.png"));
	arrowdown = LoadTextureFromImage(LoadImage("arrowdown.png"));
	arrowleft = LoadTextureFromImage(LoadImage("arrowleft.png"));
	arrowright = LoadTextureFromImage(LoadImage("arrowright.png"));

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
		pthread_mutex_lock(&mutex);
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
				if (mapL1[x - 1][y - 1].owner == 1 || (moveable && (x - lastchosenblock[0]) * (x - lastchosenblock[0]) + (y - lastchosenblock[1]) * (y - lastchosenblock[1]) == 1)) {
					chosenblock[0] = x;
					chosenblock[1] = y;
					displayHighLight = true;
				}
			}
		}
		if (IsKeyPressed(KEY_LEFT)) if (chosenblock[0] > 0) { chosenblock[0]--; NeedToRefreshPage = true; }
		if (IsKeyPressed(KEY_RIGHT)) if (chosenblock[0] < line) { chosenblock[0]++; NeedToRefreshPage = true; }
		if (IsKeyPressed(KEY_UP)) if (chosenblock[1] > 0) { chosenblock[1]--; NeedToRefreshPage = true; }
		if (IsKeyPressed(KEY_DOWN)) if (chosenblock[1] < column) { chosenblock[1]++; NeedToRefreshPage = true; }
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
					pthread_cond_signal(&cond);
				}
				else moveable = false;
			}
			if (mapL1[chosenblock[0] - 1][chosenblock[1] - 1].owner == 1) moveable = true;
		}
		lastchosenblock[0] = chosenblock[0];
		lastchosenblock[1] = chosenblock[1];

		if (IsWindowResized()) NeedToRefreshPage = true;

		BeginDrawing();
		if (NeedToRefreshPage) {
			ClearBackground(background);
			BeginMode2D(camera);

			//layer0:basic map(unlit)
			DrawRectangleRec(map_layer0, map_unlit);

			for (int i = 1; i <= line; i++) for (int j = 1; j <= column; j++) {
				if (mapL1[i - 1][j - 1].type != PLAIN) DrawTexture(tobstacle, i * 130 - 130 - line * 65 + 15, -130 - column * 65 + 15 + j * 130, WHITE);
			}
			//layer1:highlight
			for (int i = 1; i <= line; i++) for (int j = 1; j <= column; j++) {
				DrawL1Block(mapL1, i - 1, j - 1, playernum);
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

			NeedToRefreshPage = true;
			pthread_mutex_unlock(&mutex);
		}
		else pthread_mutex_unlock(&mutex);
		EndDrawing();


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
int MapDownload(SOCKET sock) {
	while (true) {
		//printf("trying to download map...\n");
		recv(sock, mapbuffer, line * column * sizeof(Block), MSG_WAITALL);
		char isappliedtmp,roundntmp;
		recv(sock, &isappliedtmp, sizeof(char), MSG_WAITALL);
		recv(sock, &roundntmp, sizeof(int), MSG_WAITALL);
		pthread_mutex_lock(&mutex);
		//printf("download success!\n");
		NeedToRefreshPage = true;
		for (int i = 0; i < line; i++) for (int j = 0; j < column; j++) mapL1[i][j] = mapbuffer[i * column + j];
		roundn = roundntmp;
		if(isappliedtmp>0){
			movecount -= isappliedtmp;
			for (int i = 0; i < movecount; i++) {
				movelist[i] = movelist[i + isappliedtmp];
			}
			NeedToUploadMove = true;
			pthread_cond_signal(&cond);
		}
		pthread_mutex_unlock(&mutex);
	}
	return 0;
}
int MoveUpload(SOCKET sock) {
	while (true) {
		pthread_mutex_lock(&mutex);
		printf("available\n");
		while (NeedToUploadMove == false || movecount == 0) {
			pthread_cond_wait(&cond, &mutex);
		}
		printf("try to upload move\n");
		Move tmpmove = movelist[0];
		while ((mapL1[tmpmove.startx - 1][tmpmove.starty - 1].owner != playernum || mapL1[tmpmove.endx - 1][tmpmove.endy - 1].type == MOUNTAIN) && movecount>0) {
			movecount--;
			for (int i = 0; i < movecount; i++) movelist[i] = movelist[i + 1];
			tmpmove = movelist[0];
		}
		printf("now jumped\n");
		int tmpcnt = movecount;
		pthread_mutex_unlock(&mutex);
		if(tmpcnt>0) send(sock, &tmpmove, sizeof(Move), 0);
		pthread_mutex_lock(&mutex);
		if(tmpcnt>0) NeedToUploadMove = false;
		pthread_mutex_unlock(&mutex);
	}
}