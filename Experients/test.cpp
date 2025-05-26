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
#include <queue>
#include <set>
#include <map>
#include <cmath>
#include <limits>

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

// --- AI Related ---
struct AStarNode {
  std::vector<int> state_vec;
  int g_score;
  int h_score;
  int f_score;

  AStarNode(const std::vector<int>& s, int g, int h) : state_vec(s), g_score(g), h_score(h) {
    f_score = g_score + h_score;
  }

  bool operator>(const AStarNode& other) const {
    if (f_score == other.f_score) {
      return h_score > other.h_score;
    }
    return f_score > other.f_score;
  }
};

std::vector<std::vector<int>> g_ai_solution_path;
int g_ai_current_step_in_path = 0;
bool g_ai_is_solving = false;


// --- Function Prototypes ---
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
void drawAIStatus();

// AI Helper Functions
std::vector<int> boardToVector(int** board, int rows, int cols);
void vectorToBoard(const std::vector<int>& vec, int** board, int rows, int cols, int empty_val);
std::vector<int> getSolvedStateVector(int rows, int cols);
void findEmptyInVector(const std::vector<int>& state_vec, int rows, int cols, int empty_val, int& empty_r, int& empty_c);
int calculateManhattanDistance(const std::vector<int>& state_vec, int rows, int cols, int empty_val);
std::vector<std::vector<int>> solvePuzzleAStar(int** initial_board, int rows, int cols);


// --- Function Definitions ---

std::vector<int> boardToVector(int** board, int rows, int cols) {
  std::vector<int> vec;
  vec.reserve(rows * cols);
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      vec.push_back(board[i][j]);
    }
  }
  return vec;
}

void vectorToBoard(const std::vector<int>& vec, int** board, int rows, int cols, int empty_val) {
  int k = 0;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      board[i][j] = vec[k];
      if (vec[k] == empty_val) {
        g_empty_tile_row = i;
        g_empty_tile_col = j;
      }
      k++;
    }
  }
}

std::vector<int> getSolvedStateVector(int rows, int cols) {
  std::vector<int> solved_vec;
  int num_tiles = rows * cols;
  solved_vec.reserve(num_tiles);
  for (int i = 0; i < num_tiles; ++i) {
    solved_vec.push_back(i);
  }
  return solved_vec;
}

void findEmptyInVector(const std::vector<int>& state_vec, int rows, int cols, int empty_val, int& empty_r, int& empty_c) {
  for (size_t i = 0; i < state_vec.size(); ++i) {
    if (state_vec[i] == empty_val) {
      empty_r = i / cols;
      empty_c = i % cols;
      return;
    }
  }
  empty_r = -1; // Should not happen
  empty_c = -1;
}

int calculateManhattanDistance(const std::vector<int>& state_vec, int rows, int cols, int empty_val) {
  int manhattan_distance = 0;
  for (size_t i = 0; i < state_vec.size(); ++i) {
    int value = state_vec[i];
    if (value != empty_val && value != (int)i) {
      int current_row = i / cols;
      int current_col = i % cols;
      int target_row = value / cols;
      int target_col = value % cols;
      manhattan_distance += std::abs(current_row - target_row) + std::abs(current_col - target_col);
    }
  }
  return manhattan_distance;
}

