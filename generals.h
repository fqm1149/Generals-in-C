#ifndef GENERALS_H
#define GENERALS
#define WIN32_LEAN_AND_MEAN     
#define NOGDI                   
#define NOUSER 
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <raylib.h>
#pragma comment(lib, "ws2_32.lib")
#define PORT 8029
#define BROADCAST_PORT 8092
#define DEFAULT_SERVER_PORT 12345

#define MOUNTAIN -1
#define CITY 1
#define PLAIN 0
#define CROWN -3

#define MOVEUP 1
#define MOVEDOWN 2
#define MOVELEFT 3
#define MOVERIGHT 4

#define LIT_PLAIN (Color){220,220,220,255}
#define LIT_MOUNTAIN (Color){187,187,187,255}
#define LIT_CITY (Color){128,128,128,255}
#define DISCONNECT_COLOR (Color){80,80,80,255}

#define FONTSIZE_ARMY 100
#define CITY_PERCENTAGE 20

#define LINE 20
#define COLUMN 19
//server->client datatype:
#define MAP_DATA 0
#define SETUP_DATA 1
#define SERVER_CMD 2
#define TEST_ALIVE 3
//server command type:
#define GAME_START 0
#define GAME_LOSE 1
#define SHOW_MAP 2
#define GAME_READY 3
#define WAIT_FOR_END 4
#define JOIN 5
//client->server datatype:
#define UPLOAD_MOVE 0
#define CLIENT_CMD 1
#define UPLOAD_NAME 2
//client command type:
#define CLIENT_EXIT 2
#define CLIENT_READY 1
#define CLIENT_CANCEL 0
#define CLIENT_LOSE 3

//game status of client:
#define DISCONNECTED 0
#define WAITING_FOR_START 1
#define START 2
#define LOSE 3
#define ENDGAME 4
#define READY 5
#define WAITING_FOR_END 6


//game status of server:1,2,5



//UI standard
#define TEXT_SPACING 5
#define TABLE_PLAYER_WIDTH 400
#define TABLE_ARMY_WIDTH 120
#define TABLE_LAND_WIDTH 120
#define TABLE_HEIGHT 80
#define TABLE_FONTSIZE 30
#define TABLE_LINE_WIDTH 2


typedef struct Block {
	int type;
	int owner;
	int num;
}Block;

typedef struct Move {
	int startx;
	int starty;
	int direction;
	int endx;
	int endy;
	int launcher;
}Move;

typedef struct SetupData {
	int totalnum;
	int clientnum;
	Color playercolor[8];
	char playername[8][20];
	int mapx;
	int mapy;
	int readynum;
}SetupData;

typedef struct StatisticData {
	int land[8];
	int army[8];
}StatisticData;

typedef struct ServerInfo {
	int ver;
	int port;
	char name[30];
	char ip[22];
}ServerInfo;
#endif