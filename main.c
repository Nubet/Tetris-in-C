#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "primlib.h"
#include "pieces.h"

#define GAME_GRID_WIDTH 15
#define GAME_GRID_HEIGHT 25
#define PIECE_SIZE 4
#define SQUARE_SIZE 25
#define FALL_DELAY 500
#define INPUT_DELAY 50

#define DEFAULT_COLOR   BLUE
#define FROZEN_COLOR    RED
#define PIVOT_COLOR     YELLOW

int game_grid[GAME_GRID_HEIGHT][GAME_GRID_WIDTH];

typedef struct {
    int type;
    int rotation;
    int x; 
    int y;
} Piece;

//Piece next_piece;

typedef struct {
    int score;
    int lines_cleared;
    bool win;
    bool quit;
    bool game_over;
} GameState;

GameState game_state = {0, 0, false, false, false};

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    int dx;
    int dy;
} Vector;

typedef struct {
    int row;
    int col;
} Cell;

Point grid_top_left;

/* DRAWING FUNCTIONS */
void draw_square(Point square_pos, int color);
void render_square(Piece piece, Cell cell, Point grid_top_left, bool is_falling_piece);
void draw_piece(Piece piece, Point grid_top_left, bool is_falling_piece);
void draw_next_piece(Piece current, Piece next, Point grid_top_left);
void render_score(Point grid_top_left);
void draw_borders(Point grid_top_left);
void render_frozen_cell(Cell cell, Point grid_top_left);
void render_frozen_cells(Point grid_top_left);

/* GAME GRID MANAGEMENT  */
void init_game_grid();
void shift_game_grid_down_from(int start_row);
void clear_lines();

/* Game Logic */
bool check_game_over(Piece piece);
void show_game_over_message(const char *message);
void init_game(Piece *current, Piece *next);
void game_loop(Piece *current, Piece *next);

/* PIECE OPERATIONS  */
bool is_collision_at(Piece piece, Cell cell);
bool is_valid_position(Piece piece);
Cell find_rotation_center(Piece piece);
void rotate_piece_around_pivot(Piece *piece);
bool move_piece(Piece *piece, Vector vec);
void freeze_piece(Piece piece);
void spawn_new_piece(Piece *piece, bool is_preview);
void clear_input();
void handle_piece_landing(Piece *current, Piece *next);
void handle_input(Piece *current);
void render_game(Piece current, Piece next, Point grid_top_left);

/* DRAWING  */
void draw_square(Point square_pos, int color) {
    int bottom_right_x = square_pos.x + SQUARE_SIZE - 1;
    int bottom_right_y = square_pos.y + SQUARE_SIZE - 1;
    gfx_filledRect(square_pos.x, square_pos.y, bottom_right_x, bottom_right_y, color);
}

void render_square(Piece piece, Cell cell, Point grid_top_left, bool is_falling_piece) {
    int cellValue = pieces[piece.type][piece.rotation][cell.row][cell.col];
    if (cellValue == 0) return;

    int grid_x = is_falling_piece ? (piece.x + cell.col) : cell.col;
    int grid_y = is_falling_piece ? (piece.y + cell.row) : cell.row;
    int screen_x = grid_top_left.x + grid_x * SQUARE_SIZE;
    int screen_y = grid_top_left.y + grid_y * SQUARE_SIZE;
    int color = (cellValue == 2) ? PIVOT_COLOR : DEFAULT_COLOR;
    Point square_pos = {screen_x, screen_y};
    draw_square(square_pos, color);
}

void draw_piece(Piece piece, Point grid_top_left, bool is_falling_piece) {
    for (int row = 0; row < PIECE_SIZE; row++) {
        for (int col = 0; col < PIECE_SIZE; col++) {
            Cell cell = { row, col };
            render_square(piece, cell, grid_top_left, is_falling_piece);
        }
    }
}

bool is_cell_frozen(Cell cell) {
    return game_grid[cell.row][cell.col] != 0;
}

bool piece_has_block_at(Piece piece, Cell cell) {
    return pieces[piece.type][piece.rotation][cell.row][cell.col] != 0;
}

void render_frozen_cell(Cell cell, Point grid_top_left) {
    if (is_cell_frozen(cell)) {
        int screen_x = grid_top_left.x + cell.col * SQUARE_SIZE;
        int screen_y = grid_top_left.y + cell.row * SQUARE_SIZE;
        Point square_pos = {screen_x, screen_y};
        draw_square(square_pos, game_grid[cell.row][cell.col]);
    }
}

