#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <tuple>
#include <chrono>
#include <limits>
#include <numeric> // for std::iota
#include <queue>   // for BFS
#include <set>     // for visited sets in BFS
#include <math.h>

// --- Constants ---
const char EMPTY_CELL = '.';
const char OBSTACLE_CELL = '#';
const char SPEED_BOOST_ITEM = 'G';
const char PAINT_BOMB_ITEM = 'E';
const char OIL_SLICK_ITEM = 'F';
const int MAX_PLAYERS_POSSIBLE = 4; // A, B, C, D

// --- Helper Structs ---
struct Direction
{
    int dr, dc;
};
const std::vector<Direction> DIRECTIONS = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {0, 0}}; // UP, DOWN, LEFT, RIGHT, STAY

struct Player
{
    int id; // 0 for our bot, 1+ for others based on input order
    int x, y;
    char color_char;
    bool eliminated;

    // Item states for simulation
    int speed_boost_turns_left_sim;
    int oil_slick_turns_to_expire_sim;  // How many turns until it vanishes if not used
    bool has_oil_slick_sim;             // True if player possesses an oil slick item
    bool paint_bomb_just_triggered_sim; // Flag if this player just used a paint bomb

    Player(int _id = -1, int _x = -1, int _y = -1, char _color = ' ') : id(_id), x(_x), y(_y), color_char(_color),
                                                                        eliminated(_x == -1 && _y == -1),
                                                                        speed_boost_turns_left_sim(0), oil_slick_turns_to_expire_sim(0),
                                                                        has_oil_slick_sim(false), paint_bomb_just_triggered_sim(false) {}
};

struct ItemOnMap
{
    int r, c;
    char type;
    int turns_to_despawn; // For items that disappear if not picked up

    ItemOnMap(int _r = 0, int _c = 0, char _t = ' ', int _despawn = 10) : r(_r), c(_c), type(_t), turns_to_despawn(_despawn) {}
};

struct MoveAction
{
    int player_id;
    int next_x, next_y;
    bool oil_used;
    int steps; // 0 for stay, 1 for 1-step, 2 for 2-step

    MoveAction(int pid = -1, int nx = -1, int ny = -1, bool ou = false, int s = 0) : player_id(pid), next_x(nx), next_y(ny), oil_used(ou), steps(s) {}

    bool is_valid() const { return player_id != -1; } // Simple validity check
};

// Forward declarations
class GameState;
std::vector<MoveAction> generate_possible_moves_for_player(const GameState &gs, int player_id_to_move);
std::vector<double> static_evaluate_state_all_players(const GameState &gs);
void apply_paint_bomb_effect(GameState &gs, int center_r, int center_c, char player_color);

class GameState
{
public:
    int M, N, K_shrink_period, game_turn_number; // Actual game turn
    std::vector<Player> players;
    std::vector<std::vector<char>> grid;
    std::vector<ItemOnMap> items_on_map; // Items on map in current state

    // Actual item states for our bot (players[0]) persisted via STATE.DAT
    int my_actual_speed_boost_turns_left;
    int my_actual_oil_slick_turns_to_expire;
    bool my_actual_has_oil_slick;
    // paint_bomb_just_picked_up is not needed here as its effect is immediate

    GameState() : M(0), N(0), K_shrink_period(0), game_turn_number(0),
                  my_actual_speed_boost_turns_left(0),
                  my_actual_oil_slick_turns_to_expire(0),
                  my_actual_has_oil_slick(false) {}

    // Deep copy constructor for simulations
    GameState(const GameState &other) = default; // std::vector and primitives copy fine

    char get_my_actual_color_char() const
    {
        if (!players.empty() && players[0].id == 0)
            return players[0].color_char;
        return ' ';
    }

    void parse_input(const std::string &filename = "MAP.INP")
    {
        std::ifstream ifs(filename);
        if (!ifs.is_open())
        {
            std::cerr << "Error: Cannot open MAP.INP" << std::endl;
            exit(1);
        }

        ifs >> M >> N >> K_shrink_period >> game_turn_number;

        players.clear();
        Player my_p_data(0); // ID 0 for my player
        ifs >> my_p_data.x >> my_p_data.y >> my_p_data.color_char;
        my_p_data.eliminated = (my_p_data.x == -1 && my_p_data.y == -1);
        players.push_back(my_p_data);

        int num_other_players_input;
        ifs >> num_other_players_input;
        for (int i = 0; i < num_other_players_input; ++i)
        {
            Player op_data(i + 1); // IDs 1, 2, ...
            ifs >> op_data.x >> op_data.y >> op_data.color_char;
            op_data.eliminated = (op_data.x == -1 && op_data.y == -1);
            players.push_back(op_data);
        }

        grid.assign(M, std::vector<char>(N));
        for (int r_idx = 0; r_idx < M; ++r_idx)
        {
            for (int c_idx = 0; c_idx < N; ++c_idx)
            {
                ifs >> grid[r_idx][c_idx];
            }
        }

        int num_map_items_input;
        ifs >> num_map_items_input;
        items_on_map.clear();
        for (int i = 0; i < num_map_items_input; ++i)
        {
            ItemOnMap item;
            ifs >> item.r >> item.c >> item.type;
            // Problem: "Các vật phẩm sẽ biến mất sau 10 lượt nếu không có người chơi nào nhặt được."
            // This implies items need a spawn turn or remaining lifetime. For simplicity, assume items given
            // in MAP.INP are valid for this turn and will despawn if their internal timer runs out.
            // We'll add a default turns_to_despawn when reading.
            item.turns_to_despawn = 10; // Default lifetime from spawn
            items_on_map.push_back(item);
        }
        ifs.close();
        load_my_actual_item_state();
        sync_my_actual_items_to_player0_sim();
    }

    void save_my_actual_item_state(const std::string &filename = "STATE.DAT")
    {
        std::ofstream ofs(filename);
        if (!ofs.is_open())
        {
            std::cerr << "Warning: Cannot write STATE.DAT" << std::endl;
            return;
        }
        ofs << my_actual_speed_boost_turns_left << std::endl;
        ofs << my_actual_oil_slick_turns_to_expire << std::endl;
        ofs << (my_actual_has_oil_slick ? 1 : 0) << std::endl;
        ofs.close();
    }

