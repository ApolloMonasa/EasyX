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
#include <Windows.h>
#include <mmsystem.h> // For MCI

#pragma comment(lib, "winmm.lib") // Link Windows Multimedia library

// --- Global Constants and Variables ---
const int DIFFICULTY_LEVELS[3] = {3, 4, 5};
int current_difficulty_idx = 0;

int g_empty_tile_row; // Current row of the empty tile
int g_empty_tile_col; // Current column of the empty tile

#define IMAGE_WIDTH 800
#define IMAGE_HEIGHT 800
#define INFO_AREA_HEIGHT 50 // Space for timer and counter
#define WINDOW_HEIGHT (IMAGE_HEIGHT + INFO_AREA_HEIGHT)

IMAGE g_puzzle_image;

DWORD g_level_start_time;
const DWORD LEVEL_TIME_LIMIT_MS = 3 * 60 * 1000; // 3 minutes per level
int g_move_count = 0;

// --- Function Prototypes ---
void loadGameResource(int rows, int cols);
void initGameBoard(int ***board, int rows, int cols);
void drawGameBoard(int **board, int rows, int cols);
void handleGameEvent(int **board, int rows, int cols, ExMessage msg, bool *game_running_flag, HWND hwnd);
bool isGameOver(int **board, int rows, int cols);
void freeGameBoard(int **board, int current_rows);
void playBGM(const char* music_file);
void stopBGM();
void drawTimerAndCounter();
void resetTimerAndCounter();


// --- Function Definitions ---

void loadGameResource(int rows, int cols) {
  char image_path[FILENAME_MAX];
  sprintf(image_path, "./res/%d_%d.png", rows, cols);
  loadimage(&g_puzzle_image, image_path, IMAGE_WIDTH, IMAGE_HEIGHT);
}

void initGameBoard(int ***board_ptr, int rows, int cols) {
  *board_ptr = (int **)malloc(sizeof(int *) * rows);
  assert(*board_ptr != NULL);
  for (int i = 0; i < rows; i++) {
    (*board_ptr)[i] = (int *)malloc(sizeof(int) * cols);
    assert((*board_ptr)[i] != NULL);
  }

  int num_tiles = rows * cols;
  int *temp_tiles = (int *)malloc(sizeof(int) * num_tiles);
  assert(temp_tiles != NULL);

  for (int i = 0; i < num_tiles; i++) { // Initialize with ordered tiles
    temp_tiles[i] = i;
  }

  // Fisher-Yates Shuffle
  for (int i = num_tiles - 1; i > 0; i--) {
    int j = rand() % (i + 1);
    int temp_val = temp_tiles[i];
    temp_tiles[i] = temp_tiles[j];
    temp_tiles[j] = temp_val;
  }

  int empty_tile_value = num_tiles - 1; // Last tile is considered empty
  int k = 0;
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      (*board_ptr)[i][j] = temp_tiles[k];
      if (temp_tiles[k] == empty_tile_value) {
        g_empty_tile_row = i;
        g_empty_tile_col = j;
      }
      k++;
    }
  }
  free(temp_tiles);
  resetTimerAndCounter();
}

void drawGameBoard(int **board, int rows, int cols) {
  int block_width = IMAGE_WIDTH / cols;
  int block_height = IMAGE_HEIGHT / rows;
  int empty_tile_value = rows * cols - 1;

  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      int dest_x = block_width * j;
      int dest_y = block_height * i;

      if (board[i][j] == empty_tile_value) {
        setfillcolor(WHITE);
        solidrectangle(dest_x, dest_y, dest_x + block_width, dest_y + block_height);
      } else {
        int src_x = block_width * (board[i][j] % cols);
        int src_y = block_height * (board[i][j] / cols);
        putimage(dest_x, dest_y, block_width, block_height, &g_puzzle_image, src_x, src_y);
      }
    }
  }
  drawTimerAndCounter(); // Draw info below the puzzle
}

void drawTimerAndCounter() {
  setfillcolor(BLACK); // Clear info area
  solidrectangle(0, IMAGE_HEIGHT, IMAGE_WIDTH, WINDOW_HEIGHT);

  char time_str[100];
  char count_str[100];

  DWORD elapsed_ms = GetTickCount() - g_level_start_time;
  DWORD remaining_ms = (LEVEL_TIME_LIMIT_MS > elapsed_ms) ? (LEVEL_TIME_LIMIT_MS - elapsed_ms) : 0;

  int minutes = remaining_ms / (60 * 1000);
  int seconds = (remaining_ms / 1000) % 60;

  sprintf(time_str, "Time: %02d:%02d", minutes, seconds);
  sprintf(count_str, "Moves: %d", g_move_count);

  settextcolor(WHITE);
  setbkmode(TRANSPARENT);

  LOGFONT f;
  gettextstyle(&f);
  f.lfHeight = 24;
  _tcscpy(f.lfFaceName, _T("Arial"));
  f.lfQuality = ANTIALIASED_QUALITY;
  settextstyle(&f);

  outtextxy(20, IMAGE_HEIGHT + 10, time_str);
  outtextxy(IMAGE_WIDTH - 150, IMAGE_HEIGHT + 10, count_str);
}

void resetTimerAndCounter() {
  g_level_start_time = GetTickCount();
  g_move_count = 0;
}

