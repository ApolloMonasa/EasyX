#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-deprecated-headers"//used for ignoring specific warnings
#define _CRT_SECURE_NO_WARNINGS
#ifdef UNICODE
#undef UNICODE
#endif

// C Standard Libraries
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h> // For strcpy, strcmp, memcpy
#include <time.h>
#include <math.h>   // For fabs
#include <limits.h> // For INT_MAX
#include <stdbool.h> // For bool, true, false

// Windows and EasyX
#include <graphics.h>
#include <Windows.h>
#include <mmsystem.h>
#include <conio.h> // For _getch

#pragma comment(lib, "winmm.lib")

const int DIFFICULTY_LEVELS[3] = { 3, 4, 5 };
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
DWORD LEVEL_TIME_LIMIT_MS = 8 * 60 * 1000;
int g_move_count = 0;
int BGMplaying = 1;

// --- Custom C Data Structures ---

// IntVector (replaces std::vector<int>)
typedef struct {
  int* data;
  size_t size;
  size_t capacity;
} IntVector;

IntVector iv_create(size_t initial_capacity) {
  IntVector iv;
  iv.size = 0;
  iv.capacity = (initial_capacity > 0) ? initial_capacity : 1;
  iv.data = (int*)malloc(iv.capacity * sizeof(int));
  assert(iv.data != NULL);
  return iv;
}

void iv_destroy(IntVector* iv) {
  if (iv && iv->data) {
    free(iv->data);
    iv->data = NULL;
    iv->size = 0;
    iv->capacity = 0;
  }
}

void iv_push_back(IntVector* iv, int value) {
  if (iv->size == iv->capacity) {
    iv->capacity = (iv->capacity == 0) ? 1 : iv->capacity * 2;
    int* new_data = (int*)realloc(iv->data, iv->capacity * sizeof(int));
    assert(new_data != NULL);
    iv->data = new_data;
  }
  iv->data[iv->size++] = value;
}

IntVector iv_copy(const IntVector* src) {
  IntVector dest = iv_create(src->size);
  if (src->size > 0) {
    memcpy(dest.data, src->data, src->size * sizeof(int));
  }
  dest.size = src->size;
  return dest;
}

int iv_compare(const IntVector* v1, const IntVector* v2) {
  if (v1->size != v2->size) {
    return (v1->size < v2->size) ? -1 : 1;
  }
  for (size_t i = 0; i < v1->size; ++i) {
    if (v1->data[i] != v2->data[i]) {
      return (v1->data[i] < v2->data[i]) ? -1 : 1;
    }
  }
  return 0;
}

void iv_clear(IntVector* iv) {
  iv->size = 0;
}


// AStarNode_C (replaces AStarNode struct)
typedef struct AStarNode_C {
  IntVector state_vec; // Owns its data
  int g_score;
  int h_score;
  int f_score;
} AStarNode_C;

AStarNode_C* create_astar_node_c(const IntVector* s, int g, int h) {
  AStarNode_C* node = (AStarNode_C*)malloc(sizeof(AStarNode_C));
  assert(node != NULL);
  node->state_vec = iv_copy(s);
  node->g_score = g;
  node->h_score = h;
  node->f_score = g + h;
  return node;
}

void destroy_astar_node_c(AStarNode_C* node) {
  if (node) {
    iv_destroy(&node->state_vec);
    free(node);
  }
}

int compare_astar_nodes_c(const AStarNode_C* node1, const AStarNode_C* node2) {
  if (node1->f_score != node2->f_score) {
    return node1->f_score - node2->f_score;
  }
  return node1->h_score - node2->h_score;
}

// PriorityQueue_C (min-heap for AStarNode_C*)
typedef struct {
  AStarNode_C** nodes;
  size_t size;
  size_t capacity;
} PriorityQueue_C;

PriorityQueue_C pq_c_create(size_t initial_capacity) {
  PriorityQueue_C pq;
  pq.size = 0;
  pq.capacity = (initial_capacity > 0) ? initial_capacity : 1;
  pq.nodes = (AStarNode_C**)malloc(pq.capacity * sizeof(AStarNode_C*));
  assert(pq.nodes != NULL);
  return pq;
}

void pq_c_destroy_container(PriorityQueue_C* pq) {
  if (pq && pq->nodes) {
    free(pq->nodes);
    pq->nodes = NULL;
    pq->size = 0;
    pq->capacity = 0;
  }
}

void _pq_c_heapify_up(PriorityQueue_C* pq, size_t index) {
  if (index == 0) return;
  size_t parent_index = (index - 1) / 2;
  if (compare_astar_nodes_c(pq->nodes[index], pq->nodes[parent_index]) < 0) {
    AStarNode_C* temp = pq->nodes[index];
    pq->nodes[index] = pq->nodes[parent_index];
    pq->nodes[parent_index] = temp;
    _pq_c_heapify_up(pq, parent_index);
  }
}

