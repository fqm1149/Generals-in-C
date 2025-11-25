#include <raylib.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <generals.h>


extern int line;
extern int column;
extern Texture tmountain;
extern Texture tcrown;
extern Texture tcity;
extern Color playercolor[8];


//传入的x与y应是实际坐标减去1
void DrawL1Block(Block** mapL1, int x, int y,int player) {
	bool islit = false;
	for (int i = -1; i <= 1; i++) for (int j = -1; j <= 1; j++) {
		int tx = x + i, ty = y + j;
		if (tx >= 0 && tx < line && ty >= 0 && ty < column) if (mapL1[tx][ty].owner == player) islit = true;
	}
	if (islit) {
		switch (mapL1[x][y].type) {
		case MOUNTAIN:
			DrawRectangle(-65 * line + 130 * x, -65 * column + 130 * y, 130, 130, LIT_MOUNTAIN);
			DrawTexture(tmountain, -65 * line + 130 * x + 15, -65 * column + 130 * y + 15, WHITE);
			break;
		case CROWN:
			DrawRectangle(-65 * line + 130 * x, -65 * column + 130 * y, 130, 130, playercolor[mapL1[x][y].owner-1]);
			DrawTexture(tcrown, -65 * line + 130 * x + 15, -65 * column + 130 * y + 15, WHITE);
			break;
		case PLAIN:
			if (mapL1[x][y].owner > 0) DrawRectangle(-65 * line + 130 * x, -65 * column + 130 * y, 130, 130, playercolor[mapL1[x][y].owner - 1]);
			else DrawRectangle(-65 * line + 130 * x, -65 * column + 130 * y, 130, 130, LIT_PLAIN);
			break;
		case CITY:
			if (mapL1[x][y].owner > 0) DrawRectangle(-65 * line + 130 * x, -65 * column + 130 * y, 130, 130, playercolor[mapL1[x][y].owner - 1]);
			else DrawRectangle(-65 * line + 130 * x, -65 * column + 130 * y, 130, 130, LIT_CITY);
			DrawTexture(tcity, -65 * line + 130 * x + 15, -65 * column + 130 * y + 15, WHITE);
			break;
		default:break;
		}
		if (mapL1[x][y].num > 0) { 
			char* text = TextFormat("%d", mapL1[x][y].num);
			int textWidth = MeasureText(text, FONTSIZE_ARMY);
			int textHeight = FONTSIZE_ARMY;
			DrawText(text, -65 * line + 130 * x + 65-textWidth/4, -65 * column + 130 * y + 65-FONTSIZE_ARMY/4, 60, WHITE);
		}
		DrawRectangleLinesEx((Rectangle) { -65 * line + 130 * x, -65 * column + 130 * y, 130, 130 }, 5, BLACK);
	}//else printf("%d %d is unlit\n", x + 1, y + 1);
}