void render_frozen_cells(Point grid_top_left) {
    for (int row = 0; row < GAME_GRID_HEIGHT; row++) {
        for (int col = 0; col < GAME_GRID_WIDTH; col++) {
            Cell cell = { row, col };
            render_frozen_cell(cell, grid_top_left);
        }
    }
}

void draw_next_piece(Piece current, Piece next, Point grid_top_left) {
    int preview_x = grid_top_left.x + GAME_GRID_WIDTH * SQUARE_SIZE + 20;
    int preview_y = grid_top_left.y - 20;
    gfx_textout(preview_x, preview_y, "Next:", WHITE);
    Point preview_grid_top_left = { preview_x, preview_y + SQUARE_SIZE };
    draw_piece(next, preview_grid_top_left, false);
}

void render_score(Point grid_top_left) {
    char score_text[50];
    sprintf(score_text, "Score: %d", game_state.score);
    int score_x = grid_top_left.x + GAME_GRID_WIDTH * SQUARE_SIZE + 20;
    int score_y = grid_top_left.y + 100;
    gfx_textout(score_x, score_y, score_text, WHITE);
}

void draw_borders(Point grid_top_left) {
    int grid_start_x = grid_top_left.x;
    int grid_start_y = grid_top_left.y;
    int grid_end_x = grid_top_left.x + GAME_GRID_WIDTH * SQUARE_SIZE - 1;
    int grid_end_y = grid_top_left.y + GAME_GRID_HEIGHT * SQUARE_SIZE - 1;
    gfx_line(grid_start_x, grid_start_y, grid_start_x, grid_end_y, YELLOW);
    gfx_line(grid_end_x, grid_start_y, grid_end_x, grid_end_y, YELLOW);
}

/* Game Grid Management */
void init_game_grid() {
    for (int row = 0; row < GAME_GRID_HEIGHT; row++)
        for (int col = 0; col < GAME_GRID_WIDTH; col++)
            game_grid[row][col] = 0;
}

void shift_game_grid_down_from(int start_row) {
    for (int row = start_row; row > 0; row--)
        for (int col = 0; col < GAME_GRID_WIDTH; col++)
            game_grid[row][col] = game_grid[row - 1][col];
    for (int col = 0; col < GAME_GRID_WIDTH; col++)
        game_grid[0][col] = 0;
}

void clear_lines() {
    for (int row = GAME_GRID_HEIGHT - 1; row >= 0; row--) {
        bool full = true;
        for (int col = 0; col < GAME_GRID_WIDTH; col++) {
            if (game_grid[row][col] == 0) {
                full = false;
                break;
            }
        }
        if (full) {
            shift_game_grid_down_from(row);
            row++;
            game_state.lines_cleared += 1;
            game_state.score += 100;
        }
    }
}

/* Piece Operations */
bool is_collision_at(Piece piece, Cell cell) {
    int grid_column = piece.x + cell.col;
    int grid_row = piece.y + cell.row;

    if (grid_column < 0 || grid_column >= GAME_GRID_WIDTH || grid_row >= GAME_GRID_HEIGHT)
        return true;

    if (grid_row >= 0 && game_grid[grid_row][grid_column] != 0)
        return true;

    return false;
}


bool is_valid_position(Piece piece) {
    for (int row = 0; row < PIECE_SIZE; row++) {
        for (int col = 0; col < PIECE_SIZE; col++) {
            Cell cell = { row, col };
            if (piece_has_block_at(piece, cell)) {
                if (is_collision_at(piece, cell))
                    return false;
            }
        }
    }
    return true;
}

Cell find_rotation_center(Piece piece) {
    for (int row = 0; row < PIECE_SIZE; row++) {
        for (int col = 0; col < PIECE_SIZE; col++) {
            if (pieces[piece.type][piece.rotation][row][col] == 2) {
                return (Cell){row, col};
            }
        }
    }
    return (Cell){0, 0};
}

void rotate_piece_around_pivot(Piece *piece) {
    Piece rotated_piece = *piece;
    rotated_piece.rotation = (rotated_piece.rotation + 1) % 4;

    Cell current_pivot = find_rotation_center(*piece);
    Cell new_pivot = find_rotation_center(rotated_piece);

    int horizontal_shift = current_pivot.col - new_pivot.col;
    int vertical_shift = current_pivot.row - new_pivot.row;

    rotated_piece.x += horizontal_shift;
    rotated_piece.y += vertical_shift;

    if (is_valid_position(rotated_piece)) {
        *piece = rotated_piece;
    }
}