void _pq_c_heapify_down(PriorityQueue_C* pq, size_t index) {
  size_t left_child_idx = 2 * index + 1;
  size_t right_child_idx = 2 * index + 2;
  size_t smallest_idx = index;

  if (left_child_idx < pq->size && compare_astar_nodes_c(pq->nodes[left_child_idx], pq->nodes[smallest_idx]) < 0) {
    smallest_idx = left_child_idx;
  }
  if (right_child_idx < pq->size && compare_astar_nodes_c(pq->nodes[right_child_idx], pq->nodes[smallest_idx]) < 0) {
    smallest_idx = right_child_idx;
  }

  if (smallest_idx != index) {
    AStarNode_C* temp = pq->nodes[index];
    pq->nodes[index] = pq->nodes[smallest_idx];
    pq->nodes[smallest_idx] = temp;
    _pq_c_heapify_down(pq, smallest_idx);
  }
}

void pq_c_push(PriorityQueue_C* pq, AStarNode_C* node) {
  if (pq->size == pq->capacity) {
    pq->capacity = (pq->capacity == 0) ? 1 : pq->capacity * 2;
    AStarNode_C** new_nodes = (AStarNode_C**)realloc(pq->nodes, pq->capacity * sizeof(AStarNode_C*));
    assert(new_nodes != NULL);
    pq->nodes = new_nodes;
  }
  pq->nodes[pq->size] = node;
  pq->size++;
  _pq_c_heapify_up(pq, pq->size - 1);
}

AStarNode_C* pq_c_pop(PriorityQueue_C* pq) {
  if (pq->size == 0) return NULL;
  AStarNode_C* top_node = pq->nodes[0];
  pq->nodes[0] = pq->nodes[pq->size - 1];
  pq->size--;
  if (pq->size > 0) {
    _pq_c_heapify_down(pq, 0);
  }
  return top_node;
}

bool pq_c_is_empty(const PriorityQueue_C* pq) {
  return pq->size == 0;
}

// IntVectorArray (replaces std::vector<std::vector<int>>)
typedef struct {
  IntVector* path_steps;
  size_t count;
  size_t capacity;
} IntVectorArray;

IntVectorArray iva_create(size_t initial_capacity) {
  IntVectorArray iva;
  iva.count = 0;
  iva.capacity = (initial_capacity > 0) ? initial_capacity : 1;
  iva.path_steps = (IntVector*)malloc(iva.capacity * sizeof(IntVector));
  assert(iva.path_steps != NULL);
  return iva;
}

void iva_destroy(IntVectorArray* iva) {
  if (iva) {
    for (size_t i = 0; i < iva->count; ++i) {
      iv_destroy(&iva->path_steps[i]);
    }
    free(iva->path_steps);
    iva->path_steps = NULL;
    iva->count = 0;
    iva->capacity = 0;
  }
}

void iva_add(IntVectorArray* iva, const IntVector* iv) {
  if (iva->count == iva->capacity) {
    iva->capacity = (iva->capacity == 0) ? 1 : iva->capacity * 2;
    IntVector* new_steps = (IntVector*)realloc(iva->path_steps, iva->capacity * sizeof(IntVector));
    assert(new_steps != NULL);
    iva->path_steps = new_steps;
  }
  iva->path_steps[iva->count++] = iv_copy(iv);
}

void iva_reverse(IntVectorArray* iva) {
  for (size_t i = 0; i < iva->count / 2; ++i) {
    IntVector temp = iva->path_steps[i];
    iva->path_steps[i] = iva->path_steps[iva->count - 1 - i];
    iva->path_steps[iva->count - 1 - i] = temp;
  }
}

void iva_clear(IntVectorArray* iva) {
  for (size_t i = 0; i < iva->count; ++i) {
    iv_destroy(&iva->path_steps[i]);
  }
  iva->count = 0;
}

// --- Hash Table for A* ---
typedef struct HashEntry_AStar {
  IntVector state;
  int g_score;
  IntVector pred_state;
  bool is_in_closed_set;
  struct HashEntry_AStar* next;
} HashEntry_AStar;

typedef struct {
  HashEntry_AStar** buckets;
  size_t num_buckets;
  size_t count;
} HashTable_AStar;

unsigned long hash_int_vector(const IntVector* iv, size_t num_buckets) {
  unsigned long hash = 5381;
  for (size_t i = 0; i < iv->size; ++i) {
    hash = ((hash << 5) + hash) + iv->data[i];
  }
  return hash % num_buckets;
}