    void load_my_actual_item_state(const std::string &filename = "STATE.DAT")
    {
        std::ifstream ifs(filename);
        if (!ifs.is_open())
        {
            my_actual_speed_boost_turns_left = 0;
            my_actual_oil_slick_turns_to_expire = 0;
            my_actual_has_oil_slick = false;
            return;
        }
        int has_oil_int = 0;
        ifs >> my_actual_speed_boost_turns_left >> my_actual_oil_slick_turns_to_expire >> has_oil_int;
        my_actual_has_oil_slick = (has_oil_int == 1);
        ifs.close();
    }

    void sync_my_actual_items_to_player0_sim()
    {
        if (!players.empty() && players[0].id == 0)
        {
            players[0].speed_boost_turns_left_sim = my_actual_speed_boost_turns_left;
            players[0].oil_slick_turns_to_expire_sim = my_actual_oil_slick_turns_to_expire;
            players[0].has_oil_slick_sim = my_actual_has_oil_slick;
            players[0].paint_bomb_just_triggered_sim = false; // Reset for sim
        }
    }

    bool is_within_bounds(int r, int c) const { return r >= 0 && r < M && c >= 0 && c < N; }

    // Checks if cell (r,c) will be sealed at END of `turn_being_concluded`
    bool cell_will_be_sealed_at_end_of_turn(int r, int c, int turn_being_concluded) const
    {
        if (turn_being_concluded <= 0 || K_shrink_period <= 0 || turn_being_concluded % K_shrink_period != 0)
        {
            return false;
        }
        int shrink_iteration = turn_being_concluded / K_shrink_period; // This is 'i' in problem "i * K"
        int sealed_layer_index = shrink_iteration - 1;                 // Layers are i-1 (0-indexed)

        if (sealed_layer_index < 0)
            return false; // Should not happen if turn_being_concluded is positive multiple of K

        return (r == sealed_layer_index || r == M - 1 - sealed_layer_index ||
                c == sealed_layer_index || c == N - 1 - sealed_layer_index);
    }

    // Check if a cell is valid to move TO (not land on obstacle, not sealed, not becoming sealed THIS turn)
    bool is_target_cell_valid_for_landing(int r, int c, int current_sim_turn_num) const
    {
        if (!is_within_bounds(r, c))
            return false;
        char cell_content = grid[r][c];
        if (cell_content == OBSTACLE_CELL || (cell_content >= 'a' && cell_content <= 'd'))
        { // Is obstacle or already sealed
            return false;
        }
        // If landing here will result in elimination by THIS turn's shrink
        if (cell_will_be_sealed_at_end_of_turn(r, c, current_sim_turn_num))
        {
            return false;
        }
        return true;
    }

    // Applies a single player's chosen move, updates their position,
    // handles item pickup (including immediate paint bomb effect), and oil slick usage.
    // Modifies the GameState directly.
    void apply_chosen_move_for_player_in_sim(const MoveAction &move)
    {
        if (!move.is_valid() || move.player_id < 0 || move.player_id >= players.size() || players[move.player_id].eliminated)
        {
            return; // Invalid move or player
        }
        Player &p = players[move.player_id];

        p.x = move.next_x;
        p.y = move.next_y;
        p.paint_bomb_just_triggered_sim = false; // Reset

        // Item Pickup
        bool can_pickup_new_item = !(p.speed_boost_turns_left_sim > 0 || p.has_oil_slick_sim);
        if (can_pickup_new_item)
        {
            auto it = items_on_map.begin();
            while (it != items_on_map.end())
            {
                if (it->r == p.x && it->c == p.y)
                {
                    if (it->type == SPEED_BOOST_ITEM)
                    {
                        p.speed_boost_turns_left_sim = 5;
                    }
                    else if (it->type == PAINT_BOMB_ITEM)
                    {
                        apply_paint_bomb_effect(*this, p.x, p.y, p.color_char);
                        p.paint_bomb_just_triggered_sim = true; // For eval if needed
                    }
                    else if (it->type == OIL_SLICK_ITEM)
                    {
                        p.has_oil_slick_sim = true;
                        p.oil_slick_turns_to_expire_sim = 5;
                    }
                    it = items_on_map.erase(it); // Item is picked up and removed
                    break;
                }
                else
                {
                    ++it;
                }
            }
        }

        // Oil Slick Usage for the move itself
        if (move.oil_used)
        {
            if (p.has_oil_slick_sim)
            { // Ensure they actually had one to use
                p.has_oil_slick_sim = false;
                p.oil_slick_turns_to_expire_sim = 0;
            }
            else
            {
                // This would be an illegal move if oil_used=true but player didn't have oil.
                // generate_possible_moves should prevent this.
            }
        }
    }
};

// --- Global Search Variables ---
std::chrono::steady_clock::time_point search_start_time_global;
double time_limit_seconds_global = 1.8; // Leave a small buffer

bool is_time_up_global()
{
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - search_start_time_global;
    return elapsed.count() >= time_limit_seconds_global;
}

// --- Function Stubs (to be fully implemented) ---
void color_tiles_from_player_positions(GameState &gs)
{
    std::map<std::pair<int, int>, int> players_on_cell_count;
    for (const auto &p : gs.players)
    {
        if (!p.eliminated && gs.is_within_bounds(p.x, p.y))
        {
            players_on_cell_count[{p.x, p.y}]++;
        }
    }

    for (const auto &p : gs.players)
    {
        if (!p.eliminated && gs.is_within_bounds(p.x, p.y))
        {
            if (players_on_cell_count[{p.x, p.y}] == 1)
            { // Only one player on the cell
                char current_cell_char = gs.grid[p.x][p.y];
                // Can only color if not obstacle and not already sealed (lowercase)
                if (current_cell_char != OBSTACLE_CELL && !(current_cell_char >= 'a' && current_cell_char <= 'd'))
                {
                    gs.grid[p.x][p.y] = p.color_char;
                }
            }
        }
    }
}

// Enclosure Detection and Filling (Complex Part)
// Helper for BFS in enclosure detection
struct BFS_Node
{
    int r, c;
};

