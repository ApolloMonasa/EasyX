// 用于学习鼠标交互
// Created by ApolloMonasa on 2025/5/25.
//
#include <graphics.h>

int main() {
  initgraph(800, 800);
  //实现左键画⚪ 右键画方
  ExMessage msg;
  while(1) {
    while(peekmessage(&msg)) {
      switch (msg.message) {
      case WM_LBUTTONDOWN :
        circle(msg.x, msg.y, 10);
        break;
      case WM_RBUTTONDOWN :
        rectangle(msg.x-10, msg.y-10, msg.x+10, msg.y+10);
        break;
      }
    }
  }
  closegraph();
  return 0;
}