HashTable_AStar ht_astar_create(size_t num_buckets) {
  HashTable_AStar ht;
  ht.num_buckets = num_buckets > 0 ? num_buckets : 1024;
  ht.buckets = (HashEntry_AStar**)calloc(ht.num_buckets, sizeof(HashEntry_AStar*));
  assert(ht.buckets != NULL);
  ht.count = 0;
  return ht;
}

void ht_astar_destroy(HashTable_AStar* ht) {
  if (!ht || !ht->buckets) return;
  for (size_t i = 0; i < ht->num_buckets; ++i) {
    HashEntry_AStar* entry = ht->buckets[i];
    while (entry != NULL) {
      HashEntry_AStar* next_entry = entry->next;
      iv_destroy(&entry->state);
      if (entry->pred_state.data != NULL) {
        iv_destroy(&entry->pred_state);
      }
      free(entry);
      entry = next_entry;
    }
  }
  free(ht->buckets);
  ht->buckets = NULL;
  ht->num_buckets = 0;
  ht->count = 0;
}

HashEntry_AStar* ht_astar_find(HashTable_AStar* ht, const IntVector* state) {
  if (!ht || !ht->buckets) return NULL;
  unsigned long bucket_index = hash_int_vector(state, ht->num_buckets);
  HashEntry_AStar* entry = ht->buckets[bucket_index];
  while (entry != NULL) {
    if (iv_compare(&entry->state, state) == 0) {
      return entry;
    }
    entry = entry->next;
  }
  return NULL;
}

HashEntry_AStar* ht_astar_insert_or_get(HashTable_AStar* ht, const IntVector* state, bool* new_entry_created) {
  if (!ht || !ht->buckets) {
    if (new_entry_created) *new_entry_created = false;
    return NULL;
  }
  HashEntry_AStar* existing_entry = ht_astar_find(ht, state);
  if (existing_entry != NULL) {
    if (new_entry_created) *new_entry_created = false;
    return existing_entry;
  }

  unsigned long bucket_index = hash_int_vector(state, ht->num_buckets);
  HashEntry_AStar* new_node = (HashEntry_AStar*)malloc(sizeof(HashEntry_AStar));
  assert(new_node != NULL);

  new_node->state = iv_copy(state);
  new_node->g_score = INT_MAX;
  new_node->pred_state.data = NULL;
  new_node->pred_state.size = 0;
  new_node->pred_state.capacity = 0;
  new_node->is_in_closed_set = false;
  new_node->next = ht->buckets[bucket_index];
  ht->buckets[bucket_index] = new_node;
  ht->count++;

  if (new_entry_created) *new_entry_created = true;
  return new_node;
}


IntVectorArray g_ai_solution_path;
int g_ai_current_step_in_path = 0;
bool g_ai_is_solving = false;


// --- Function Prototypes (C style) ---
void loadGameResource(int rows, int cols);
void initGameBoard(int*** board_ptr, int rows, int cols);
void drawGameBoard(int** board, int rows, int cols);
void drawFullImagePreview();
void handleGameEvent(int** board, int rows, int cols, ExMessage msg, bool* game_running_flag, HWND hwnd);
bool isGameOver(int** board, int rows, int cols);
void freeGameBoard(int** board, int current_rows);
void playBGM(const char* music_file);
void stopBGM();
void drawTimerAndCounter();
void resetTimerAndCounter();
void drawAIStatus();
int timeSet();
void GoXY(int, int);
void Hide();
int Menu();
int Help();
int Size();
int Game(int);


// AI Helper Functions (C style)
IntVector boardToVector(int** board, int rows, int cols);
void vectorToBoard(const IntVector* vec, int** board, int rows, int cols, int empty_val);
IntVector getSolvedStateVector(int rows, int cols);
void findEmptyInVector(const IntVector* state_vec, int rows, int cols, int empty_val, int* empty_r, int* empty_c);
int calculateManhattanDistance(const IntVector* state_vec, int rows, int cols, int empty_val);
IntVectorArray solvePuzzleAStar(int** initial_board, int rows, int cols);
void swap_ints(int* a, int* b);


// --- Function Definitions ---

void swap_ints(int* a, int* b) {
  int temp = *a;
  *a = *b;
  *b = temp;
}

IntVector boardToVector(int** board, int rows, int cols) {
  IntVector vec = iv_create(rows * cols);
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      iv_push_back(&vec, board[i][j]);
    }
  }
  return vec;
}

void vectorToBoard(const IntVector* vec, int** board, int rows, int cols, int empty_val) {
  int k = 0;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      board[i][j] = vec->data[k];
      if (vec->data[k] == empty_val) {
        g_empty_tile_row = i;
        g_empty_tile_col = j;
      }
      k++;
    }
  }
}

IntVector getSolvedStateVector(int rows, int cols) {
  int num_tiles = rows * cols;
  IntVector solved_vec = iv_create(num_tiles);
  for (int i = 0; i < num_tiles; ++i) {
    iv_push_back(&solved_vec, i);
  }
  return solved_vec;
}

