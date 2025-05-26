

#define _CRT_SECURE_NO_WARNINGS
#ifdef UNICODE
#undef UNICODE
#endif

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <graphics.h>
#include <Windows.h>
#include <mmsystem.h>
#include <vector>
#include <algorithm>

#pragma comment(lib, "winmm.lib")

const int DIFFICULTY_LEVELS[3] = {3, 4, 5};
int current_difficulty_idx = 0;

int g_empty_tile_row;
int g_empty_tile_col;

const int PUZZLE_AREA_WIDTH = 800;
const int PUZZLE_AREA_HEIGHT = 800;

const int PREVIEW_AREA_WIDTH = PUZZLE_AREA_WIDTH;
const int PREVIEW_AREA_X_OFFSET = PUZZLE_AREA_WIDTH;

const int INFO_AREA_HEIGHT = 50;

const int TOTAL_WINDOW_WIDTH = PUZZLE_AREA_WIDTH + PREVIEW_AREA_WIDTH;
const int WINDOW_HEIGHT = PUZZLE_AREA_HEIGHT + INFO_AREA_HEIGHT;

IMAGE g_puzzle_image;

DWORD g_level_start_time;
const DWORD LEVEL_TIME_LIMIT_MS = 8 * 60 * 1000;
int g_move_count = 0;

void loadGameResource(int rows, int cols);
void initGameBoard(int ***board_ptr, int rows, int cols);
void drawGameBoard(int **board, int rows, int cols);
void drawFullImagePreview();
void handleGameEvent(int **board, int rows, int cols, ExMessage msg, bool *game_running_flag, HWND hwnd);
bool isGameOver(int **board, int rows, int cols);
void freeGameBoard(int **board, int current_rows);
void playBGM(const char* music_file);
void stopBGM();
void drawTimerAndCounter();
void resetTimerAndCounter();

void loadGameResource(int rows, int cols) {
  char image_path[FILENAME_MAX];
  sprintf(image_path, "./res/%d_%d.png", rows, cols);
  loadimage(&g_puzzle_image, image_path, PUZZLE_AREA_WIDTH, PUZZLE_AREA_HEIGHT);
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

  int empty_tile_value = num_tiles - 1;

  bool solvable_configuration_generated = false;
  int generation_attempts = 0;

  while (!solvable_configuration_generated && generation_attempts < 10) {
    generation_attempts++;

    for (int i = 0; i < num_tiles; i++) {
      temp_tiles[i] = i;
    }

    for (int i = num_tiles - 1; i > 0; i--) {
      int j = rand() % (i + 1);
      int temp_val = temp_tiles[i];
      temp_tiles[i] = temp_tiles[j];
      temp_tiles[j] = temp_val;
    }

    int inv_count = 0;
    std::vector<int> puzzle_1d_for_inversions;
    for (int i = 0; i < num_tiles; i++) {
      if (temp_tiles[i] != empty_tile_value) {
        puzzle_1d_for_inversions.push_back(temp_tiles[i]);
      }
    }

    for (size_t i = 0; i < puzzle_1d_for_inversions.size(); i++) {
      for (size_t j = i + 1; j < puzzle_1d_for_inversions.size(); j++) {
        if (puzzle_1d_for_inversions[i] > puzzle_1d_for_inversions[j]) {
          inv_count++;
        }
      }
    }

    int blank_tile_temp_idx = -1;
    for(int i = 0; i < num_tiles; ++i) {
      if (temp_tiles[i] == empty_tile_value) {
        blank_tile_temp_idx = i;
        break;
      }
    }
    assert(blank_tile_temp_idx != -1);
    int blank_tile_row_from_top = blank_tile_temp_idx / cols;

    bool is_currently_solvable;
    if (cols % 2 == 1) {
      is_currently_solvable = (inv_count % 2 == 0);
    } else {
      int blank_row_from_bottom_0_indexed = (rows - 1) - blank_tile_row_from_top;
      is_currently_solvable = ((inv_count + blank_row_from_bottom_0_indexed) % 2 == 0);
    }

    if (is_currently_solvable) {
      solvable_configuration_generated = true;
    } else {
      if (num_tiles >= 2) {
        int idx1_to_swap = -1, idx2_to_swap = -1;

        for (int i = 0; i < num_tiles; ++i) {
          if (temp_tiles[i] != empty_tile_value) {
            idx1_to_swap = i;
            break;
          }
        }
        for (int i = idx1_to_swap + 1; i < num_tiles; ++i) {
          if (temp_tiles[i] != empty_tile_value) {
            idx2_to_swap = i;
            break;
          }
        }

        if (idx1_to_swap != -1 && idx2_to_swap != -1) {
          int swap_temp_val = temp_tiles[idx1_to_swap];
          temp_tiles[idx1_to_swap] = temp_tiles[idx2_to_swap];
          temp_tiles[idx2_to_swap] = swap_temp_val;
          solvable_configuration_generated = true;
        }
      }
    }
  }

  if (!solvable_configuration_generated) {
    fprintf(stderr, "Warning: Failed to generate a guaranteed solvable puzzle after %d attempts. Proceeding with last attempt.\n", generation_attempts);
  }

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
  int block_width = PUZZLE_AREA_WIDTH / cols;
  int block_height = PUZZLE_AREA_HEIGHT / rows;
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
}