void handleGameEvent(int **board, int rows, int cols, ExMessage msg, bool *game_running_flag, HWND hwnd) {
  if (msg.message == WM_KEYDOWN) {
    if (msg.vkcode == VK_ESCAPE) {
      EndBatchDraw(); // Pause drawing for MessageBox
      int choice = MessageBox(hwnd, "Are you sure you want to quit?", "Confirm Exit", MB_YESNO | MB_ICONQUESTION);
      if (choice == IDYES) {
        *game_running_flag = false;
      }
      BeginBatchDraw(); // Resume drawing
      return; // ESC handled
    }

    int target_row = g_empty_tile_row; // Tile to swap with empty space
    int target_col = g_empty_tile_col;
    bool can_move = false;

    // Determine which tile to move based on arrow key (empty tile "moves" into its spot)
    switch (msg.vkcode) {
    case VK_UP:    target_row = g_empty_tile_row - 1; if (target_row >= 0)   can_move = true; break;
    case VK_DOWN:  target_row = g_empty_tile_row + 1; if (target_row < rows) can_move = true; break;
    case VK_LEFT:  target_col = g_empty_tile_col - 1; if (target_col >= 0)   can_move = true; break;
    case VK_RIGHT: target_col = g_empty_tile_col + 1; if (target_col < cols) can_move = true; break;
    default: return; // Other keys ignored
    }

    if (can_move) {
      // Swap the target tile with the empty space
      board[g_empty_tile_row][g_empty_tile_col] = board[target_row][target_col];
      board[target_row][target_col] = rows * cols - 1; // Mark new empty space

      // Update empty tile's new position
      g_empty_tile_row = target_row;
      g_empty_tile_col = target_col;
      g_move_count++;
    }
  }
}

bool isGameOver(int **board, int rows, int cols) {
  int expected_value = 0;
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      if (board[i][j] != expected_value) return false;
      expected_value++;
    }
  }
  return true; // All tiles in order
}

void freeGameBoard(int **board, int current_rows) {
  if (board != NULL) {
    for (int i = 0; i < current_rows; i++) {
      free(board[i]);
    }
    free(board);
  }
}

void playBGM(const char* music_file) {
  char mci_command[256];
  mciSendString("close bgm", NULL, 0, NULL); // Close any previous instance
  sprintf(mci_command, "open \"%s\" alias bgm", music_file);
  if (mciSendString(mci_command, NULL, 0, NULL) == 0) {
    mciSendString("play bgm repeat", NULL, 0, NULL);
  } else {
    printf("Failed to load BGM: %s\n", music_file);
  }
}

void stopBGM() {
  mciSendString("stop bgm", NULL, 0, NULL);
  mciSendString("close bgm", NULL, 0, NULL);
}

// --- Main Game Logic ---
int main() {
  srand((unsigned int)time(NULL));

  int **game_board = NULL;
  int current_rows, current_cols;
  current_rows = current_cols = DIFFICULTY_LEVELS[current_difficulty_idx];

  HWND hwnd = initgraph(IMAGE_WIDTH, WINDOW_HEIGHT);

  playBGM("./res/music.mp3"); // Ensure music.mp3 is in ./res/

  loadGameResource(current_rows, current_cols);
  initGameBoard(&game_board, current_rows, current_cols);

  BeginBatchDraw();
  bool game_running = true;
  while (game_running) {
    // Check for timeout
    if (GetTickCount() - g_level_start_time >= LEVEL_TIME_LIMIT_MS) {
      EndBatchDraw();
      char lose_msg[100];
      sprintf(lose_msg, "Time's up! You lost.\nMoves: %d", g_move_count);
      MessageBox(hwnd, lose_msg, "Game Over", MB_OK | MB_ICONINFORMATION);
      game_running = false;
      if (!game_running) continue; // Prepare to exit loop
    }

    drawGameBoard(game_board, current_rows, current_cols);

    // Check for win condition
    if (isGameOver(game_board, current_rows, current_cols)) {
      current_difficulty_idx++;
      if (current_difficulty_idx >= sizeof(DIFFICULTY_LEVELS) / sizeof(DIFFICULTY_LEVELS[0])) { // All levels completed
        EndBatchDraw();
        char win_all_msg[100];
        sprintf(win_all_msg, "Congratulations! You beat all levels!\nLast level moves: %d", g_move_count);
        MessageBox(hwnd, win_all_msg, "Victory!", MB_OK);
        game_running = false;
      } else { // Advance to next level
        EndBatchDraw();
        char next_level_msg[100];
        sprintf(next_level_msg, "Level Clear! Moves: %d\nPress OK for next level.", g_move_count);
        MessageBox(hwnd, next_level_msg, "Success!", MB_OK);

        freeGameBoard(game_board, current_rows); // Clean up old board
        game_board = NULL;
        current_rows = current_cols = DIFFICULTY_LEVELS[current_difficulty_idx];

        loadGameResource(current_rows, current_cols);
        initGameBoard(&game_board, current_rows, current_cols);
        BeginBatchDraw(); // Start drawing for new level
      }
      if (!game_running) continue; // Prepare to exit loop
    }

    ExMessage msg = {0};
    if (peekmessage(&msg, EX_KEY)) { // Check for keyboard input
      handleGameEvent(game_board, current_rows, current_cols, msg, &game_running, hwnd);
    }

    if (!game_running) continue; // If ESC was pressed and confirmed

    FlushBatchDraw();
    Sleep(16); // Maintain ~60 FPS and reduce CPU load
  }
  EndBatchDraw(); // Ensure batch drawing ends if loop exited abruptly

  stopBGM();
  freeGameBoard(game_board, current_rows); // Clean up final board
  game_board = NULL;

  closegraph();
  return 0;
}