bool move_piece(Piece *piece, Vector vec) {
    Piece moved = *piece;
    moved.x += vec.dx;
    moved.y += vec.dy;
    if (is_valid_position(moved)) {
        *piece = moved;
        return true;
    }
    return false;
}

void freeze_piece(Piece piece) {
    for (int row = 0; row < PIECE_SIZE; row++) {
        for (int col = 0; col < PIECE_SIZE; col++) {
            Cell cell = { row, col };
            if (piece_has_block_at(piece, cell)) {
                int grid_column = piece.x + cell.col;
                int grid_row = piece.y + cell.row;
                if (grid_row >= 0 && grid_row < GAME_GRID_HEIGHT && grid_column >= 0 && grid_column < GAME_GRID_WIDTH)
                    game_grid[grid_row][grid_column] = FROZEN_COLOR;
            }
        }
    }
}

/* Game Loop */
void spawn_new_piece(Piece *piece, bool is_preview) {
    *piece = (Piece){
        .type = rand() % 7,
        .rotation = 0,
        .x = is_preview ? 0 : (GAME_GRID_WIDTH - PIECE_SIZE) / 2,
        .y = is_preview ? 0 : -2
    };
}

bool check_game_over(Piece piece) {
    return !is_valid_position(piece);
}

void show_game_over_message(const char *message) {
    gfx_filledRect(0, 0, gfx_screenWidth() - 1, gfx_screenHeight() - 1, BLACK);
    gfx_textout((gfx_screenWidth() - 150) / 2, gfx_screenHeight() / 2, message, RED);
    gfx_textout((gfx_screenWidth() - 180) / 2, gfx_screenHeight() / 2 + 20, "Press ENTER to exit", WHITE);
    gfx_updateScreen();
    while (1) {
        int key = gfx_pollkey();
        if (key == SDLK_RETURN || key == SDLK_ESCAPE)
            break;
        SDL_Delay(50);
    }
}

void clear_input() {
    while (gfx_pollkey() != -1) {}
}

void handle_piece_landing(Piece *current, Piece *next) {
    freeze_piece(*current);
    clear_lines();
    clear_input();
    *current = *next;
    spawn_new_piece(next, true);
    game_state.game_over = check_game_over(*current);
}

void handle_input(Piece *current) {
    int key = gfx_pollkey();
    Vector move_left = {-1,0};
    Vector move_right =  {1,0};
    Vector move_down =  {0,1};
    		
    switch (key) {
        case SDLK_LEFT:  move_piece(current, move_left); break;
        case SDLK_RIGHT: move_piece(current, move_right); break;
        case SDLK_DOWN:  move_piece(current, move_down); break;
        case ' ':        rotate_piece_around_pivot(current); break;
        case SDLK_ESCAPE:
        case SDLK_RETURN: game_state.quit = true; break;
    }
}

void render_game(Piece current, Piece next, Point grid_top_left) {
    gfx_filledRect(0, 0, gfx_screenWidth() - 1, gfx_screenHeight() - 1, BLACK);

    render_frozen_cells(grid_top_left);
    draw_piece(current, grid_top_left, true);
    draw_borders(grid_top_left);
    draw_next_piece(current, next, grid_top_left);
    render_score(grid_top_left);

    gfx_updateScreen();
}

void init_game(Piece *current, Piece *next) {
    if (gfx_init()) exit(3);
    srand(time(NULL));
    init_game_grid();

    grid_top_left.x = (gfx_screenWidth() - GAME_GRID_WIDTH * SQUARE_SIZE) / 2;
    grid_top_left.y = (gfx_screenHeight() - GAME_GRID_HEIGHT * SQUARE_SIZE) / 2;

    spawn_new_piece(current, false);
    spawn_new_piece(next, true);
}

void game_loop(Piece *current, Piece *next) {
    unsigned int last_fall_time = SDL_GetTicks();
    game_state.game_over = false;
	
	Vector move_down = {0,1};
	
    while (!game_state.quit && !game_state.game_over) {
        handle_input(current);

        if (SDL_GetTicks() - last_fall_time >= FALL_DELAY) {
            if (!move_piece(current, move_down)) {
                handle_piece_landing(current, next);
                if (game_state.game_over) {
                    show_game_over_message("Game Over!");
                    game_state.quit = true;
                }
            }
            last_fall_time = SDL_GetTicks();
        }

        render_game(*current, *next, grid_top_left);
        SDL_Delay(INPUT_DELAY);
    }
}

int main() {
    Piece current_piece, next_piece;
    init_game(&current_piece, &next_piece);
    game_loop(&current_piece, &next_piece);
    return 0;
}