void drawFullImagePreview() {
  putimage(PREVIEW_AREA_X_OFFSET, 0, &g_puzzle_image);
}

void drawTimerAndCounter() {
  setfillcolor(BLACK);
  solidrectangle(0, PUZZLE_AREA_HEIGHT, TOTAL_WINDOW_WIDTH, WINDOW_HEIGHT);

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
  strcpy(f.lfFaceName, "Arial");
  f.lfQuality = ANTIALIASED_QUALITY;
  settextstyle(&f);

  outtextxy(20, PUZZLE_AREA_HEIGHT + 10, time_str);
  outtextxy(PREVIEW_AREA_X_OFFSET + 20, PUZZLE_AREA_HEIGHT + 10, count_str);
}

void resetTimerAndCounter() {
  g_level_start_time = GetTickCount();
  g_move_count = 0;
}

void handleGameEvent(int **board, int rows, int cols, ExMessage msg, bool *game_running_flag, HWND hwnd) {
  if (msg.message == WM_KEYDOWN) {
    if (msg.vkcode == VK_ESCAPE) {
      EndBatchDraw();
      int choice = MessageBox(hwnd, "Are you sure you want to quit?", "Confirm Exit", MB_YESNO | MB_ICONQUESTION);
      if (choice == IDYES) {
        *game_running_flag = false;
      }
      BeginBatchDraw();
      return;
    }

    int tile_to_move_row = g_empty_tile_row;
    int tile_to_move_col = g_empty_tile_col;
    bool can_move = false;

    switch (msg.vkcode) {
    case VK_UP:
      tile_to_move_row = g_empty_tile_row + 1;
      if (tile_to_move_row < rows) can_move = true;
      break;
    case VK_DOWN:
      tile_to_move_row = g_empty_tile_row - 1;
      if (tile_to_move_row >= 0) can_move = true;
      break;
    case VK_LEFT:
      tile_to_move_col = g_empty_tile_col + 1;
      if (tile_to_move_col < cols) can_move = true;
      break;
    case VK_RIGHT:
      tile_to_move_col = g_empty_tile_col - 1;
      if (tile_to_move_col >= 0) can_move = true;
      break;
    default: return;
    }

    if (can_move) {
      board[g_empty_tile_row][g_empty_tile_col] = board[tile_to_move_row][tile_to_move_col];
      board[tile_to_move_row][tile_to_move_col] = rows * cols - 1;

      g_empty_tile_row = tile_to_move_row;
      g_empty_tile_col = tile_to_move_col;
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
  return true;
}

void freeGameBoard(int **board, int current_rows) {
  if (board != NULL) {
    for (int i = 0; i < current_rows; i++) {
      if (board[i] != NULL) {
        free(board[i]);
        board[i] = NULL;
      }
    }
    free(board);
  }
}

void playBGM(const char* music_file) {
  char mci_command[256];
  mciSendString("close bgm", NULL, 0, NULL);
  sprintf(mci_command, "open \"%s\" alias bgm", music_file);
  if (mciSendString(mci_command, NULL, 0, NULL) == 0) {
    mciSendString("play bgm repeat", NULL, 0, NULL);
  } else {
    char error_msg[300];
    sprintf(error_msg, "Failed to load BGM: %s\nCheck if file exists and path is correct.", music_file);
    MessageBox(NULL, error_msg, "Music Error", MB_OK | MB_ICONERROR);
  }
}

void stopBGM() {
  mciSendString("stop bgm", NULL, 0, NULL);
  mciSendString("close bgm", NULL, 0, NULL);
}

int main() {
  srand((unsigned int)time(NULL));

  int **game_board = NULL;
  int current_rows, current_cols;
  current_rows = current_cols = DIFFICULTY_LEVELS[current_difficulty_idx];

  HWND hwnd = initgraph(TOTAL_WINDOW_WIDTH, WINDOW_HEIGHT);
  if (hwnd == NULL) {
    printf("Failed to initialize graphics window.\n");
    return -1;
  }

  SetWindowText(hwnd, "Traditional Culture Puzzle Game");

  playBGM("./res/music.mp3");

  loadGameResource(current_rows, current_cols);
  initGameBoard(&game_board, current_rows, current_cols);

  BeginBatchDraw();
  bool game_running = true;
  while (game_running) {
    if (GetTickCount() - g_level_start_time >= LEVEL_TIME_LIMIT_MS) {
      EndBatchDraw();
      char lose_msg[100];
      sprintf(lose_msg, "Time's up! You lost.\nMoves: %d", g_move_count);
      MessageBox(hwnd, lose_msg, "Game Over", MB_OK | MB_ICONINFORMATION);
      game_running = false;
      if (!game_running) continue;
      BeginBatchDraw();
    }

    cleardevice();
    drawGameBoard(game_board, current_rows, current_cols);
    drawFullImagePreview();
    drawTimerAndCounter();

    if (isGameOver(game_board, current_rows, current_cols)) {
      EndBatchDraw();
      current_difficulty_idx++;
      if (current_difficulty_idx >= sizeof(DIFFICULTY_LEVELS) / sizeof(DIFFICULTY_LEVELS[0])) {
        char win_all_msg[100];
        sprintf(win_all_msg, "Congratulations! You beat all levels!\nLast level moves: %d", g_move_count);
        MessageBox(hwnd, win_all_msg, "Victory!", MB_OK | MB_ICONINFORMATION);
        game_running = false;
      } else {
        char next_level_msg[150];
        sprintf(next_level_msg, "Level Clear! Moves: %d\nTime taken: %lu seconds.\nPress OK for next level (%dx%d).",
                g_move_count, (GetTickCount() - g_level_start_time)/1000,
                DIFFICULTY_LEVELS[current_difficulty_idx], DIFFICULTY_LEVELS[current_difficulty_idx]);
        MessageBox(hwnd, next_level_msg, "Success!", MB_OK | MB_ICONINFORMATION);

        freeGameBoard(game_board, current_rows);
        game_board = NULL;
        current_rows = current_cols = DIFFICULTY_LEVELS[current_difficulty_idx];

        loadGameResource(current_rows, current_cols);
        initGameBoard(&game_board, current_rows, current_cols);
      }
      if (!game_running) continue;
      BeginBatchDraw();
    }

    ExMessage msg = {0};
    if (peekmessage(&msg, EX_KEY | EX_MOUSE)) {
      if(msg.message == WM_KEYDOWN || msg.message == WM_KEYUP || msg.message == WM_CHAR) {
        handleGameEvent(game_board, current_rows, current_cols, msg, &game_running, hwnd);
      }
    }

    if (!game_running) continue;

    FlushBatchDraw();
    Sleep(16);
  }
  EndBatchDraw();

  stopBGM();
  freeGameBoard(game_board, current_rows);
  game_board = NULL;

  closegraph();
  return 0;
}