void findEmptyInVector(const IntVector* state_vec, int rows, int cols, int empty_val, int* empty_r, int* empty_c) {
  for (size_t i = 0; i < state_vec->size; ++i) {
    if (state_vec->data[i] == empty_val) {
      *empty_r = i / cols;
      *empty_c = i % cols;
      return;
    }
  }
  *empty_r = -1;
  *empty_c = -1;
}

int calculateManhattanDistance(const IntVector* state_vec, int rows, int cols, int empty_val) {
  int manhattan_distance = 0;
  for (size_t i = 0; i < state_vec->size; ++i) {
    int value = state_vec->data[i];
    if (value != empty_val && value != (int)i) {
      int current_row = i / cols;
      int current_col = i % cols;
      int target_row = value / cols;
      int target_col = value % cols;
      manhattan_distance += abs(current_row - target_row) + abs(current_col - target_col);
    }
  }
  return manhattan_distance;
}

IntVectorArray solvePuzzleAStar(int** initial_board, int rows, int cols) {
  IntVector initial_state_vec = boardToVector(initial_board, rows, cols);
  IntVector target_state_vec = getSolvedStateVector(rows, cols);
  int empty_val = rows * cols - 1;

  PriorityQueue_C open_set = pq_c_create(1000);
  HashTable_AStar state_data_table = ht_astar_create(10000);

  IntVectorArray path = iva_create(0);

  bool new_entry_flag;
  HashEntry_AStar* initial_entry = ht_astar_insert_or_get(&state_data_table, &initial_state_vec, &new_entry_flag);
  assert(initial_entry != NULL && new_entry_flag);

  initial_entry->g_score = 0;

  int initial_h = calculateManhattanDistance(&initial_state_vec, rows, cols, empty_val);
  AStarNode_C* start_node = create_astar_node_c(&initial_state_vec, 0, initial_h);
  pq_c_push(&open_set, start_node);

  int dr[] = { -1, 1, 0, 0 };
  int dc[] = { 0, 0, -1, 1 };
  int states_processed = 0;

  while (!pq_c_is_empty(&open_set)) {
    AStarNode_C* current_pq_node = pq_c_pop(&open_set);

    HashEntry_AStar* current_ht_entry = ht_astar_find(&state_data_table, &current_pq_node->state_vec);
    assert(current_ht_entry != NULL);

    if (current_ht_entry->g_score < current_pq_node->g_score) {
      destroy_astar_node_c(current_pq_node);
      continue;
    }

    states_processed++;

    if (states_processed % 5000 == 0 && rows * cols > 9) {
      printf("A*: Processed %d states... Open set size: %zu, Hash table entries: %zu\n",
             states_processed, open_set.size, state_data_table.count);
      ExMessage temp_msg;
      if (peekmessage(&temp_msg, EX_KEY) && temp_msg.vkcode == VK_ESCAPE) {
        printf("A* cancelled by user.\n");
        destroy_astar_node_c(current_pq_node);
        goto cleanup_and_return_empty;
      }
    }

    if (iv_compare(&current_ht_entry->state, &target_state_vec) == 0) {
      IntVector curr_path_state_tracer = iv_copy(&target_state_vec);
      HashEntry_AStar* path_tracer_entry = ht_astar_find(&state_data_table, &curr_path_state_tracer);

      while (path_tracer_entry != NULL && path_tracer_entry->pred_state.data != NULL) {
        iva_add(&path, &path_tracer_entry->state);
        IntVector temp_pred_for_next_trace = iv_copy(&path_tracer_entry->pred_state);
        iv_destroy(&curr_path_state_tracer);
        curr_path_state_tracer = temp_pred_for_next_trace;
        path_tracer_entry = ht_astar_find(&state_data_table, &curr_path_state_tracer);
      }
      if (iv_compare(&curr_path_state_tracer, &initial_state_vec) == 0) {
        iva_add(&path, &initial_state_vec);
      }
      iv_destroy(&curr_path_state_tracer);
      iva_reverse(&path);
      printf("A*: Path found with %zu steps after processing %d states.\n", path.count > 0 ? path.count - 1 : 0, states_processed);

      destroy_astar_node_c(current_pq_node);
      goto cleanup_and_return_path;
    }

    if (current_ht_entry->is_in_closed_set) {
      destroy_astar_node_c(current_pq_node);
      continue;
    }
    current_ht_entry->is_in_closed_set = true;

    int empty_r, empty_c;
    findEmptyInVector(&current_ht_entry->state, rows, cols, empty_val, &empty_r, &empty_c);

    for (int i = 0; i < 4; ++i) {
      int next_empty_r = empty_r + dr[i];
      int next_empty_c = empty_c + dc[i];

      if (next_empty_r >= 0 && next_empty_r < rows && next_empty_c >= 0 && next_empty_c < cols) {
        IntVector next_state_vec = iv_copy(&current_ht_entry->state);
        swap_ints(&next_state_vec.data[empty_r * cols + empty_c],
                  &next_state_vec.data[next_empty_r * cols + next_empty_c]);

        HashEntry_AStar* neighbor_ht_entry = ht_astar_insert_or_get(&state_data_table, &next_state_vec, &new_entry_flag);
        assert(neighbor_ht_entry != NULL);

        if (neighbor_ht_entry->is_in_closed_set) {
          iv_destroy(&next_state_vec);
          continue;
        }

        int tentative_g_score = current_ht_entry->g_score + 1;

        if (tentative_g_score < neighbor_ht_entry->g_score) {
          if (neighbor_ht_entry->pred_state.data != NULL) {
            iv_destroy(&neighbor_ht_entry->pred_state);
          }
          neighbor_ht_entry->pred_state = iv_copy(&current_ht_entry->state);
          neighbor_ht_entry->g_score = tentative_g_score;

          int h_score = calculateManhattanDistance(&next_state_vec, rows, cols, empty_val);
          AStarNode_C* neighbor_pq_node = create_astar_node_c(&next_state_vec, tentative_g_score, h_score);
          pq_c_push(&open_set, neighbor_pq_node);
        }
        iv_destroy(&next_state_vec);
      }
    }
    destroy_astar_node_c(current_pq_node);
  }

  printf("A*: No path found after processing %d states.\n", states_processed);

cleanup_and_return_empty:
  // Path is already empty or will be effectively empty

cleanup_and_return_path:
  while (!pq_c_is_empty(&open_set)) {
    destroy_astar_node_c(pq_c_pop(&open_set));
  }
  pq_c_destroy_container(&open_set);
  ht_astar_destroy(&state_data_table);
  iv_destroy(&initial_state_vec);
  iv_destroy(&target_state_vec);
  return path;
}


