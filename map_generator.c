#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

//we define: -2=border(not shown),-1=mountain,0=plain,positive num=tower with soilders,-3=birthplace.

int x=13,y=13;

void setupborder(int **map,int show_x,int show_y){
    for(int i=0;i<=show_x+1;i++){map[i][0]=-2;map[i][show_y+1]=-2;}
    for(int i=0;i<=show_y+1;i++){map[0][i]=-2;map[show_x+1][i]=-2;}
}

void snakesearch(int localmap[3][3],bool recordmap[3][3],int x,int y){
    if(localmap[x][y]==0){
       if(x>0){
            if(recordmap[x-1][y]==false){
                recordmap[x-1][y]=true;
                if(localmap[x-1][y]==0) snakesearch(localmap,recordmap,x-1,y);
            } 
        }
        if(x<2){
            if(recordmap[x+1][y]==false){
                recordmap[x+1][y]=true;
                if(localmap[x+1][y]==0) snakesearch(localmap,recordmap,x+1,y);
            }
        }
        if(y>0){
            if(recordmap[x][y-1]==false){
                recordmap[x][y-1]=true;
                if(localmap[x][y-1]==0) snakesearch(localmap,recordmap,x,y-1);
            }
        }
        if(y<2){
            if(recordmap[x][y+1]==false){
                recordmap[x][y+1]=true;
                if(localmap[x][y+1]==0) snakesearch(localmap,recordmap,x,y+1);
            }
        }}
}

int numofblock(int **map,int center_x,int center_y){
    int cnt=0;
    bool searched[3][3]={0};
    int tempmap[3][3];
    for(int i=0;i<3;i++) for(int j=0;j<3;j++) tempmap[i][j]=map[center_x+i-1][center_y+j-1];
    for(int i=0;i<3;i++) for(int j=0;j<3;j++)if(searched[i][j]==false){searched[i][j]=true;if(tempmap[i][j]==0) cnt++;
    snakesearch(tempmap,searched,i,j);}
    return cnt;
}

bool appendable(int **map,int target_x,int target_y){
    if(map[target_x][target_y]!=0) return false;
    int cnt1=numofblock(map,target_x,target_y);
    map[target_x][target_y]=1;
    int cnt2=numofblock(map,target_x,target_y);
    map[target_x][target_y]=0;
    if(cnt1==cnt2) return true;
    else return false;
}

void prtmap_incmd(int **map,int show_x,int show_y){
    for(int i=1;i<=show_x;i++){
        for(int j=1;j<=show_y;j++) printf("%4d",map[i][j]);
        printf("\n");
    }
}

void map_maker(int **map,int show_x,int show_y,float mountain_percentage,int player_num){
    int area=show_x*show_y;
    int cur_mnt=0;
    while((float)cur_mnt<=mountain_percentage*(float)area){
        int tmpx=rand()%show_x+1;
        int tmpy=rand()%show_y+1;
        cur_mnt+=create_mnt(map,tmpx,tmpy);
    }
}

int create_mnt(int **map,int x,int y){
    int tp=0;
    if(map[x][y]==0) if(appendable(map,x,y)) {map[x][y]=-1;tp=1;}
    if(rand()%100>35 && map[x][y]!=-2) tp+=create_mnt(map,x+rand()%3-1,y+rand()%3-1);
    return tp;
}

int testmain(){
    srand((unsigned int)time(NULL));
    int **layer_0=malloc((x+2)*sizeof(int *));
    for(int i=0;i<x+2;i++) {layer_0[i]=malloc((y+2)*sizeof(int));memset(layer_0[i],0,(y+2)*sizeof(int));}
    setupborder(layer_0,x,y);
    map_maker(layer_0,x,y,0.3,0);
    prtmap_incmd(layer_0,x,y);
    for(int i=0;i<x+2;i++) free(layer_0[i]);
    free(layer_0);
}