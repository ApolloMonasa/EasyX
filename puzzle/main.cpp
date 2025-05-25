// 这是一个传统文化拼图游戏
// Created by ApolloMonasa on 2025/5/25.
//

#ifdef UNICODE
#undef UNICODE
#endif
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <graphics.h>

const int difficulty[3] = {3, 4, 5};
int dif = 0;

int pi, pj;

//[预设]
#define IMGW 800
#define IMGH 800

IMAGE img;
IMAGE white;

//资源加载
void loadResource(int row, int col) {
  loadimage(&img, "./res/3_3.png", IMGW, IMGH);
  loadimage(&white, "./res/white.png", IMGW/col, IMGH/row);
}
//初始化地图
void initMap(int ***map, int row, int col) {
  *map = (int **)malloc(sizeof(int *) * row);
  assert(*map);
  for (int i = 0; i < row; i++) {
      (*map)[i] = (int *)malloc(sizeof(int) * col);
      assert((*map)[i]);
  }
  int length = row * col;
  int *temp = (int *)malloc(sizeof(int) * length);
  assert(temp);
  for (int i = 0; i < length; i++) {
    temp[i] = i;
  }
  for (int i = 0; i < row; i++) {
    for (int j = 0; j < col; j++) {
      if(length==0) {
        (*map)[i][j] = temp[0];
        break;
      }
      int pos = rand() % length;
      (*map)[i][j] = temp[pos];
      for (int k = pos; k < length - 1; k++) temp[k] = temp[k+1];
      length--;
    }
  }
  free(temp);
}
//绘制地图
void drawMap(int **map, int row, int col) {
  //每一小块
  int blockW = IMGW / col;
  int blockH = IMGH / row;
  int mx = row * col - 1;
  int x = 0, y = 0;
  int xx = 0, yy = 0;
  for (int i = 0; i < row; i++) {
    for (int j = 0; j < col; j++) {
      x = blockW * j;
      y = blockH * i;
      if(map[i][j] == mx) {
        putimage(x, y, &white);
        pi = i;
        pj = j;
      }
      else {
        xx = blockW * (map[i][j] % col);
        yy = blockH * (map[i][j] / col);
        putimage(x, y, blockW, blockH, &img, xx, yy);
      }
    }
  }
}
//游戏过程
void gameEvent(int **map, int row, int col, ExMessage msg) {
  int mx = row * col - 1;
  if(msg.message == WM_LBUTTONDOWN) {
    int i = pi, j = pj;
    //每一小块
    int blockW = IMGW / col;
    int blockH = IMGH / row;
    //坐标转换行列
    int msgi = msg.y / blockH;
    int msgj = msg.x / blockW;
    if(msgi >= 0 && msgi < row && msgj >= 0 && msgj < col) {
      if (i == msgi && j + 1 == msgj ||
          i == msgi && j - 1 == msgj ||
          i + 1 == msgi && j == msgj ||
          i - 1 == msgi && j == msgj ) {
        map[i][j] = map[msgi][msgj];
        map[msgi][msgj] = mx;
      }
    }

  }
}
//判定
bool gameOver(int **map, int row, int col) {
  int mx = row * col - 1;
  for (int i = 0; i < row; i++) {
    for (int j = 0; j < col; j++) {
      int cnt = i*col + j;
      if(cnt == mx) break;
      if(map[i][j] != cnt) return false;
    }
  }
  return true;
}

int main() {
  srand((unsigned int)time(nullptr));
  int ** map = nullptr;
  int row, col;
  row = col = 3;
  //创建窗口
  HWND hwnd = initgraph(IMGW, IMGH);
  //加载资源
  loadResource(row, col);
  //初始化地图
  initMap(&map, row, col);
  BeginBatchDraw();
  while(true) {
    drawMap(map, row, col);
    if(gameOver(map, row, col)) {
      dif++;
      if(dif == 3) {
        MessageBox(hwnd, "游戏结束，顺利通关！", "提示", MB_OK);
        break;
      } else {
        MessageBox(hwnd, "顺利完成，点击继续！", "提示", MB_OK);
        row = col = difficulty[dif];
        //重新加载资源
        loadResource(row, col);
        //重新初始化地图
        initMap(&map, row, col);
      }
    }
    ExMessage msg;
    if(peekmessage(&msg)) {
      gameEvent(map, row, col, msg);
    }
    FlushBatchDraw();
  }
  EndBatchDraw();

  getchar();
  // 释放地图内存
  if (map != nullptr) {
    for (int i = 0; i < row; i++) {
      free(map[i]);
    }
    free(map);
    map = nullptr;
  }
  //关闭窗口
  closegraph();
  return 0;
}