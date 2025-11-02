#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <raylib.h>
#define BLACK_ALPHA 100

extern int line;
extern int column;

bool isinmap(Vector2 mouseInWorld) {
	if (mouseInWorld.x >= -65 * line && mouseInWorld.x <= 65 * line && mouseInWorld.y >= -65 * column && mouseInWorld.y <= 65 * column) return true;
	else return false;
}

int chosenColumn(float mouse_Y) {
	return ((int)mouse_Y + 65 * column) / 130 + 1;
}
int chosenLine(float mouse_X) {
	return ((int)mouse_X + 65 * line) / 130 + 1;
}

void drawhighlight(int mapx, int mapy,Color color,int islit) {
	float startx = -line * 65 + mapx * 130 - 130;
	float starty = -column * 65 + mapy * 130 -130;
	DrawRectangleLinesEx((Rectangle) { startx, starty, 130, 130 }, 8, color);
	if(islit==1){
		if (mapx > 1) DrawRectangle(startx - 130, starty, 130, 130, (Color) { 0, 0, 0, BLACK_ALPHA });
		if (mapx < line) DrawRectangle(startx + 130, starty, 130, 130, (Color) { 0, 0, 0, BLACK_ALPHA });
		if (mapy > 1) DrawRectangle(startx, starty - 130, 130, 130, (Color) { 0, 0, 0, BLACK_ALPHA });
		if (mapy < column) DrawRectangle(startx, starty + 130, 130, 130, (Color) { 0, 0, 0, BLACK_ALPHA });
	}
}