void GoXY(int x, int y) {
  HANDLE hout;
  COORD cor;
  hout = GetStdHandle(STD_OUTPUT_HANDLE);
  cor.X = x;
  cor.Y = y;
  SetConsoleCursorPosition(hout, cor);
}
void Hide() {
  HANDLE hout = GetStdHandle(STD_OUTPUT_HANDLE);
  CONSOLE_CURSOR_INFO cor_info = { 1,0 };
  SetConsoleCursorInfo(hout, &cor_info);
}
int Menu() {
  GoXY(40, 12);
  printf("===Traditional Culture Puzzle Game===");
  GoXY(43, 14);
  printf("1.Start Game");
  GoXY(43, 16);
  printf("2.Game Controls");
  GoXY(43, 18);
  printf("3.Exit Game");
  Hide();
  char ch;
  int choice = 0;
  ch = _getch();
  switch (ch) {
  case '1':choice = 1; break;
  case '2':choice = 2; break;
  case '3':choice = 3; break;
  }
  system("cls");
  return choice;
}
int Help() {
  GoXY(40, 8);
  printf("Move the pieces on the left to match the image on the right.");
  GoXY(40, 12);
  printf("Use arrow keys to move the white (empty) tile.");
  GoXY(40, 16);
  printf("Press Esc during the game to quit.");
  GoXY(40, 20);
  printf("Press K for AI assistance (one move at a time).");
  GoXY(40, 24);
  printf("Press M to mute/unmute background music.");
  GoXY(40, 28);
  printf("Press any other key to return to the main menu.");

  Hide();
  char ch = _getch();
  if (ch == 'm' || ch == 'M') {
    system("cls");
    BGMplaying = !BGMplaying;
    return 1;
  }
  system("cls");
  return 0;
}

void loadGameResource(int rows, int cols) {
  char image_path[FILENAME_MAX];
  sprintf(image_path, "./res/%d_%d.png", rows, cols);
  loadimage(&g_puzzle_image, image_path, PUZZLE_AREA_WIDTH, PUZZLE_AREA_HEIGHT);
}