std::vector<std::vector<int>> solvePuzzleAStar(int** initial_board, int rows, int cols) {
  std::vector<int> initial_state_vec = boardToVector(initial_board, rows, cols);
  std::vector<int> target_state_vec = getSolvedStateVector(rows, cols);
  int empty_val = rows * cols - 1;

  std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> open_set;
  std::set<std::vector<int>> closed_set;
  std::map<std::vector<int>, int> g_score_map;
  std::map<std::vector<int>, std::vector<int>> predecessor;

  int initial_h = calculateManhattanDistance(initial_state_vec, rows, cols, empty_val);
  open_set.push(AStarNode(initial_state_vec, 0, initial_h));
  g_score_map[initial_state_vec] = 0;

  int dr[] = {-1, 1, 0, 0};
  int dc[] = {0, 0, -1, 1};
  int states_processed = 0;

  while (!open_set.empty()) {
    AStarNode current_node = open_set.top();
    open_set.pop();
    states_processed++;

    if (states_processed % 5000 == 0 && rows * cols > 9) {
      printf("A*: Processed %d states... Open set size: %zu\n", states_processed, open_set.size());
      ExMessage temp_msg;
      if (peekmessage(&temp_msg, EX_KEY) && temp_msg.vkcode == VK_ESCAPE) {
        printf("A* cancelled by user.\n");
        return {};
      }
    }

    if (current_node.state_vec == target_state_vec) {
      std::vector<std::vector<int>> path;
      std::vector<int> curr_path_state = target_state_vec;
      while (predecessor.count(curr_path_state)) {
        path.push_back(curr_path_state);
        curr_path_state = predecessor[curr_path_state];
      }
      path.push_back(initial_state_vec);
      std::reverse(path.begin(), path.end());
      printf("A*: Path found with %zu steps after processing %d states.\n", path.size() - 1, states_processed);
      return path;
    }

    if (closed_set.count(current_node.state_vec)) {
      continue;
    }
    closed_set.insert(current_node.state_vec);

    int empty_r, empty_c;
    findEmptyInVector(current_node.state_vec, rows, cols, empty_val, empty_r, empty_c);

    for (int i = 0; i < 4; ++i) {
      int next_empty_r = empty_r + dr[i];
      int next_empty_c = empty_c + dc[i];

      if (next_empty_r >= 0 && next_empty_r < rows && next_empty_c >= 0 && next_empty_c < cols) {
        std::vector<int> next_state_vec = current_node.state_vec;
        std::swap(next_state_vec[empty_r * cols + empty_c], next_state_vec[next_empty_r * cols + next_empty_c]);

        if (closed_set.count(next_state_vec)) {
          continue;
        }

        int tentative_g_score = current_node.g_score + 1;

        if (!g_score_map.count(next_state_vec) || tentative_g_score < g_score_map[next_state_vec]) {
          predecessor[next_state_vec] = current_node.state_vec;
          g_score_map[next_state_vec] = tentative_g_score;
          int h_score = calculateManhattanDistance(next_state_vec, rows, cols, empty_val);
          open_set.push(AStarNode(next_state_vec, tentative_g_score, h_score));
        }
      }
    }
  }
  printf("A*: No path found after processing %d states.\n", states_processed);
  return {};
}


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
    for (int i = 0; i < num_tiles; i++) temp_tiles[i] = i;
    for (int i = num_tiles - 1; i > 0; i--) {
      int j = rand() % (i + 1);
      std::swap(temp_tiles[i], temp_tiles[j]);
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
        if (puzzle_1d_for_inversions[i] > puzzle_1d_for_inversions[j]) inv_count++;
      }
    }

    int blank_tile_temp_idx = -1;
    for(int i = 0; i < num_tiles; ++i) if (temp_tiles[i] == empty_tile_value) { blank_tile_temp_idx = i; break; }
    assert(blank_tile_temp_idx != -1);
    int blank_tile_row_from_top = blank_tile_temp_idx / cols;

    bool is_currently_solvable;
    if (cols % 2 == 1) is_currently_solvable = (inv_count % 2 == 0);
    else {
      int blank_row_from_bottom_0_indexed = (rows - 1) - blank_tile_row_from_top;
      is_currently_solvable = ((inv_count + blank_row_from_bottom_0_indexed) % 2 == 0);
    }

    if (is_currently_solvable) solvable_configuration_generated = true;
    else if (num_tiles >= 2) {
      int idx1 = -1, idx2 = -1;
      for (int i = 0; i < num_tiles; ++i) if (temp_tiles[i] != empty_tile_value) { idx1 = i; break; }
      for (int i = idx1 + 1; i < num_tiles; ++i) if (temp_tiles[i] != empty_tile_value) { idx2 = i; break; }
      if (idx1 != -1 && idx2 != -1) { std::swap(temp_tiles[idx1], temp_tiles[idx2]); solvable_configuration_generated = true; }
    }
  }
  if (!solvable_configuration_generated) fprintf(stderr, "Warning: Failed to generate a solvable puzzle.\n");

  int k = 0;
  for (int i = 0; i < rows; i++) for (int j = 0; j < cols; j++) {
      (*board_ptr)[i][j] = temp_tiles[k];
      if (temp_tiles[k] == empty_tile_value) { g_empty_tile_row = i; g_empty_tile_col = j; }
      k++;
    }
  free(temp_tiles);

  g_ai_solution_path.clear();
  g_ai_current_step_in_path = 0;
  g_ai_is_solving = false;
  resetTimerAndCounter();
}

