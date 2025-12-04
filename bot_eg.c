#define _CRT_SECURE_NO_WARNINGS
#define GEN_BOT_DEV
#include <stdlib.h>
#include <generals.h>
#include <stdio.h>
#include <errno.h>
#include <windows.h>
#include <threads.h>
#include <stdbool.h>
StatisticData statisticData;
SetupData setupdata;
Block** mapL1;
Block* mapbuffer;
int line, column;
int playernum;//自己的编号，从1开始
char messageType, currentCMD;
char msgType,serverCmd;//接收使用
bool NeedToFreeMap = false;
bool islose = false;
//Bot Info
char bot_name[20]="TEST_BOT";


//Bot Core Func Claim
int bot_operate();


//Bot Memory Var




//Bot Export Func
__declspec(dllexport) int bot_function(BotData *botdata) {
	int port = botdata->port;
	char* cmd = botdata->cmd;
	WSADATA wsadata;
	int wsares = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (wsares != 0) return 1;
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serv_addr;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
	printf("bot waiting for server to accept...\n");
	connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	printf("error:%d\n", WSAGetLastError());
	messageType = CLIENT_CMD;
	currentCMD = CLIENT_READY;
	Sleep(50);
	send(sock, &messageType, 1, 0);
	send(sock, &currentCMD, 1, 0);
	while (*cmd!=BOT_EXIT) {
		recv(sock, &msgType, 1, MSG_WAITALL);
		if (msgType == SETUP_DATA) {
			recv(sock, &setupdata, sizeof(SetupData), MSG_WAITALL);
			playernum = setupdata.clientnum + 1;
			if (strcmp(setupdata.playername[playernum - 1], bot_name) != 0) {
				messageType = UPLOAD_NAME;
				send(sock, &messageType, 1, 0);
				send(sock, bot_name, 20, 0);
				printf("Upload Bot Name\n");
			}
		}
		if (msgType == SERVER_CMD) {
			recv(sock, &serverCmd, 1, MSG_WAITALL);
			if (serverCmd == SERVER_OFF) {
				if (NeedToFreeMap) {
					for (int i = 0; i < line; i++) free(mapL1[i]);
					free(mapL1);
					free(mapbuffer);
				}
				break;
			}
			switch (serverCmd) {
			case SHOW_MAP:
				for (int i = 0; i < line; i++) free(mapL1[i]);
				free(mapL1);
				free(mapbuffer);
				NeedToFreeMap = false;
				messageType = CLIENT_CMD;
				currentCMD = CLIENT_READY;
				Sleep(50);
				send(sock, &messageType, 1, 0);
				send(sock, &currentCMD, 1, 0);
				break;
			case GAME_READY:
				NeedToFreeMap = true;
				islose = false;
				memset(&statisticData, 0, sizeof(StatisticData));
				line = setupdata.mapx;
				column = setupdata.mapy;
				mapL1 = malloc(line * sizeof(Block*));
				for (int i = 0; i < line; i++) {
					mapL1[i] = malloc(column * sizeof(Block));
				}
				mapbuffer = malloc(line * column * sizeof(Block));
				break;
			default:
				break;
			}
		}
		if (msgType == MAP_DATA)
		{
			memset(&statisticData, 0, sizeof(StatisticData));
			recv(sock, mapbuffer, line * column * sizeof(Block), MSG_WAITALL);
			char isappliedtmp;
			int roundntmp;
			recv(sock, &isappliedtmp, sizeof(char), MSG_WAITALL);
			recv(sock, &roundntmp, sizeof(int), MSG_WAITALL);
			for (int i = 0; i < line; i++) for (int j = 0; j < column; j++) mapL1[i][j] = mapbuffer[i * column + j];
			for (int i = 1; i <= line; i++) for (int j = 1; j <= column; j++) {
				if (mapL1[i - 1][j - 1].owner > 0) {
					statisticData.land[mapL1[i - 1][j - 1].owner - 1]++;
					statisticData.army[mapL1[i - 1][j - 1].owner - 1] += mapL1[i - 1][j - 1].num;
				}
			}
			printf("now bot have army:%d\n", statisticData.army[playernum - 1]);
			if (statisticData.army[playernum - 1] == 0 && islose == false) {
				islose = true;
				messageType = CLIENT_CMD;
				currentCMD = CLIENT_LOSE;
				send(sock, &messageType, 1, 0);
				send(sock, &currentCMD, 1, 0);
				printf("Bot Lose!\n");
			}
			bot_operate();
		}
	}
	closesocket(sock);
	WSACleanup();
	printf("cleaned\n");

	return 0;
}


//Bot Core Func
int bot_operate() {

}