void detect_and_fill_enclosures_and_eliminate_players(GameState &gs, int current_sim_turn_num)
{
    // For each player color 'P_COLOR' present on the map:
    // 1. Identify all cells of P_COLOR.
    // 2. For each connected component of P_COLOR cells:
    //    a. Perform a BFS/DFS starting from all cells in this component simultaneously.
    //       This BFS explores EMPTY_CELLs and cells of OTHER_PLAYER_COLORs.
    //    b. If the BFS can reach the *current game border* (considering map shrink),
    //       then this component is NOT enclosed. The current border is defined by the
    //       outermost non-sealed cells.
    //       Effective border cells: min_r, max_r, min_c, max_c for unsealed area.
    //       If K_shrink_period > 0 and current_sim_turn_num > 0:
    //          int current_shrink_depth = (current_sim_turn_num -1) / gs.K_shrink_period; // layers already sealed
    //       else current_shrink_depth = -1; (no shrink yet or before first K)
    //       min_r_border = current_shrink_depth + 1; max_r_border = gs.M - 1 - (current_shrink_depth + 1);
    //       min_c_border = current_shrink_depth + 1; max_c_border = gs.N - 1 - (current_shrink_depth + 1);
    //       (Adjust if K_shrink_period is 0 or game hasn't started shrinking)

    //    c. If the BFS cannot reach the border (it's bounded by P_COLOR cells or obstacles),
    //       then all cells visited by this BFS (that are not obstacles) form an enclosed region.
    //       Fill these visited cells with P_COLOR.
    //    d. Check for players of OTHER_COLORs within this newly filled region and eliminate them.

    // This is a highly simplified sketch. A robust implementation is complex.
    // Let's try a common approach: iterate all empty/opponent cells. For each, run a BFS.
    // If BFS hits border, it's not enclosed. If BFS is bounded ONLY by one color and obstacles, it's enclosed by that color.

    std::vector<std::vector<bool>> visited_global(gs.M, std::vector<bool>(gs.N, false));
    int current_shrink_level = 0;
    if (gs.K_shrink_period > 0 && current_sim_turn_num > 0)
    {
        // How many layers are ALREADY sealed before this turn's potential shrink
        current_shrink_level = (current_sim_turn_num - 1) / gs.K_shrink_period;
    }

    for (int r_start = 0; r_start < gs.M; ++r_start)
    {
        for (int c_start = 0; c_start < gs.N; ++c_start)
        {
            if (visited_global[r_start][c_start] || gs.grid[r_start][c_start] == OBSTACLE_CELL ||
                (gs.grid[r_start][c_start] >= 'a' && gs.grid[r_start][c_start] <= 'd') || // Already sealed
                (gs.grid[r_start][c_start] >= 'A' && gs.grid[r_start][c_start] <= 'D')    // Already player color
            )
            {
                continue;
            }

            // Start BFS from (r_start, c_start) which is currently EMPTY_CELL
            std::queue<BFS_Node> q;
            q.push({r_start, c_start});
            std::vector<std::vector<bool>> visited_bfs(gs.M, std::vector<bool>(gs.N, false));
            visited_bfs[r_start][c_start] = true;
            visited_global[r_start][c_start] = true;

            std::vector<BFS_Node> component_cells; // Cells in this floodable area
            component_cells.push_back({r_start, c_start});

            bool touches_border = false;
            std::set<char> bordering_player_colors;

            while (!q.empty())
            {
                BFS_Node curr = q.front();
                q.pop();

                // Check if curr is on the effective game border
                // Effective border: row `sl` or `M-1-sl`, col `sl` or `N-1-sl` where `sl` is current_shrink_level
                // Or simpler: if it's row 0, M-1, col 0, N-1 AND that cell isn't supposed to be sealed yet.
                bool is_on_map_edge = (curr.r == 0 || curr.r == gs.M - 1 || curr.c == 0 || curr.c == gs.N - 1);
                bool cell_is_currently_sealed = (gs.grid[curr.r][curr.c] >= 'a' && gs.grid[curr.r][curr.c] <= 'd') ||
                                                gs.cell_will_be_sealed_at_end_of_turn(curr.r, curr.c, current_sim_turn_num - 1); // Check if sealed by *previous* turn if any

                // A cell is on the "true" open border if it's on an edge row/col AND that edge layer isn't sealed yet.
                int cell_layer = std::min({curr.r, gs.M - 1 - curr.r, curr.c, gs.N - 1 - curr.c});
                if (cell_layer <= current_shrink_level)
                { // This cell is part of an already sealed or currently sealing layer
                  // If it's an obstacle already or player color, it might form part of boundary.
                  // If it's empty and part of sealed layer, it should become obstacle after shrink.
                }
                else
                { // Cell is in the "playable" area
                    if (curr.r == current_shrink_level + 1 || curr.r == gs.M - 1 - (current_shrink_level + 1) ||
                        curr.c == current_shrink_level + 1 || curr.c == gs.N - 1 - (current_shrink_level + 1))
                    {
                        touches_border = true;
                    }
                }

                for (const auto &dir : DIRECTIONS)
                {
                    if (dir.dr == 0 && dir.dc == 0)
                        continue;
                    int nr = curr.r + dir.dr;
                    int nc = curr.c + dir.dc;

                    if (gs.is_within_bounds(nr, nc))
                    {
                        if (visited_bfs[nr][nc])
                            continue;

                        char neighbor_char = gs.grid[nr][nc];
                        if (neighbor_char == EMPTY_CELL)
                        {
                            visited_bfs[nr][nc] = true;
                            visited_global[nr][nc] = true; // Mark globally to avoid re-BFSing this component
                            q.push({nr, nc});
                            component_cells.push_back({nr, nc});
                        }
                        else if (neighbor_char >= 'A' && neighbor_char <= 'D')
                        { // Player color
                            bordering_player_colors.insert(neighbor_char);
                        }
                        else if (neighbor_char == OBSTACLE_CELL || (neighbor_char >= 'a' && neighbor_char <= 'd'))
                        {
                            // Obstacle or sealed cell, forms part of boundary
                        }
                    }
                    else
                    { // Out of bounds implies it touches the absolute border
                        touches_border = true;
                    }
                }
            } // End of BFS for one component

            if (!touches_border && bordering_player_colors.size() == 1)
            {
                char enclosing_color = *bordering_player_colors.begin();
                // Fill component_cells with enclosing_color
                for (const auto &cell_to_fill : component_cells)
                {
                    gs.grid[cell_to_fill.r][cell_to_fill.c] = enclosing_color;
                }
                // Eliminate players inside this newly filled region
                for (auto &p_check : gs.players)
                {
                    if (!p_check.eliminated && p_check.color_char != enclosing_color)
                    {
                        for (const auto &filled_cell : component_cells)
                        {
                            if (p_check.x == filled_cell.r && p_check.y == filled_cell.c)
                            {
                                p_check.eliminated = true;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

void shrink_map_and_eliminate_players(GameState &gs, int turn_being_concluded)
{
    if (turn_being_concluded <= 0 || gs.K_shrink_period <= 0 || turn_being_concluded % gs.K_shrink_period != 0)
    {
        return; // No shrink this turn
    }
    int shrink_iteration = turn_being_concluded / gs.K_shrink_period;
    int layer_to_seal_idx = shrink_iteration - 1;

    if (layer_to_seal_idx < 0)
        return;

    for (int r = 0; r < gs.M; ++r)
    {
        for (int c = 0; c < gs.N; ++c)
        {
            bool is_on_sealing_border = (r == layer_to_seal_idx || r == gs.M - 1 - layer_to_seal_idx ||
                                         c == layer_to_seal_idx || c == gs.N - 1 - layer_to_seal_idx);

            // Only consider cells that are *at this specific layer*, not deeper ones already sealed.
            // A cell (r,c) is in layer k if min(r, M-1-r, c, N-1-c) == k.
            int cell_layer = std::min({r, gs.M - 1 - r, c, gs.N - 1 - c});

            if (is_on_sealing_border && cell_layer == layer_to_seal_idx)
            {
                // Eliminate players on this cell
                for (auto &p : gs.players)
                {
                    if (!p.eliminated && p.x == r && p.y == c)
                    {
                        p.eliminated = true;
                    }
                }
                // Update grid cell char
                if (gs.grid[r][c] == EMPTY_CELL)
                    gs.grid[r][c] = OBSTACLE_CELL;
                else if (gs.grid[r][c] >= 'A' && gs.grid[r][c] <= 'D')
                    gs.grid[r][c] = tolower(gs.grid[r][c]);
                // else if already OBSTACLE or lowercase, it stays.
            }
        }
    }
}

void decrement_item_durations_and_despawn(GameState &gs)
{
    // Player item durations
    for (auto &p : gs.players)
    {
        if (p.speed_boost_turns_left_sim > 0)
        {
            p.speed_boost_turns_left_sim--;
        }
        // Oil slick expiry if not used (has_oil_slick_sim becomes false when turns_to_expire_sim hits 0)
        if (p.has_oil_slick_sim && p.oil_slick_turns_to_expire_sim > 0)
        {
            p.oil_slick_turns_to_expire_sim--;
            if (p.oil_slick_turns_to_expire_sim == 0)
            {
                p.has_oil_slick_sim = false;
            }
        }
    }
    // Map item despawning
    gs.items_on_map.erase(
        std::remove_if(gs.items_on_map.begin(), gs.items_on_map.end(),
                       [](ItemOnMap &item)
                       {
                           item.turns_to_despawn--;
                           return item.turns_to_despawn <= 0;
                       }),
        gs.items_on_map.end());
    // Item Spawning: "lượt thứ 10 ... 2 trong 3 loại vật phẩm sẽ xuất hiện"
    // This is complex to simulate fairly in MaxN. Usually, MaxN assumes no new major random events.
    // For a full sim, if gs.game_turn_number == 10 (being concluded), you'd add 2 random items.
    // This adds non-determinism that MaxN struggles with. We'll skip new item spawning in sim for now.
}

// This function simulates all end-of-turn effects after all players have made their moves.
void simulate_full_end_of_turn(GameState &gs, int turn_being_concluded)
{
    // Order of operations as per problem:
    // 1. Player movement and immediate item effects (Paint Bomb) are handled by apply_chosen_move_for_player_in_sim
    // 2. "Tại mọi thời điểm, khi người chơi đứng trên một ô ... Nếu đó là ô trống, ô này sẽ được tô..." (Standard coloring)
    color_tiles_from_player_positions(gs);

    // 3. "Tại cuối mỗi lượt, nếu các ô cùng một màu COLOR tạo thành một vùng khép kín..."
    detect_and_fill_enclosures_and_eliminate_players(gs, turn_being_concluded);

    // Recolor after enclosure fill, in case a player was on an empty cell that got enclosed then filled.
    // color_tiles_from_player_positions(gs); // Might be needed if enclosure changes player's cell type

    // 4. "Khi kết thúc lượt i * K ... bản đồ sẽ thu hẹp"
    shrink_map_and_eliminate_players(gs, turn_being_concluded);

    // 5. Decrement item durations / Despawn items on map
    decrement_item_durations_and_despawn(gs);
}

void apply_paint_bomb_effect(GameState &gs, int center_r, int center_c, char player_color)
{
    for (int dr = -2; dr <= 2; ++dr)
    {
        for (int dc = -2; dc <= 2; ++dc)
        {
            int r = center_r + dr;
            int c = center_c + dc;
            if (gs.is_within_bounds(r, c))
            {
                char current_cell = gs.grid[r][c];
                if (current_cell == OBSTACLE_CELL || (current_cell >= 'a' && current_cell <= 'd'))
                {
                    continue; // Cannot paint obstacles or sealed cells
                }

                // "Nếu trong vùng có người chơi khác, các ô đó không bị tô"
                // "Nếu một ô có nhiều hơn một người chơi thì màu của ô đó giữ nguyên"
                bool other_player_on_cell = false;
                int players_on_this_cell_count = 0;
                for (const auto &p_check : gs.players)
                {
                    if (!p_check.eliminated && p_check.x == r && p_check.y == c)
                    {
                        players_on_this_cell_count++;
                        if (p_check.color_char != player_color)
                        {
                            other_player_on_cell = true;
                        }
                    }
                }
                if (other_player_on_cell || players_on_this_cell_count > 1)
                {
                    continue; // Condition to not paint
                }
                gs.grid[r][c] = player_color;
            }
        }
    }
}

std::vector<MoveAction> generate_possible_moves_for_player(const GameState &gs, int player_id_to_move)
{
    std::vector<MoveAction> moves;
    if (player_id_to_move < 0 || player_id_to_move >= gs.players.size() || gs.players[player_id_to_move].eliminated)
    {
        return moves;
    }

    const Player &p = gs.players[player_id_to_move];
    int current_x = p.x;
    int current_y = p.y;

    // 1-step moves (including stay)
    for (const auto &dir : DIRECTIONS)
    {
        int next_x = current_x + dir.dr;
        int next_y = current_y + dir.dc;
        int steps = (dir.dr == 0 && dir.dc == 0) ? 0 : 1;

        if (gs.is_target_cell_valid_for_landing(next_x, next_y, gs.game_turn_number))
        {
            moves.emplace_back(player_id_to_move, next_x, next_y, false, steps);
        }
    }

    // 2-step moves (Speed Boost)
    if (p.speed_boost_turns_left_sim > 0)
    {
        for (const auto &dir : DIRECTIONS)
        {
            if (dir.dr == 0 && dir.dc == 0)
                continue; // Cannot boost "stay"

            int inter_x = current_x + dir.dr;
            int inter_y = current_y + dir.dc;
            int final_x = current_x + 2 * dir.dr;
            int final_y = current_y + 2 * dir.dc;

            // Path 1: No oil slick needed
            // Intermediate cell must be passable (not obstacle/sealed unless pathing through with oil)
            // Final cell must be valid for landing.
            bool inter_valid_no_oil = gs.is_within_bounds(inter_x, inter_y) &&
                                      gs.grid[inter_x][inter_y] != OBSTACLE_CELL &&
                                      !(gs.grid[inter_x][inter_y] >= 'a' && gs.grid[inter_x][inter_y] <= 'd') &&
                                      !gs.cell_will_be_sealed_at_end_of_turn(inter_x, inter_y, gs.game_turn_number);

            if (inter_valid_no_oil && gs.is_target_cell_valid_for_landing(final_x, final_y, gs.game_turn_number))
            {
                moves.emplace_back(player_id_to_move, final_x, final_y, false, 2);
            }
            // Path 2: Using Oil Slick for the intermediate step
            else if (p.has_oil_slick_sim && p.oil_slick_turns_to_expire_sim > 0)
            {
                // Intermediate cell IS an obstacle/sealed, but can pass with oil
                bool inter_is_passable_with_oil = gs.is_within_bounds(inter_x, inter_y) &&
                                                  (gs.grid[inter_x][inter_y] == OBSTACLE_CELL || (gs.grid[inter_x][inter_y] >= 'a' && gs.grid[inter_x][inter_y] <= 'd')) &&
                                                  !gs.cell_will_be_sealed_at_end_of_turn(inter_x, inter_y, gs.game_turn_number); // Cannot pass through cell that disappears this turn

                if (inter_is_passable_with_oil && gs.is_target_cell_valid_for_landing(final_x, final_y, gs.game_turn_number))
                {
                    moves.emplace_back(player_id_to_move, final_x, final_y, true, 2);
                }
            }
        }
    }

    // Ensure unique moves if multiple paths lead to same (next_x, next_y, oil_used, steps) state.
    // For now, let MaxN handle multiple paths if they arise. A more robust way is to use a set.
    if (moves.empty())
    { // If truly no moves, player must stay
        if (gs.is_target_cell_valid_for_landing(current_x, current_y, gs.game_turn_number))
        { // Can they stay?
            moves.emplace_back(player_id_to_move, current_x, current_y, false, 0);
        }
        else
        {
            // Player is trapped on a cell that will eliminate them, or no valid spot.
            // Outputting current position is game's default for invalid move, let judge handle elimination.
            // For simulation, this means they are stuck. MaxN will find this has bad score.
            moves.emplace_back(player_id_to_move, current_x, current_y, false, 0); // Still "attempt" to stay
        }
    }
    return moves;
}

// --- Static Evaluation Function ---
std::vector<double> static_evaluate_state_all_players(const GameState &gs)
{
    std::vector<double> player_scores(gs.players.size(), 0.0);

    for (size_t i = 0; i < gs.players.size(); ++i)
    {
        const Player &p_eval = gs.players[i];
        if (p_eval.eliminated)
        {
            player_scores[i] = -1e9; // Very large penalty for being eliminated
            continue;
        }

        double current_p_score = 0;
        // 1. Tile Count
        int tiles_owned = 0;
        for (int r = 0; r < gs.M; ++r)
        {
            for (int c = 0; c < gs.N; ++c)
            {
                if (gs.grid[r][c] == p_eval.color_char)
                {
                    tiles_owned++;
                }
            }
        }
        current_p_score += tiles_owned * 100.0;

        // 2. Safety from Shrinking Edge
        int s_level_p_pos = std::min({p_eval.x, gs.M - 1 - p_eval.x, p_eval.y, gs.N - 1 - p_eval.y});
        if (gs.K_shrink_period > 0 && gs.game_turn_number > 0)
        {
            int turn_of_shrink_for_p_layer = (s_level_p_pos + 1) * gs.K_shrink_period;
            int turns_until_p_layer_shrinks = turn_of_shrink_for_p_layer - gs.game_turn_number; // Game turn, not sim turn for eval of safety

            if (turns_until_p_layer_shrinks <= 0)
            {                              // Will shrink this game turn or already should have
                current_p_score -= 5000.0; // Very unsafe
            }
            else if (turns_until_p_layer_shrinks <= gs.K_shrink_period)
            {
                current_p_score -= (gs.K_shrink_period - turns_until_p_layer_shrinks + 1) * 80.0;
            }
            else
            {
                current_p_score += s_level_p_pos * 5.0; // Bonus for being deeper inside
            }
        }
        else
        {
            current_p_score += s_level_p_pos * 5.0; // No shrink, still good to be central
        }

        // 3. Item bonuses (possession)
        if (p_eval.has_oil_slick_sim)
            current_p_score += 150.0;
        if (p_eval.speed_boost_turns_left_sim > 0)
            current_p_score += p_eval.speed_boost_turns_left_sim * 50.0;
        // Paint bomb is immediate, so no "possession" bonus here. Its value is in the tiles it colored.

        // 4. Proximity to unowned items on map (small bonus)
        for (const auto &item : gs.items_on_map)
        {
            int dist_to_item = std::abs(p_eval.x - item.r) + std::abs(p_eval.y - item.c);
            if (dist_to_item <= 2)
            { // Close to an item
                bool can_pickup_this_item_type = !(p_eval.speed_boost_turns_left_sim > 0 || p_eval.has_oil_slick_sim);
                if (can_pickup_this_item_type)
                    current_p_score += (3 - dist_to_item) * 30.0;
            }
        }

        // 5. Voronoi-like heuristic: count empty cells closer to this player than any other
        // This is more complex, skip for now for "full implementation" focus on rules.

        player_scores[i] = current_p_score;
    }
    return player_scores;
}

// --- MaxN Search Struct & Function ---
struct MaxN_Result
{
    MoveAction best_move_for_our_bot; // Only populated at the root call for our bot
    std::vector<double> resulting_scores_for_all_players;

    MaxN_Result()
    {
        best_move_for_our_bot.player_id = -1; // Invalid by default
        resulting_scores_for_all_players.assign(MAX_PLAYERS_POSSIBLE, -std::numeric_limits<double>::infinity());
    }
};

MaxN_Result max_n_search_recursive(
    GameState current_sim_state,
    int depth,
    int player_idx_currently_deciding,                     // Whose turn is it to pick an action in this node
    std::vector<MoveAction> &actions_for_this_turn_so_far, // Actions collected for players 0 to player_idx_currently_deciding-1
    int sim_turn_number,                                   // The game turn number being simulated
    int our_actual_bot_id                                  // The ID of our bot (always 0 in this setup)
)
{
    if (is_time_up_global())
    {
        MaxN_Result timeout_res; // Default bad scores
        return timeout_res;
    }

    // ----- Base Case 1: All players have decided their action for this simulated turn -----
    if (player_idx_currently_deciding >= current_sim_state.players.size() || player_idx_currently_deciding >= MAX_PLAYERS_POSSIBLE)
    {
        GameState state_after_all_actions = current_sim_state; // Make a copy to apply actions
        // Apply all collected actions for this turn
        for (const auto &action : actions_for_this_turn_so_far)
        {
            if (action.is_valid())
            { // Make sure it's not a placeholder for an eliminated player
              // The state already reflects moves up to player_idx_currently_deciding-1.
              // This step is more about preparing the state for simulate_full_end_of_turn.
              // The actual application of moves into 'state_after_all_actions' should happen
              // as players decide, or here if we pass down base_state.
              // Let's assume 'current_sim_state' has moves applied up to player_idx_currently_deciding-1.
              // No, `actions_for_this_turn_so_far` are applied to a *copy* of the state from *before* this turn's actions.

                // Revisit: The state should be copied at the beginning of exploring a turn.
                // Actions are applied to this fresh copy.
                // The `current_sim_state` passed to the *next player's decision* should be the one *after the previous player's move*.
                // This means the current structure of `max_n_search_recursive` which passes `current_sim_state` down needs refinement
                // OR `actions_for_this_turn_so_far` are applied to a base state for the turn *at this point*.

                // For simplicity with current structure: assume actions are applied sequentially to current_sim_state
                // when it was their turn. So, current_sim_state *is* state_after_all_actions up to current point.
            }
        }
        // Now apply end-of-turn effects
        simulate_full_end_of_turn(state_after_all_actions, sim_turn_number);

        // Base Case 2: Max depth reached OR game over in state_after_all_actions
        bool game_is_over = false;
        int active_players = 0;
        for (const auto &p : state_after_all_actions.players)
            if (!p.eliminated)
                active_players++;
        if (active_players <= 1)
            game_is_over = true;

        if (depth == 0 || game_is_over)
        {
            MaxN_Result leaf_res;
            leaf_res.resulting_scores_for_all_players = static_evaluate_state_all_players(state_after_all_actions);
            return leaf_res;
        }

        // --- Not a leaf: Recurse for the next full game turn (depth - 1) ---
        // Start decision for next turn with player 0 (or first active player)
        std::vector<MoveAction> next_sim_turn_actions_placeholder(state_after_all_actions.players.size(), MoveAction(-1, -1, -1));
        int first_player_next_turn = 0;
        while (first_player_next_turn < state_after_all_actions.players.size() && state_after_all_actions.players[first_player_next_turn].eliminated)
        {
            first_player_next_turn++;
        }
        if (first_player_next_turn >= state_after_all_actions.players.size())
        { // All eliminated
            MaxN_Result res;
            res.resulting_scores_for_all_players = static_evaluate_state_all_players(state_after_all_actions);
            return res;
        }

        return max_n_search_recursive(state_after_all_actions, depth - 1,
                                      first_player_next_turn,
                                      next_sim_turn_actions_placeholder,
                                      sim_turn_number + 1,
                                      our_actual_bot_id);
    } // ----- End of Base Case 1 (all players decided for current turn) -----

    // --- Current player (player_idx_currently_deciding) makes their decision ---
    // Skip if this player is eliminated
    if (player_idx_currently_deciding < current_sim_state.players.size() &&
        current_sim_state.players[player_idx_currently_deciding].eliminated)
    {

        // Assign a "no-op" action for the eliminated player
        if (player_idx_currently_deciding < actions_for_this_turn_so_far.size())
        {
            actions_for_this_turn_so_far[player_idx_currently_deciding] = MoveAction(player_idx_currently_deciding, -1, -1); // Stay/No-op
        }
        // Recurse for the next player in the same turn
        return max_n_search_recursive(current_sim_state, depth, // Depth unchanged
                                      player_idx_currently_deciding + 1,
                                      actions_for_this_turn_so_far,
                                      sim_turn_number,
                                      our_actual_bot_id);
    }

    std::vector<MoveAction> possible_moves = generate_possible_moves_for_player(current_sim_state, player_idx_currently_deciding);

    if (possible_moves.empty())
    { // Player is trapped
        if (player_idx_currently_deciding < actions_for_this_turn_so_far.size())
        {
            int stay_x = -1, stay_y = -1; // Default for safety
            if (player_idx_currently_deciding < current_sim_state.players.size())
            {
                stay_x = current_sim_state.players[player_idx_currently_deciding].x;
                stay_y = current_sim_state.players[player_idx_currently_deciding].y;
            }
            actions_for_this_turn_so_far[player_idx_currently_deciding] = MoveAction(player_idx_currently_deciding, stay_x, stay_y, false, 0); // Stay
        }
        return max_n_search_recursive(current_sim_state, depth, // Depth unchanged
                                      player_idx_currently_deciding + 1,
                                      actions_for_this_turn_so_far,
                                      sim_turn_number,
                                      our_actual_bot_id);
    }

    MaxN_Result best_outcome_for_this_player_decision; // Stores the set of scores from the path they choose
    // Initialize score for current player to very low
    if (player_idx_currently_deciding < best_outcome_for_this_player_decision.resulting_scores_for_all_players.size())
    {
        best_outcome_for_this_player_decision.resulting_scores_for_all_players[player_idx_currently_deciding] = -std::numeric_limits<double>::infinity();
    }

    // Iterate through possible moves for player_idx_currently_deciding
    for (const auto &move : possible_moves)
    {
        if (is_time_up_global())
            break;

        GameState state_after_this_players_move = current_sim_state;             // Copy state before this player's move
        state_after_this_players_move.apply_chosen_move_for_player_in_sim(move); // Apply the move

        std::vector<MoveAction> next_actions_for_turn = actions_for_this_turn_so_far; // Copy
        if (player_idx_currently_deciding < next_actions_for_turn.size())
        {
            next_actions_for_turn[player_idx_currently_deciding] = move;
        }
        else
        { /* Should not happen if sized correctly */
        }

        MaxN_Result recursive_call_outcome = max_n_search_recursive(
            state_after_this_players_move,     // Pass state *after* this player's move
            depth,                             // Depth is unchanged until all players in turn have moved
            player_idx_currently_deciding + 1, // Next player to decide in this turn
            next_actions_for_turn,
            sim_turn_number,
            our_actual_bot_id);

        if (is_time_up_global() && recursive_call_outcome.resulting_scores_for_all_players.empty())
            continue;

        double current_player_score_from_this_path = -std::numeric_limits<double>::infinity();
        if (player_idx_currently_deciding < recursive_call_outcome.resulting_scores_for_all_players.size())
        {
            current_player_score_from_this_path = recursive_call_outcome.resulting_scores_for_all_players[player_idx_currently_deciding];
        }

        double best_score_found_so_far_for_this_player = -std::numeric_limits<double>::infinity();
        if (player_idx_currently_deciding < best_outcome_for_this_player_decision.resulting_scores_for_all_players.size())
        {
            best_score_found_so_far_for_this_player = best_outcome_for_this_player_decision.resulting_scores_for_all_players[player_idx_currently_deciding];
        }

        if (current_player_score_from_this_path > best_score_found_so_far_for_this_player)
        {
            best_outcome_for_this_player_decision.resulting_scores_for_all_players = recursive_call_outcome.resulting_scores_for_all_players;
            // If this is our bot making the decision (player_idx_currently_deciding == our_actual_bot_id)
            // AND we are at the root of its decision for this turn (i.e., the initial call for player 0 for the top-level turn)
            // then store this move as its best_move. This is usually handled by the iterative deepening loop.
            // Here, we store the move that led to this best outcome FOR THIS PLAYER at this node.
            if (player_idx_currently_deciding == our_actual_bot_id)
            {
                best_outcome_for_this_player_decision.best_move_for_our_bot = move;
            }
        }
    }
    return best_outcome_for_this_player_decision;
}

// --- Main Function ---
int main()
{
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    GameState root_game_state;
    root_game_state.parse_input("MAP.INP"); // Loads actual game state and player 0's items

    MoveAction final_action_to_take;
    final_action_to_take.player_id = -1; // Invalid default

    if (root_game_state.game_turn_number == 0) {
        // On turn 0, player position is -1, -1, but they are NOT eliminated.
        // We need to choose an initial spawning position.
        // The `eliminated` flag for players[0] should be considered false for placement purposes.
        // (The parsing sets it true, but for turn 0 decision, it's effectively false).

        // For turn 0, a simple heuristic for placement is usually fine. MaxN is overkill.
        std::vector<MoveAction> initial_options; // We need to generate placement options

        // Generate valid empty cells for placement
        std::vector<std::pair<int,int>> valid_spawn_points;
        for (int r = 0; r < root_game_state.M; ++r) {
            for (int c = 0; c < root_game_state.N; ++c) {
                // A spawn point must be empty and not an obstacle.
                // is_target_cell_valid_for_landing checks for obstacles and future sealing.
                // For turn 0, future sealing isn't an immediate concern for placement.
                if (root_game_state.grid[r][c] == EMPTY_CELL &&
                    !root_game_state.cell_will_be_sealed_at_end_of_turn(r,c,0) ) { // Check for sealing at end of turn 0 (unlikely to seal anything then)
                     valid_spawn_points.push_back({r,c});
                }
            }
        }

        if (!valid_spawn_points.empty()) {
            // Simplistic: pick a central-ish valid empty cell.
            int best_r = root_game_state.M / 2;
            int best_c = root_game_state.N / 2;
            double min_dist_sq_center = std::numeric_limits<double>::max();
            bool found_initial = false;

            for (const auto& sp_point : valid_spawn_points) {
                double dist_sq = pow(sp_point.first - root_game_state.M / 2.0, 2) +
                                 pow(sp_point.second - root_game_state.N / 2.0, 2);
                if (!found_initial || dist_sq < min_dist_sq_center) {
                    min_dist_sq_center = dist_sq;
                    best_r = sp_point.first;
                    best_c = sp_point.second;
                    found_initial = true;
                }
            }
            if (found_initial) {
                final_action_to_take = MoveAction(0, best_r, best_c);
            } else { // Should always find a valid spawn if map is per spec
                final_action_to_take = MoveAction(0, valid_spawn_points[0].first, valid_spawn_points[0].second);
            }
        } else { // Should not happen if map has empty cells as per problem spec
            std::cerr << "CRITICAL: No valid spawn points found on Turn 0!" << std::endl;
            final_action_to_take = MoveAction(0, root_game_state.M / 2, root_game_state.N / 2); // Ultimate failsafe
        }

    // --- END OF CORRECTED TURN 0 LOGIC ---
    } else if (root_game_state.players.empty() || root_game_state.players[0].eliminated) {
        // This condition now correctly handles cases where player IS actually eliminated on subsequent turns.
        final_action_to_take = MoveAction(0, 0, 0); // Dummy output
    } else { // Game turn > 0 and player is not eliminated
        search_start_time_global = std::chrono::steady_clock::now();
        MaxN_Result best_overall_result_for_bot; 

        int max_search_depth = 3; 
        for (int d = 1; d <= max_search_depth; ++d) {
            if (is_time_up_global()) break;
            // std::cerr << "MaxN: Trying depth " << d << std::endl;

            std::vector<MoveAction> actions_for_turn_placeholder(root_game_state.players.size(), MoveAction(-1,-1,-1));
            
            root_game_state.sync_my_actual_items_to_player0_sim();

            MaxN_Result result_this_depth = max_n_search_recursive(
                root_game_state, 
                d,               
                0,               
                actions_for_turn_placeholder,
                root_game_state.game_turn_number, 
                0                
            );

            if (!is_time_up_global()) { 
                double new_score_for_bot = -std::numeric_limits<double>::infinity();
                if(!result_this_depth.resulting_scores_for_all_players.empty() && result_this_depth.resulting_scores_for_all_players.size() > 0) { // Check size
                    new_score_for_bot = result_this_depth.resulting_scores_for_all_players[0];
                }

                double old_best_score_for_bot = -std::numeric_limits<double>::infinity();
                 if(!best_overall_result_for_bot.resulting_scores_for_all_players.empty() && best_overall_result_for_bot.resulting_scores_for_all_players.size() > 0) { // Check size
                     old_best_score_for_bot = best_overall_result_for_bot.resulting_scores_for_all_players[0];
                }


                if (result_this_depth.best_move_for_our_bot.is_valid() && (d == 1 || new_score_for_bot > old_best_score_for_bot)) {
                    best_overall_result_for_bot = result_this_depth;
                }
            } else {
                // std::cerr << "MaxN: Time up during depth " << d << std::endl;
                break;
            }
        }
        
        if (best_overall_result_for_bot.best_move_for_our_bot.is_valid()) {
            final_action_to_take = best_overall_result_for_bot.best_move_for_our_bot;
        } else { 
            std::cerr << "MaxN FALLBACK! No valid move from search." << std::endl;
            std::vector<MoveAction> fallback_options = generate_possible_moves_for_player(root_game_state, 0);
            if (!fallback_options.empty()) {
                final_action_to_take = fallback_options[0]; // Simplest fallback
                double best_fallback_score = -std::numeric_limits<double>::infinity();
                 for(const auto& opt : fallback_options){
                    GameState temp_gs = root_game_state;
                    temp_gs.apply_chosen_move_for_player_in_sim(opt); 
                    std::vector<double> scores = static_evaluate_state_all_players(temp_gs);
                    if(!scores.empty() && scores.size() > 0 && scores[0] > best_fallback_score){ // Check size
                        best_fallback_score = scores[0];
                        final_action_to_take = opt;
                    }
                 }
            } else { 
                final_action_to_take = MoveAction(0, root_game_state.players[0].x, root_game_state.players[0].y);
            }
        }
    }

    // After final_action_to_take is decided for player 0 (our bot):
    // Update its *actual* item state for STATE.DAT persistence
    if (final_action_to_take.player_id == 0 && !root_game_state.players[0].eliminated)
    {
        Player &actual_bot_player_state = root_game_state.players[0]; // For direct update before saving
        bool can_pickup_now = !(root_game_state.my_actual_speed_boost_turns_left > 0 || root_game_state.my_actual_has_oil_slick);

        if (can_pickup_now)
        {
            // Check items_on_map from the *original* root_game_state, not a sim copy
            auto it = root_game_state.items_on_map.begin(); // Use root_game_state.items_on_map
            while (it != root_game_state.items_on_map.end())
            {
                if (it->r == final_action_to_take.next_x && it->c == final_action_to_take.next_y)
                {
                    if (it->type == SPEED_BOOST_ITEM)
                        root_game_state.my_actual_speed_boost_turns_left = 5;
                    // Paint bomb effect is immediate, not stored as an "active item" for next turn
                    else if (it->type == OIL_SLICK_ITEM)
                    {
                        root_game_state.my_actual_has_oil_slick = true;
                        root_game_state.my_actual_oil_slick_turns_to_expire = 5;
                    }
                    // Item is considered picked up, judge will remove it from next MAP.INP
                    break;
                }
                else
                {
                    ++it;
                }
            }
        }

        if (final_action_to_take.oil_used)
        {
            root_game_state.my_actual_has_oil_slick = false;
            root_game_state.my_actual_oil_slick_turns_to_expire = 0;
        }
        else if (root_game_state.my_actual_has_oil_slick && root_game_state.my_actual_oil_slick_turns_to_expire > 0)
        {
            root_game_state.my_actual_oil_slick_turns_to_expire--;
            if (root_game_state.my_actual_oil_slick_turns_to_expire == 0)
                root_game_state.my_actual_has_oil_slick = false;
        }
        if (root_game_state.my_actual_speed_boost_turns_left > 0)
        {
            root_game_state.my_actual_speed_boost_turns_left--;
        }
    }

    root_game_state.save_my_actual_item_state();

    std::ofstream move_out_file("MOVE.OUT");
    if (!move_out_file.is_open())
    {
        std::cerr << "Error: Cannot open MOVE.OUT" << std::endl;
        return 1;
    }
    move_out_file << final_action_to_take.next_x << " " << final_action_to_take.next_y << std::endl;
    move_out_file.close();

    return 0;
}