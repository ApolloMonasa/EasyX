// 用于学习基础绘图以及键盘交互
// Created by 27847 on 2025/5/25.
//
#include <conio.h> // getch()
#include <cstdio>
#include <ctime>
#include <easyx.h>

// 按键交互
struct Ball {
  int x, y, r;
  int dx, dy;
};

Ball ball = {500, 400, 15, 5, 5};

void DrawBall(Ball ball) {
  setfillcolor(RED);
  solidcircle(ball.x, ball.y, ball.r);
}

void MoveBall() {
  if(ball.x+ball.r>=800 || ball.x-ball.r<= 0) ball.dx *= -1;
  if(ball.y+ball.r>=800 || ball.y-ball.r<= 0) ball.dy *= -1;
  ball.x += ball.dx;
  ball.y -= ball.dy;
}

Ball myball = {500, 500, 15, 5, 5};
void KeyDown() {
  int userKey = _getch();
  switch (userKey) {
  case 'w':
  case 'W':
  case  72:
    myball.y -= myball.dy;
    break;
  case 's':
  case 'S':
  case  80:
    myball.y += myball.dy;
    break;
  case 'a':
  case 'A':
  case  75:
    myball.x -= myball.dx;
    break;
  case 'd':
  case 'D':
  case  77:
    myball.x += myball.dx;
    break;
  default:
    break;
  }
}

void KeyDown2() {
  if(GetAsyncKeyState(VK_UP)){
    myball.y -= myball.dy;
  }
  if(GetAsyncKeyState(VK_DOWN)){
    myball.y += myball.dy;
  }
  if(GetAsyncKeyState(VK_LEFT)){
    myball.x -= myball.dx;
  }
  if(GetAsyncKeyState(VK_RIGHT)){
    myball.x += myball.dx;
  }
}


//定时器：控制自动移动的对象
int Timer(int dura, int id){
  static int startTime[10];
  int endTime = clock();
  if(endTime - startTime[id] > dura) {
    startTime[id] = endTime;
    return 1;
  }
  return 0;
}

int main() {
  /* 基础绘图 */

//  //初始化窗口，创建， 默认黑色
//  initgraph(800, 800, EX_NOCLOSE | EX_NOMINIMIZE);
//  //两种设置窗口颜色的方法：RGB和宏
//  COLORREF color =  RGB(122, 226, 237);
//  //setbkcolor(LIGHTBLUE);
//  setbkcolor(color);
//  setlinecolor(BLACK);
//  //刷新显示
//  cleardevice();

//  //绘图
//  //设置填充颜色
//  setfillcolor(YELLOW);
//  line(0, 0, 800, 600);
//  //描线
//  circle(80, 60, 50);
//  rectangle(80, 60, 800, 600);
//  //填充
//  //带线
//  fillcircle(160, 120, 100);
//  //不带线
//  solidcircle(200, 500, 30);
//  ellipse(0, 0, 800, 600);

//  //画棋盘
//  for (int i = 0; i <= 800; i+=40) {
//      line(0, i, 800, i);
//      line(i, 0, i, 800);
//  }

  /* 贴图 */
    initgraph(800, 600);

//  //1.原样贴图
  IMAGE img;
//  loadimage(&img, "./res/beauty.png", 1000, 800);//缩放是可选的
  loadimage(&img, "./res/sun.jpg", 800, 600);
  //展示
  putimage(0, 0, &img);

  //2.透明贴图
//  IMAGE tests[2];
//  loadimage(tests, "./res/test.png", 800, 600);
//  loadimage(tests+1, "./res/testbk.png", 800, 600);
//  putimage(0, 0, tests, SRCAND);
//  putimage(0, 0, tests + 1, SRCPAINT);


  /*按键交互*/
  //1.阻塞按键交互
  //2.非阻塞按键交互
//  initgraph(800, 800);
//  BeginBatchDraw();
//  while(1) {
//    cleardevice();
//    DrawBall(ball);
//    DrawBall(myball);
//    if(Timer(20, 0)) MoveBall();
////    if(kbhit()) KeyDown();//判断存在按键再处理防止干扰
//
//    if(Timer(20, 1)) KeyDown2();
//
//    //Sleep(20);
//    FlushBatchDraw();
//  }
//  EndBatchDraw();
  printf("hello world!\n");
  getchar();
  closegraph();
  return 0;
}