void drawGameBoard(int **board, int rows, int cols) {
  int block_width = PUZZLE_AREA_WIDTH / cols;
  int block_height = PUZZLE_AREA_HEIGHT / rows;
  int empty_tile_value = rows * cols - 1;
  for (int i = 0; i < rows; i++) for (int j = 0; j < cols; j++) {
      int dest_x = block_width * j; int dest_y = block_height * i;
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

void drawFullImagePreview() { putimage(PREVIEW_AREA_X_OFFSET, 0, &g_puzzle_image); }

void drawTimerAndCounter() {
  setfillcolor(BLACK);
  solidrectangle(0, PUZZLE_AREA_HEIGHT, TOTAL_WINDOW_WIDTH, WINDOW_HEIGHT);
  char time_str[100], count_str[100];
  DWORD elapsed_ms = GetTickCount() - g_level_start_time;
  DWORD remaining_ms = (LEVEL_TIME_LIMIT_MS > elapsed_ms) ? (LEVEL_TIME_LIMIT_MS - elapsed_ms) : 0;
  int minutes = remaining_ms / (60000); int seconds = (remaining_ms / 1000) % 60;
  sprintf(time_str, "Time: %02d:%02d", minutes, seconds);
  sprintf(count_str, "Moves: %d", g_move_count);
  settextcolor(WHITE); setbkmode(TRANSPARENT);
  LOGFONT f; gettextstyle(&f); f.lfHeight = 24; strcpy(f.lfFaceName, "Arial"); f.lfQuality = ANTIALIASED_QUALITY; settextstyle(&f);
  outtextxy(20, PUZZLE_AREA_HEIGHT + 10, time_str);
  outtextxy(PREVIEW_AREA_X_OFFSET + 20, PUZZLE_AREA_HEIGHT + 10, count_str);
}

void drawAIStatus() {
  if (g_ai_is_solving) {
    settextcolor(YELLOW);
    LOGFONT f_ai;
    gettextstyle(&f_ai);
    f_ai.lfHeight = 20;
    strcpy(f_ai.lfFaceName, "Arial");
    settextstyle(&f_ai);
    outtextxy(TOTAL_WINDOW_WIDTH - 220, PUZZLE_AREA_HEIGHT + 15, "AI Calculating...");
  }
}

void resetTimerAndCounter() { g_level_start_time = GetTickCount(); g_move_count = 0; }

void handleGameEvent(int **board, int rows, int cols, ExMessage msg, bool *game_running_flag, HWND hwnd) {
  if (msg.message == WM_KEYDOWN) {
    if (msg.vkcode == VK_ESCAPE) {
      EndBatchDraw();
      g_ai_is_solving = false; g_ai_solution_path.clear();
      int choice = MessageBox(hwnd, "Quit?", "Confirm Exit", MB_YESNO | MB_ICONQUESTION);
      if (choice == IDYES) *game_running_flag = false;
      BeginBatchDraw(); return;
    }

    if (msg.vkcode == 'K' || msg.vkcode == 'k') {
      if (g_ai_is_solving) {
        MessageBox(hwnd, "AI is busy. Wait or ESC.", "AI Busy", MB_OK | MB_ICONINFORMATION); return;
      }
      if (isGameOver(board, rows, cols)) {
        MessageBox(hwnd, "Solved!", "AI", MB_OK | MB_ICONINFORMATION); return;
      }

      if (g_ai_solution_path.empty() || g_ai_current_step_in_path >= g_ai_solution_path.size() -1 ) {
        g_ai_solution_path.clear(); g_ai_current_step_in_path = 0; g_ai_is_solving = true;
        if (rows * cols > 9) {
          EndBatchDraw();
          MessageBox(hwnd, "AI (A*) calculating path.\nMay take time for large puzzles.\nPress ESC in console to try cancelling.", "AI Calc", MB_OK | MB_ICONINFORMATION);
          BeginBatchDraw();
        }
        FlushBatchDraw();
        g_ai_solution_path = solvePuzzleAStar(board, rows, cols);
        g_ai_is_solving = false;

        if (g_ai_solution_path.empty()) {
          EndBatchDraw();
          MessageBox(hwnd, "AI (A*) no solution or cancelled.", "AI Error", MB_OK | MB_ICONERROR);
          BeginBatchDraw(); return;
        }
      }

      g_ai_current_step_in_path++;
      if (g_ai_current_step_in_path < g_ai_solution_path.size()) {
        vectorToBoard(g_ai_solution_path[g_ai_current_step_in_path], board, rows, cols, rows * cols - 1);
        g_move_count++;
      } else {
        g_ai_solution_path.clear(); g_ai_current_step_in_path = 0;
      }
      return;
    }

    bool player_moved = false;
    int original_empty_r = g_empty_tile_row, original_empty_c = g_empty_tile_col;
    int tile_to_move_row = g_empty_tile_row, tile_to_move_col = g_empty_tile_col;
    bool can_move = false;

    switch (msg.vkcode) {
    case VK_DOWN:    tile_to_move_row = g_empty_tile_row + 1; if (tile_to_move_row < rows) can_move = true; break;
    case VK_UP:  tile_to_move_row = g_empty_tile_row - 1; if (tile_to_move_row >= 0) can_move = true; break;
    case VK_RIGHT:  tile_to_move_col = g_empty_tile_col + 1; if (tile_to_move_col < cols) can_move = true; break;
    case VK_LEFT: tile_to_move_col = g_empty_tile_col - 1; if (tile_to_move_col >= 0) can_move = true; break;
    default: return;
    }

    if (can_move) {
      board[original_empty_r][original_empty_c] = board[tile_to_move_row][tile_to_move_col];
      board[tile_to_move_row][tile_to_move_col] = rows * cols - 1;
      g_empty_tile_row = tile_to_move_row; g_empty_tile_col = tile_to_move_col;
      g_move_count++; player_moved = true;
    }
    if (player_moved) { g_ai_solution_path.clear(); g_ai_current_step_in_path = 0; }
  }
}

bool isGameOver(int **board, int rows, int cols) {
  int expected_value = 0;
  for (int i = 0; i < rows; i++) for (int j = 0; j < cols; j++) {
      if (board[i][j] != expected_value++) return false;
    }
  return true;
}

void freeGameBoard(int **board, int current_rows) {
  if (board != NULL) {
    for (int i = 0; i < current_rows; i++) if (board[i] != NULL) { free(board[i]); board[i] = NULL; }
    free(board);
  }
}

void playBGM(const char* music_file) {
  char mci_command[256]; mciSendString("close bgm", NULL, 0, NULL);
  sprintf(mci_command, "open \"%s\" alias bgm", music_file);
  if (mciSendString(mci_command, NULL, 0, NULL) == 0) mciSendString("play bgm repeat", NULL, 0, NULL);
  else {
    char err[300]; sprintf(err, "Failed to load BGM: %s", music_file);
    MessageBox(NULL, err, "Music Error", MB_OK | MB_ICONERROR);
  }
}
void stopBGM() { mciSendString("stop bgm", NULL, 0, NULL); mciSendString("close bgm", NULL, 0, NULL); }

int main() {
  srand((unsigned int)time(NULL));
  int **game_board = NULL;
  int current_rows = DIFFICULTY_LEVELS[current_difficulty_idx];
  int current_cols = DIFFICULTY_LEVELS[current_difficulty_idx];

  HWND hwnd = initgraph(TOTAL_WINDOW_WIDTH, WINDOW_HEIGHT);
  if (hwnd == NULL) { printf("Failed to init graphics.\n"); return -1; }
  SetWindowText(hwnd, "Traditional Culture Puzzle Game - A* AI");
  playBGM("./res/music.mp3");
  loadGameResource(current_rows, current_cols);
  initGameBoard(&game_board, current_rows, current_cols);

  BeginBatchDraw();
  bool game_running = true;
  while (game_running) {
    if (GetTickCount() - g_level_start_time >= LEVEL_TIME_LIMIT_MS) {
      EndBatchDraw(); char msg[100]; sprintf(msg, "Time's up! Lost. Moves: %d", g_move_count);
      MessageBox(hwnd, msg, "Game Over", MB_OK | MB_ICONINFORMATION);
      game_running = false; if (!game_running) continue; BeginBatchDraw();
    }

    cleardevice();
    drawGameBoard(game_board, current_rows, current_cols);
    drawFullImagePreview();
    drawTimerAndCounter();
    drawAIStatus();

    if (isGameOver(game_board, current_rows, current_cols)) {
      EndBatchDraw(); current_difficulty_idx++;
      if (current_difficulty_idx >= sizeof(DIFFICULTY_LEVELS) / sizeof(DIFFICULTY_LEVELS[0])) {
        char msg[100]; sprintf(msg, "All levels clear! Moves: %d", g_move_count);
        MessageBox(hwnd, msg, "Victory!", MB_OK | MB_ICONINFORMATION); game_running = false;
      } else {
        char msg[150]; sprintf(msg, "Level Clear! Moves: %d, Time: %lus s\nNext: %dx%d",
                g_move_count, (GetTickCount() - g_level_start_time)/1000,
                DIFFICULTY_LEVELS[current_difficulty_idx], DIFFICULTY_LEVELS[current_difficulty_idx]);
        MessageBox(hwnd, msg, "Success!", MB_OK | MB_ICONINFORMATION);
        freeGameBoard(game_board, current_rows); game_board = NULL;
        current_rows = current_cols = DIFFICULTY_LEVELS[current_difficulty_idx];
        g_ai_solution_path.clear(); g_ai_current_step_in_path = 0; g_ai_is_solving = false; // Reset AI
        loadGameResource(current_rows, current_cols);
        initGameBoard(&game_board, current_rows, current_cols);
      }
      if (!game_running) continue; BeginBatchDraw();
    }

    ExMessage msg = {0};
    if (peekmessage(&msg, EX_KEY | EX_MOUSE)) { // Process all messages
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
  freeGameBoard(game_board, current_rows); game_board = NULL;
  closegraph();
  return 0;
}