void initGameBoard(int*** board_ptr, int rows, int cols) {
  *board_ptr = (int**)malloc(sizeof(int*) * rows);
  assert(*board_ptr != NULL);
  for (int i = 0; i < rows; i++) {
    (*board_ptr)[i] = (int*)malloc(sizeof(int) * cols);
    assert((*board_ptr)[i] != NULL);
  }

  int num_tiles = rows * cols;
  int* temp_tiles = (int*)malloc(sizeof(int) * num_tiles);
  assert(temp_tiles != NULL);
  int empty_tile_value = num_tiles - 1;
  bool solvable_configuration_generated = false;
  int generation_attempts = 0;
  bool is_currently_solvable = false;

  while (!solvable_configuration_generated && generation_attempts < 200) {
    generation_attempts++;
    for (int i = 0; i < num_tiles; i++) temp_tiles[i] = i;
    for (int i = num_tiles - 1; i > 0; i--) {
      int j = rand() % (i + 1);
      swap_ints(&temp_tiles[i], &temp_tiles[j]);
    }

    int inv_count = 0;
    int current_empty_pos = -1;

    IntVector temp_for_inv = iv_create(num_tiles - 1);
    for (int i = 0; i < num_tiles; ++i) {
      if (temp_tiles[i] == empty_tile_value) {
        current_empty_pos = i;
      }
      else {
        iv_push_back(&temp_for_inv, temp_tiles[i]);
      }
    }
    assert(current_empty_pos != -1);

    for (size_t i = 0; i < temp_for_inv.size; ++i) {
      for (size_t j = i + 1; j < temp_for_inv.size; ++j) {
        if (temp_for_inv.data[i] > temp_for_inv.data[j]) {
          inv_count++;
        }
      }
    }
    iv_destroy(&temp_for_inv);


    int blank_tile_row_from_top = current_empty_pos / cols;

    if (cols % 2 == 1) {
      is_currently_solvable = (inv_count % 2 == 0);
    }
    else {
      int blank_row_from_bottom_1_indexed = rows - blank_tile_row_from_top;
      is_currently_solvable = ((inv_count + blank_row_from_bottom_1_indexed) % 2 != 0);
    }

    if (is_currently_solvable) solvable_configuration_generated = true;
  }

  if (!solvable_configuration_generated) {
    fprintf(stderr, "Warning: Failed to generate a solvable puzzle configuration after %d attempts. The puzzle might be unsolvable.\n", generation_attempts);
  }


  int k = 0;
  for (int i = 0; i < rows; i++) for (int j = 0; j < cols; j++) {
      (*board_ptr)[i][j] = temp_tiles[k];
      if (temp_tiles[k] == empty_tile_value) { g_empty_tile_row = i; g_empty_tile_col = j; }
      k++;
    }
  free(temp_tiles);

  iva_clear(&g_ai_solution_path);
  g_ai_current_step_in_path = 0;
  g_ai_is_solving = false;
  resetTimerAndCounter();
}


