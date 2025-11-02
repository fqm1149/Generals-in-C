#ifndef GENERALS_H
#define GENERALS
#define WIN32_LEAN_AND_MEAN     
#define NOGDI                   
#define NOUSER 
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define PORT 8029

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

#define FONTSIZE_ARMY 100
#define CITY_PERCENTAGE 20

#define LINE 20
#define COLUMN 19

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
}Move;



#endif
