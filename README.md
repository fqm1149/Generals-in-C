# Generals in C - 多人在线策略游戏

## 项目简介

这是一个用C语言实现的《Generals.io》风格的多人在线策略游戏。项目包含完整的客户端-服务器架构，支持多人实时对战、自定义地图、BOT开发等功能。

## 玩法介绍

### 已实现的核心功能

#### 基础游戏机制
1. **地图系统**
   - 随机生成的地图（平原、山脉、城市）
   - 可自定义地图尺寸（行数和列数）
   - 山脉作为不可通行障碍物
   - 城市作为可占领的中立建筑，占领后每回合生产兵力

2. **单位与占领**
   - 每个玩家有一个王城（Crown），每回合生产1兵力
   - 普通地块每25回合生产1兵力
   - 通过移动兵力占领敌方或中立地块
   - 兵力战斗机制：攻击方-1 > 防守方则占领，否则防守方减少兵力

3. **移动与战斗**
   - 点击己方地块选择，再点击相邻地块移动
   - 支持多步移动指令排队
   - 兵力移动规则：从起点移动到相邻地块
   - 占领敌方首都后，该玩家所有地块归占领方所有

#### 网络与多人游戏
1. **服务器功能**
   - 基于TCP的客户端-服务器通信
   - UDP广播自动发现服务器
   - 支持多个房间同时等待
   - 玩家断线自动处理

2. **客户端功能**
   - 可缩放/拖拽的地图视图
   - 实时显示兵力统计表
   - 支持域名/IP直连服务器
   - 游戏过程中的实时状态更新


## BOT开发指南

### BOT架构概述

BOT以动态链接库（DLL）形式存在，通过`bot_function`入口函数与服务器交互。

### 核心数据结构

```c
// 地块信息
typedef struct Block {
    int type;      // 类型：PLAIN(0), MOUNTAIN(-1), CITY(1), CROWN(-3)
    int owner;     // 所有者（0=中立，>0=玩家编号）
    int num;       // 兵力数量
} Block;

// 移动指令
typedef struct Move {
    int startx, starty;    // 起始坐标
    int direction;         // 移动方向
    int endx, endy;        // 目标坐标
    int launcher;          // 发起玩家
} Move;

// 游戏统计
typedef struct StatisticData {
    int land[8];    // 各玩家领地数
    int army[8];    // 各玩家总兵力
} StatisticData;
```

### BOT开发步骤

#### 1. 创建BOT项目
基于`bot_eg.c`模板创建新的BOT：
- 修改`bot_name`为自己的BOT名称
- 实现`bot_operate()`函数

#### 2. 核心函数：bot_operate()
这是BOT的主要逻辑函数，每回合被调用一次。你需要在这里：
- 分析当前地图状态（`mapL1`数组）
- 决策移动策略
- 发送移动指令

#### 3. 地图访问示例
```c
// 获取(x,y)位置的地块信息（坐标从0开始）
Block block = mapL1[x][y];

// 检查地块类型
if (block.type == PLAIN) {
    // 平原地块
} else if (block.type == CITY) {
    // 城市地块
} else if (block.type == CROWN) {
    // 首都地块（可能是自己或他人的）
} else if (block.type == MOUNTAIN) {
    // 山脉（不可通行）
}

// 检查地块所有者
if (block.owner == playernum) {
    // 自己的地块
} else if (block.owner == 0) {
    // 中立地块
} else {
    // 敌方地块，block.owner为敌方玩家编号
}
```

#### 4. 发送移动指令
```c
// 在bot_operate()中发送移动
void bot_operate() {
    // 你的逻辑计算最佳移动
    Move best_move;
    best_move.startx = from_x + 1;  // 注意：坐标需要+1（游戏内部使用1-based）
    best_move.starty = from_y + 1;
    best_move.endx = to_x + 1;
    best_move.endy = to_y + 1;
    // 将移动存入movelist
    // 游戏指令通过socket与服务器通信
}
```

### 5. 统计信息获取
```c
// BOT当前玩家编号（从1开始）
int my_player_num = playernum;

// 获取自己的兵力统计
int my_army = statisticData.army[playernum - 1];
int my_land = statisticData.land[playernum - 1];
```



### 注意事项
1. **坐标系统**：游戏内部使用1-based坐标，但`mapL1`数组是0-based
2. **线程安全**：BOT在单独线程运行，注意数据同步
3. **性能考虑**：每回合有800ms（默认）决策时间
4. **错误处理**：确保BOT不会崩溃，处理异常情况
5. **作弊Bot**：由于server会发送整张地图，因此bot具有透视权限，若你的bot调用了作弊内容，请自觉在bot发布时加以说明

## 计划更新
1. **游玩记录保存**
2. **Bot VS Bot测试机**

## 开源Bot
* [Campanula](https://github.com/fqm1149/Campanula/), click [here](https://github.com/fqm1149/Campanula/releases/latest) to download.
## 友情链接
Generals C++复刻版本: [Re-Generals](https://github.com/InvernoFrio/Re-Generals) By [InvernoFrio](https://github.com/InvernoFrio/)