void drawGameBoard(int** board, int rows, int cols) {
  int block_width = PUZZLE_AREA_WIDTH / cols;
  int block_height = PUZZLE_AREA_HEIGHT / rows;
  int empty_tile_value = rows * cols - 1;
  for (int i = 0; i < rows; i++) for (int j = 0; j < cols; j++) {
      int dest_x = block_width * j; int dest_y = block_height * i;
      if (board[i][j] == empty_tile_value) {
        setfillcolor(WHITE);
        solidrectangle(dest_x, dest_y, dest_x + block_width, dest_y + block_height);
      }
      else {
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
  DWORD remaining_ms = (LEVEL_TIME_LIMIT_MS > elapsed_ms || LEVEL_TIME_LIMIT_MS == 0) ?
                                                                                      (LEVEL_TIME_LIMIT_MS == 0 ? 0xFFFFFFFF : LEVEL_TIME_LIMIT_MS - elapsed_ms) : 0;

  settextcolor(WHITE); setbkmode(TRANSPARENT);
  LOGFONT f; gettextstyle(&f); f.lfHeight = 24; strcpy(f.lfFaceName, "Arial"); f.lfQuality = ANTIALIASED_QUALITY; settextstyle(&f);

  if (LEVEL_TIME_LIMIT_MS == 0) {
    sprintf(time_str, "Time: --:--");
  }
  else {
    int minutes = remaining_ms / (60000); int seconds = (remaining_ms / 1000) % 60;
    sprintf(time_str, "Time: %02d:%02d", minutes, seconds);
  }
  sprintf(count_str, "Moves: %d", g_move_count);

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
    outtextxy(TOTAL_WINDOW_WIDTH - 250, PUZZLE_AREA_HEIGHT + 15, "AI Calculating...");
  }
}

void resetTimerAndCounter() { g_level_start_time = GetTickCount(); g_move_count = 0; }

void handleGameEvent(int** board, int rows, int cols, ExMessage msg, bool* game_running_flag, HWND hwnd) {
  if (msg.message == WM_KEYDOWN) {
    if (msg.vkcode == VK_ESCAPE) {
      EndBatchDraw();
      g_ai_is_solving = false;
      iva_clear(&g_ai_solution_path);
      int choice = MessageBox(hwnd, "Quit?", "Confirm Exit", MB_YESNO | MB_ICONQUESTION);
      if (choice == IDYES) *game_running_flag = false;
      BeginBatchDraw(); return;
    }
    if (msg.vkcode == 'M' || msg.vkcode == 'm') {
      BGMplaying = !BGMplaying;
      if (BGMplaying) playBGM("./res/music.mp3");
      else stopBGM();
      return;
    }

    if (msg.vkcode == 'K' || msg.vkcode == 'k') {
      if (g_ai_is_solving) {
        MessageBox(hwnd, "AI is busy. Wait or press ESC in console to try cancelling A*.", "AI Busy", MB_OK | MB_ICONINFORMATION); return;
      }
      if (isGameOver(board, rows, cols)) {
        MessageBox(hwnd, "Puzzle is already solved!", "AI", MB_OK | MB_ICONINFORMATION); return;
      }

      if (g_ai_solution_path.count == 0 || g_ai_current_step_in_path >= g_ai_solution_path.count - 1) {
        iva_destroy(&g_ai_solution_path);
        g_ai_solution_path = iva_create(10);
        g_ai_current_step_in_path = 0;
        g_ai_is_solving = true;

        cleardevice();
        drawGameBoard(board, rows, cols);
        drawFullImagePreview();
        drawTimerAndCounter();
        drawAIStatus();
        FlushBatchDraw();

        if (rows * cols > 9) {
          EndBatchDraw();
          MessageBox(hwnd, "AI (A*) calculating path.\nMay take some time for larger puzzles.\nPress ESC in console to try cancelling.", "AI Calculation", MB_OK | MB_ICONINFORMATION);
          BeginBatchDraw();
        }

        g_ai_solution_path = solvePuzzleAStar(board, rows, cols);
        g_ai_is_solving = false;

        if (g_ai_solution_path.count == 0) {
          EndBatchDraw();
          MessageBox(hwnd, "AI (A*) could not find a solution or was cancelled.", "AI Error", MB_OK | MB_ICONERROR);
          BeginBatchDraw();
          return;
        }
      }

      g_ai_current_step_in_path++;
      if (g_ai_current_step_in_path < g_ai_solution_path.count) {
        vectorToBoard(&g_ai_solution_path.path_steps[g_ai_current_step_in_path], board, rows, cols, rows * cols - 1);
        g_move_count++;
      }
      else {
        iva_clear(&g_ai_solution_path);
        g_ai_current_step_in_path = 0;
        MessageBox(hwnd, "AI path complete or puzzle solved by AI!", "AI Path End", MB_OK | MB_ICONINFORMATION);
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
    if (player_moved) {
      iva_clear(&g_ai_solution_path);
      g_ai_current_step_in_path = 0;
    }
  }
}


bool isGameOver(int** board, int rows, int cols) {
  int expected_value = 0;
  for (int i = 0; i < rows; i++) for (int j = 0; j < cols; j++) {
      if (board[i][j] != expected_value++) return false;
    }
  return true;
}

void freeGameBoard(int** board, int current_rows) {
  if (board != NULL) {
    for (int i = 0; i < current_rows; i++) if (board[i] != NULL) { free(board[i]); board[i] = NULL; }
    free(board);
  }
}

void playBGM(const char* music_file) {
  char mci_command[256];
  mciSendString("close bgm", NULL, 0, NULL);
  sprintf(mci_command, "open \"%s\" alias bgm", music_file);
  if (mciSendString(mci_command, NULL, 0, NULL) == 0) {
    mciSendString("play bgm repeat", NULL, 0, NULL);
  }
  else {
    printf("Warning: Failed to load BGM %s\n", music_file);
  }
}
void stopBGM() {
  mciSendString("stop bgm", NULL, 0, NULL);
  mciSendString("close bgm", NULL, 0, NULL);
}

int Game(int difficulty_idx_param) {
  srand((unsigned int)time(NULL));
  int** game_board = NULL;
  current_difficulty_idx = difficulty_idx_param;
  int current_rows = DIFFICULTY_LEVELS[current_difficulty_idx];
  int current_cols = DIFFICULTY_LEVELS[current_difficulty_idx];

  g_ai_solution_path = iva_create(10);

  HWND hwnd = initgraph(TOTAL_WINDOW_WIDTH, WINDOW_HEIGHT);
  if (hwnd == NULL) { printf("Failed to init graphics.\n"); return -1; }
  SetWindowText(hwnd, "Traditional Culture Puzzle Game - A* AI (With C data structures)");

  if (BGMplaying) playBGM("./res/music.mp3");

  loadGameResource(current_rows, current_cols);
  initGameBoard(&game_board, current_rows, current_cols);

  BeginBatchDraw();
  bool game_running = true;
  while (game_running) {
    if (GetTickCount() - g_level_start_time >= LEVEL_TIME_LIMIT_MS && LEVEL_TIME_LIMIT_MS > 0) {
      EndBatchDraw(); char msg[100]; sprintf(msg, "Time's up! You Lost. Moves: %d", g_move_count);
      MessageBox(hwnd, msg, "Game Over", MB_OK | MB_ICONINFORMATION);
      game_running = false; if (!game_running) continue; BeginBatchDraw();
    }

    cleardevice();
    drawGameBoard(game_board, current_rows, current_cols);
    drawFullImagePreview();
    drawTimerAndCounter();
    drawAIStatus();

    if (isGameOver(game_board, current_rows, current_cols)) {
      EndBatchDraw();
      current_difficulty_idx++;
      if (current_difficulty_idx >= sizeof(DIFFICULTY_LEVELS) / sizeof(DIFFICULTY_LEVELS[0])) {
        char msg[100]; sprintf(msg, "All levels cleared! Total Moves: %d", g_move_count);
        MessageBox(hwnd, msg, "Victory!", MB_OK | MB_ICONINFORMATION); game_running = false;
      }
      else {
        char msg[150];
        DWORD time_taken_ms = GetTickCount() - g_level_start_time;
        sprintf(msg, "Level Clear! Moves: %d, Time: %lu.%03lus\nNext Level: %dx%d",
                g_move_count, time_taken_ms / 1000, time_taken_ms % 1000,
                DIFFICULTY_LEVELS[current_difficulty_idx], DIFFICULTY_LEVELS[current_difficulty_idx]);
        MessageBox(hwnd, msg, "Success!", MB_OK | MB_ICONINFORMATION);

        freeGameBoard(game_board, current_rows); game_board = NULL;
        current_rows = current_cols = DIFFICULTY_LEVELS[current_difficulty_idx];

        iva_clear(&g_ai_solution_path);
        g_ai_current_step_in_path = 0;
        g_ai_is_solving = false;

        loadGameResource(current_rows, current_cols);
        initGameBoard(&game_board, current_rows, current_cols);
      }
      if (!game_running) continue;
      BeginBatchDraw();
    }

    ExMessage msg = { 0 };
    if (peekmessage(&msg, EX_KEY)) {
      if (msg.message == WM_KEYDOWN) {
        handleGameEvent(game_board, current_rows, current_cols, msg, &game_running, hwnd);
      }
    }
    if (!game_running) continue;
    FlushBatchDraw();
    Sleep(16);
  }
  EndBatchDraw();
  if (BGMplaying) stopBGM();

  freeGameBoard(game_board, current_rows); game_board = NULL;
  iva_destroy(&g_ai_solution_path);

  closegraph();
  return 0;
}

int Size() {
  GoXY(40, 12);
  printf("Select Game Map Size (Enter 1 to 3):");
  GoXY(40, 14);
  printf("1. Map Size: 3x3");
  GoXY(40, 16);
  printf("2. Map Size: 4x4");
  GoXY(40, 18);
  printf("3. Map Size: 5x5");
  GoXY(40, 20);
  printf("4. Return to Main Menu");
  Hide();
  char ch;
  int result = 0;
  ch = _getch();
  switch (ch) {
  case '1':result = 1; break;
  case '2':result = 2; break;
  case '3':result = 3; break;
  case '4':result = 4; break;
  }
  system("cls");
  return result;
}

int timeSet() {
  GoXY(40, 12);
  printf("Select Difficulty (Time Limit - Enter 1 to 4):");
  GoXY(40, 14);
  printf("1. Easy (8 minutes)");
  GoXY(40, 16);
  printf("2. Medium (4 minutes)");
  GoXY(40, 18);
  printf("3. Hard (2 minutes)");
  GoXY(40, 20);
  printf("4. No Time Limit");
  GoXY(40, 22);
  printf("5. Return to Main Menu");
  Hide();
  char ch; int co = 0;
  ch = _getch();
  switch (ch) {
  case '1':LEVEL_TIME_LIMIT_MS = 8 * 60 * 1000; break;
  case '2':LEVEL_TIME_LIMIT_MS = 4 * 60 * 1000; break;
  case '3':LEVEL_TIME_LIMIT_MS = 2 * 60 * 1000; break;
  case '4':LEVEL_TIME_LIMIT_MS = 0; break;
  case '5':co = 1; break;
  }
  system("cls");
  return co;
}

int main() {
  int end = 1, choice, size_choice, help_choice_result;
  while (end) {
    choice = Menu();
    switch (choice) {
    case 1:
      size_choice = Size();
      if (size_choice == 4) { break; }
      if (timeSet() == 1) { break; }
      Game(size_choice - 1);
      system("cls");
      break;
    case 2:
      help_choice_result = Help();
      break;
    case 3:
      end = 0;
      break;
    }
  }
  return 0;
}
#pragma clang diagnostic pop