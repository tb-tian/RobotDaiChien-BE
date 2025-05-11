#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <tuple>
#include <chrono> // For time management
#include <limits>
#include <random> // For tie-breaking if needed (not implemented deeply here)

// --- Constants (same as before) ---
const char EMPTY_CELL = '.';
const char OBSTACLE_CELL = '#';
// ... (other constants)

// --- Helper Structs (Player, ItemOnMap, Direction - same as before) ---
struct Direction
{
    int dr, dc;
};
const std::vector<Direction> DIRECTIONS = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}, {0, 0}};

struct Player
{
    int id; // Unique ID (0 for my_player, 1+ for opponents)
    int x, y;
    char color_char;
    bool eliminated;
    // Item states for this player (if simulating for opponents)
    int speed_boost_turns_left_sim;
    int oil_slick_turns_to_expire_sim;
    bool has_oil_slick_sim;

    Player(int _id = -1, int _x = -1, int _y = -1, char _color = ' ') : id(_id), x(_x), y(_y), color_char(_color),
                                                                        eliminated(_x == -1 && _y == -1),
                                                                        speed_boost_turns_left_sim(0), oil_slick_turns_to_expire_sim(0), has_oil_slick_sim(false)
    {
    }
};

struct ItemOnMap
{
    int r, c;
    char type;
};

struct MoveAction
{
    int player_id; // Whose move it is
    int next_x, next_y;
    bool oil_used;
    int steps; // 0 for stay, 1 for 1-step, 2 for 2-step

    MoveAction(int pid = -1, int nx = -1, int ny = -1, bool ou = false, int s = 0) : player_id(pid), next_x(nx), next_y(ny), oil_used(ou), steps(s) {}
};

// Forward declaration
class GameState;
std::vector<MoveAction> generate_possible_moves_for_player(const GameState &gs, int player_id_to_move);
double static_evaluate_state(const GameState &gs, char my_actual_color_char);
// This function needs to simulate coloring, enclosure, etc.
void simulate_end_of_turn_effects(GameState &gs_after_all_moves);

class GameState
{
public:
    int M, N, K_shrink_period, current_turn;
    // Player 0 is always "my_player" from the input perspective
    // Opponents will be player 1, 2, ...
    std::vector<Player> players; // players[0] is my_player
    std::vector<std::vector<char>> grid;
    std::vector<ItemOnMap> items_on_map_sim; // Items present in this simulated state

    // Item states specific to my_player (from STATE.DAT and current turn pickups)
    // These are for the *actual* game state, not the deep simulation copies necessarily
    int my_actual_speed_boost_turns_left;
    int my_actual_oil_slick_turns_to_expire;
    bool my_actual_has_oil_slick;
    bool my_actual_paint_bomb_just_picked_up;

    GameState() : M(0), N(0), K_shrink_period(0), current_turn(0),
                  my_actual_speed_boost_turns_left(0), my_actual_oil_slick_turns_to_expire(0),
                  my_actual_has_oil_slick(false), my_actual_paint_bomb_just_picked_up(false) {}

    // Copy constructor for simulations
    GameState(const GameState &other) = default; // Default works for most members
                                                 // Deep copy for vectors is handled by std::vector's copy ctor.

    char get_my_actual_color_char() const
    {
        if (!players.empty())
            return players[0].color_char;
        return ' '; // Should not happen if parsed correctly
    }

    void parse_input(const std::string &filename = "MAP.INP")
    {
        std::ifstream ifs(filename);
        // ... (ifs open check) ...
        ifs >> M >> N >> K_shrink_period >> current_turn;

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
        for (int i = 0; i < M; ++i)
            for (int j = 0; j < N; ++j)
                ifs >> grid[i][j];

        int num_map_items;
        ifs >> num_map_items;
        items_on_map_sim.clear();
        for (int i = 0; i < num_map_items; ++i)
        {
            ItemOnMap item;
            ifs >> item.r >> item.c >> item.type;
            items_on_map_sim.push_back(item);
        }
        ifs.close();
        load_my_actual_item_state(); // Load persistent state
    }

    void save_my_actual_item_state(const std::string &filename = "STATE.DAT")
    {
        std::ofstream ofs(filename);
        // ... (ofs open check) ...
        ofs << my_actual_speed_boost_turns_left << std::endl;
        ofs << my_actual_oil_slick_turns_to_expire << std::endl;
        ofs << (my_actual_has_oil_slick ? 1 : 0) << std::endl;
        ofs.close();
    }

    void load_my_actual_item_state(const std::string &filename = "STATE.DAT")
    {
        std::ifstream ifs(filename);
        my_actual_paint_bomb_just_picked_up = false; // Reset flag
        if (!ifs.is_open())
        { /* set defaults */
            return;
        }
        int has_oil_int = 0;
        ifs >> my_actual_speed_boost_turns_left >> my_actual_oil_slick_turns_to_expire >> has_oil_int;
        my_actual_has_oil_slick = (has_oil_int == 1);
        ifs.close();

        // Sync loaded state to players[0] for simulation consistency
        if (!players.empty())
        {
            players[0].speed_boost_turns_left_sim = my_actual_speed_boost_turns_left;
            players[0].oil_slick_turns_to_expire_sim = my_actual_oil_slick_turns_to_expire;
            players[0].has_oil_slick_sim = my_actual_has_oil_slick;
        }
    }

    bool is_within_bounds(int r, int c) const { /* ... same ... */ return r >= 0 && r < M && c >= 0 && c < N; }
    bool cell_will_be_sealed_this_turn(int r, int c, int turn_to_check) const
    {
        // Use turn_to_check instead of this->current_turn for simulation
        if (turn_to_check > 0 && K_shrink_period > 0 && turn_to_check % K_shrink_period == 0)
        {
            int sealed_layer_index = (turn_to_check / K_shrink_period) - 1;
            if (sealed_layer_index < 0)
                sealed_layer_index = 0;
            return (r == sealed_layer_index || r == M - 1 - sealed_layer_index ||
                    c == sealed_layer_index || c == N - 1 - sealed_layer_index);
        }
        return false;
    }
    bool is_valid_for_move(int r, int c, int turn_for_seal_check, bool can_pass_one_obstacle = false) const
    {
        if (!is_within_bounds(r, c))
            return false;
        char cell_content = grid[r][c];
        if (cell_content == OBSTACLE_CELL || (cell_content >= 'a' && cell_content <= 'd'))
        {
            return can_pass_one_obstacle;
        }
        if (cell_will_be_sealed_this_turn(r, c, turn_for_seal_check))
            return false;
        return true;
    }

    // Applies a single player's move, updates their position and item status if they pick one up.
    // DOES NOT handle end-of-turn global effects like coloring, map shrink.
    // Returns true if move was applied (even if it was just "staying due to invalid move")
    bool apply_single_player_move_action(const MoveAction &move)
    {
        if (move.player_id < 0 || move.player_id >= players.size() || players[move.player_id].eliminated)
        {
            return false; // Invalid player or eliminated
        }
        Player &p = players[move.player_id];

        // Basic validation (if move itself is malformed, e.g. next_x,y are invalid initially)
        // generate_possible_moves_for_player should give valid target cells.
        // If move.next_x,y is an obstacle and oil_used is false, it's an invalid move by game rules -> player stands still.
        // The move generation should filter impossible moves. This is more about applying a chosen one.

        int final_x = move.next_x;
        int final_y = move.next_y;

        // Check if the proposed move is valid according to game rules (pathing)
        // This logic is complex and overlaps with generate_possible_moves
        // For now, assume 'move' is a validly generated choice.
        // If judge makes player stand still for bad output, our simulation must too.
        // For Minimax, we typically assume we can make the chosen move.
        // The core of this function is updating player pos and handling item pickup.

        p.x = final_x;
        p.y = final_y;

        // Handle item pickup for this player
        bool can_pickup = !(p.speed_boost_turns_left_sim > 0 || p.has_oil_slick_sim);
        if (can_pickup)
        {
            auto it = items_on_map_sim.begin();
            while (it != items_on_map_sim.end())
            {
                if (it->r == final_x && it->c == final_y)
                {
                    // Player picks up item
                    if (it->type == 'G')
                        p.speed_boost_turns_left_sim = 5;
                    else if (it->type == 'E')
                    { /* Paint bomb logic for actual player handled differently */
                    }
                    else if (it->type == 'F')
                    {
                        p.has_oil_slick_sim = true;
                        p.oil_slick_turns_to_expire_sim = 5;
                    }
                    it = items_on_map_sim.erase(it); // Item removed from map
                    break;
                }
                else
                {
                    ++it;
                }
            }
        }

        // Decrement this player's item durations (if they had them before this move)
        // This needs to be careful: if they just picked up speed boost, it's 5.
        // If they *used* oil slick for THIS move, p.has_oil_slick_sim should be false now.
        if (move.oil_used)
        {
            p.has_oil_slick_sim = false;
            p.oil_slick_turns_to_expire_sim = 0;
        }
        else if (p.has_oil_slick_sim && p.oil_slick_turns_to_expire_sim > 0)
        {
            p.oil_slick_turns_to_expire_sim--;
            if (p.oil_slick_turns_to_expire_sim == 0)
                p.has_oil_slick_sim = false;
        }
        if (p.speed_boost_turns_left_sim > 0)
        { // Don't decrement if just picked up
          // This needs to be handled carefully. Speed boost is active FOR the move.
          // Duration reduces AFTER the turn it's used in.
          // Let's assume decrement happens at end of full turn simulation.
        }
        return true;
    }

    // Placeholder for complex end-of-turn game logic
    void simulate_full_end_of_turn(int turn_number_being_concluded)
    {
        // 1. Tô màu based on player positions (if not multiple players on same cell)
        std::map<std::pair<int, int>, int> players_on_cell_count;
        for (const auto &p : players)
        {
            if (!p.eliminated)
                players_on_cell_count[{p.x, p.y}]++;
        }
        for (const auto &p : players)
        {
            if (!p.eliminated && players_on_cell_count[{p.x, p.y}] == 1)
            {
                if (is_within_bounds(p.x, p.y) && grid[p.x][p.y] != OBSTACLE_CELL && !(grid[p.x][p.y] >= 'a' && grid[p.x][p.y] <= 'd'))
                {
                    grid[p.x][p.y] = p.color_char;
                }
            }
        }

        // 2. Vùng khép kín (This is VERY complex - requires BFS/DFS and border checks)
        //    - For each color, find connected components.
        //    - Check if any component is "closed" (cannot reach map edge without crossing another color/obstacle).
        //    - Fill closed regions.
        //    - Eliminate players inside opponent's newly closed regions.
        //    THIS IS A MAJOR BOTTLENECK AND COMPLEXITY FOR ACCURATE SIMULATION.
        //    For a basic Minimax, this might be simplified or approximated.

        // 3. Thu hẹp bản đồ (Map Shrinking)
        if (turn_number_being_concluded > 0 && K_shrink_period > 0 && turn_number_being_concluded % K_shrink_period == 0)
        {
            int layer_to_seal = (turn_number_being_concluded / K_shrink_period) - 1;
            if (layer_to_seal < 0)
                layer_to_seal = 0;

            for (int r = 0; r < M; ++r)
            {
                for (int c = 0; c < N; ++c)
                {
                    bool is_on_sealing_border = (r == layer_to_seal || r == M - 1 - layer_to_seal ||
                                                 c == layer_to_seal || c == N - 1 - layer_to_seal);
                    if (is_on_sealing_border)
                    {
                        // Check if player is on this cell to be sealed
                        for (auto &p : players)
                        {
                            if (!p.eliminated && p.x == r && p.y == c)
                            {
                                p.eliminated = true;
                            }
                        }
                        // Update grid cell char
                        if (grid[r][c] == EMPTY_CELL)
                            grid[r][c] = OBSTACLE_CELL;
                        else if (grid[r][c] >= 'A' && grid[r][c] <= 'D')
                            grid[r][c] = tolower(grid[r][c]);
                        // else if already OBSTACLE_CELL or sealed (lowercase), it remains.
                    }
                }
            }
        }

        // 4. Decrement player item durations that tick down per turn
        for (auto &p : players)
        {
            if (p.speed_boost_turns_left_sim > 0)
                p.speed_boost_turns_left_sim--;
            // Oil slick expiry if not used handled in apply_single_player_move
        }

        // 5. Item Spawning/Despawning (Simplified for Minimax - usually assume current items or none)
        //    Rule: "lượt thứ 10 ... 2 trong 3 loại vật phẩm sẽ xuất hiện"
        //    Rule: "biến mất sau 10 lượt nếu không ... nhặt"
        //    This adds non-determinism. Minimax typically doesn't handle future random events well.
        //    Often, you'd evaluate based on current items or assume no new critical items appear during lookahead.
    }
};

// --- Minimax Logic ---
struct MinimaxResult
{
    MoveAction best_move;
    double score;
    MinimaxResult(MoveAction m = MoveAction(), double s = 0.0) : best_move(m), score(s) {}
};

// Global for time checking
std::chrono::steady_clock::time_point search_start_time;
double time_limit_seconds = 1.8; // Leave a small buffer from 2s

bool is_time_up()
{
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - search_start_time;
    return elapsed.count() >= time_limit_seconds;
}

// player_idx_to_move: whose turn it is in this simulated state. 0 for us (MAX), 1+ for opponents (MIN)
// For simplicity here, if player_idx_to_move > 0, it's a single MIN opponent.
MinimaxResult minimax_alpha_beta(GameState current_sim_state, int depth, double alpha, double beta, int player_idx_to_move, int turn_number_in_sim)
{
    if (is_time_up())
    { // Check time at beginning of calls
        return MinimaxResult(MoveAction(), player_idx_to_move == 0 ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity());
    }

    // Base case: depth reached or terminal state (e.g., game over - simplified here)
    // A more robust terminal check would see if no players can move or only one remains.
    bool game_effectively_over = true;
    int non_eliminated_count = 0;
    for (const auto &p : current_sim_state.players)
        if (!p.eliminated)
            non_eliminated_count++;
    if (non_eliminated_count <= 1)
        game_effectively_over = true;
    else
        game_effectively_over = false;

    if (depth == 0 || game_effectively_over)
    {
        return MinimaxResult(MoveAction(), static_evaluate_state(current_sim_state, current_sim_state.players[0].color_char));
    }

    std::vector<MoveAction> possible_moves = generate_possible_moves_for_player(current_sim_state, player_idx_to_move);

    if (possible_moves.empty())
    { // No moves for this player (trapped, eliminated)
        // If no moves, the state doesn't change for this player.
        // Need to consider if this player's turn is skipped, or if they just "stand still".
        // Then proceed to next player or evaluate.
        // For now, evaluate current state if no moves.
        return MinimaxResult(MoveAction(), static_evaluate_state(current_sim_state, current_sim_state.players[0].color_char));
    }

    // Move Ordering (simple heuristic: evaluate state after each move)
    // This is a shallow, greedy sort. More complex would be needed for strong ordering.
    std::sort(possible_moves.begin(), possible_moves.end(),
              [&](const MoveAction &a, const MoveAction &b)
              {
                  GameState state_after_a = current_sim_state;
                  state_after_a.apply_single_player_move_action(a);
                  // Not simulating full end of turn for simple move ordering here
                  double score_a = static_evaluate_state(state_after_a, current_sim_state.players[0].color_char);

                  GameState state_after_b = current_sim_state;
                  state_after_b.apply_single_player_move_action(b);
                  double score_b = static_evaluate_state(state_after_b, current_sim_state.players[0].color_char);

                  if (player_idx_to_move == 0)
                      return score_a > score_b; // MAX player
                  return score_a < score_b;     // MIN player
              });

    MoveAction best_move_for_this_level = possible_moves[0]; // Default if all fail or time up

    if (player_idx_to_move == 0)
    { // MAX Player (Our Bot)
        double max_eval = -std::numeric_limits<double>::infinity();
        for (const auto &move : possible_moves)
        {
            if (is_time_up())
                break;
            GameState next_state = current_sim_state;
            next_state.apply_single_player_move_action(move);

            // Simulate opponent(s) turn if any. For simplicity, assume 1 opponent (player_id 1)
            // If multiple opponents, this loop structure needs to change (e.g. iterate all opponents)
            // or go to next depth for the same turn_number_in_sim if players move sequentially in a turn.
            // Problem: "ở mỗi lượt, mỗi người chơi sẽ có thể thực hiện duy nhất một hành động"
            // This implies all players make their move, THEN end-of-turn effects.
            // This Minimax needs to be structured around full turns (all players move).

            // --- REVISED MINIMAX STRUCTURE FOR FULL TURNS ---
            // This requires a different approach than simple alternating player Minimax.
            // Let's assume for this call, we are deciding OUR move (player_idx_to_move == 0).
            // The recursive call would then simulate opponent moves for the SAME turn.
            // This is getting very complex for a direct translation.

            // --- SIMPLIFIED ALTERNATING PLAYER MINIMAX FOR NOW ---
            // (This is not perfectly matching the game's simultaneous move nature for all players per turn)
            // Let's assume player_idx_to_move is the player who will move *next* in an alternating sequence.

            // If we are MAX player (0), after our move, it's MIN player's (1) turn.
            // If there's only one opponent, next_player_to_move_idx becomes 1.
            // The depth decreases *after* a full cycle of players or a certain number of plies.
            // Here, let's say depth decreases per individual player move for simplicity.

            // The "turn_number_in_sim" needs to increment only after ALL players have moved in a simulated turn.
            // If we make a move (player 0), then we recurse for player 1 at the same depth (or depth-1 if thinking ply-wise)
            // and same turn_number_in_sim.
            // When player 1 (last opponent) makes their move, then we call simulate_full_end_of_turn
            // and then recurse for player 0 at depth-1 and turn_number_in_sim+1.

            // This requires careful state management of whose turn it is.
            // For now, let's stick to a simple 2-player alternating Minimax.
            // `player_idx_to_move` will toggle between 0 and 1.

            MinimaxResult result;
            if (next_state.players.size() > 1)
            { // If there's an opponent
                // After our move (player 0), it's player 1's turn in this simplified model
                result = minimax_alpha_beta(next_state, depth - 1, alpha, beta, 1, turn_number_in_sim /* turn doesn't advance yet */);
            }
            else
            {                                                             // No opponent, just evaluate state after our move
                next_state.simulate_full_end_of_turn(turn_number_in_sim); // End of turn after our move
                result.score = static_evaluate_state(next_state, next_state.players[0].color_char);
            }

            if (result.score > max_eval)
            {
                max_eval = result.score;
                best_move_for_this_level = move;
            }
            alpha = std::max(alpha, max_eval);
            if (beta <= alpha)
                break; // Alpha-beta pruning
        }
        return MinimaxResult(best_move_for_this_level, max_eval);
    }
    else
    { // MIN Player (Opponent, assumed player_idx_to_move = 1)
        double min_eval = std::numeric_limits<double>::infinity();
        for (const auto &move : possible_moves)
        {
            if (is_time_up())
                break;
            GameState next_state = current_sim_state;
            next_state.apply_single_player_move_action(move); // Opponent makes their move

            // After MIN player (1) moves, simulate end of this turn's effects
            // And then it's MAX player's (0) turn again for the next (deeper) turn
            next_state.simulate_full_end_of_turn(turn_number_in_sim); // End of current turn actions

            MinimaxResult result = minimax_alpha_beta(next_state, depth - 1, alpha, beta, 0, turn_number_in_sim + 1 /* Next turn */);

            if (result.score < min_eval)
            {
                min_eval = result.score;
                best_move_for_this_level = move; // This is opponent's best move
            }
            beta = std::min(beta, min_eval);
            if (beta <= alpha)
                break; // Alpha-beta pruning
        }
        return MinimaxResult(best_move_for_this_level, min_eval);
    }
}

// --- Move Generation (Simplified - needs full item logic from original) ---
std::vector<MoveAction> generate_possible_moves_for_player(const GameState &gs, int player_id_to_move)
{
    std::vector<MoveAction> moves;
    if (player_id_to_move < 0 || player_id_to_move >= gs.players.size() || gs.players[player_id_to_move].eliminated)
    {
        return moves; // No moves if player invalid or eliminated
    }

    const Player &p = gs.players[player_id_to_move];
    int current_x = p.x;
    int current_y = p.y;

    // 1-step moves (including stay)
    for (const auto &dir : DIRECTIONS)
    {
        int next_x = current_x + dir.dr;
        int next_y = current_y + dir.dc;
        if (gs.is_valid_for_move(next_x, next_y, gs.current_turn, false))
        { // Use gs.current_turn for seal check for THIS turn's decision
            moves.emplace_back(player_id_to_move, next_x, next_y, false, (dir.dr == 0 && dir.dc == 0) ? 0 : 1);
        }
    }

    // 2-step moves (if speed boost)
    if (p.speed_boost_turns_left_sim > 0)
    {
        for (const auto &dir : DIRECTIONS)
        {
            if (dir.dr == 0 && dir.dc == 0)
                continue;
            int inter_x = current_x + dir.dr;
            int inter_y = current_y + dir.dc;
            int final_x = current_x + 2 * dir.dr;
            int final_y = current_y + 2 * dir.dc;

            // No oil
            if (gs.is_valid_for_move(inter_x, inter_y, gs.current_turn, false) &&
                gs.is_valid_for_move(final_x, final_y, gs.current_turn, false))
            {
                moves.emplace_back(player_id_to_move, final_x, final_y, false, 2);
            }
            // With oil
            else if (p.has_oil_slick_sim && p.oil_slick_turns_to_expire_sim > 0)
            {
                bool inter_is_obstacle = false;
                if (gs.is_within_bounds(inter_x, inter_y))
                {
                    char inter_char = gs.grid[inter_x][inter_y];
                    if (inter_char == OBSTACLE_CELL || (inter_char >= 'a' && inter_char <= 'd'))
                        inter_is_obstacle = true;
                }
                // If intermediate is obstacle AND final is clear
                if (inter_is_obstacle && gs.is_valid_for_move(final_x, final_y, gs.current_turn, false))
                {
                    if (!gs.cell_will_be_sealed_this_turn(inter_x, inter_y, gs.current_turn))
                    { // Don't pass through sealing cell
                        moves.emplace_back(player_id_to_move, final_x, final_y, true, 2);
                    }
                }
            }
        }
    }
    // Remove duplicate target cells if any (e.g. stay vs. move-back-to-item)
    // For now, allow duplicates, Minimax will pick one.
    // If no moves generated, player might be trapped.
    if (moves.empty() && gs.is_valid_for_move(current_x, current_y, gs.current_turn, false))
    {
        moves.emplace_back(player_id_to_move, current_x, current_y, false, 0); // Can always "stay" if current pos is valid
    }
    return moves;
}

// --- Static Evaluation (Adapted from original evaluate_move) ---
double static_evaluate_state(const GameState &gs, char my_actual_color_char)
{
    double score = 0.0;
    int my_tiles = 0;
    int opponent_tiles = 0; // Assuming one composite opponent for score difference

    const Player *my_sim_player = nullptr;
    for (const auto &p : gs.players)
    {
        if (p.color_char == my_actual_color_char)
        {
            my_sim_player = &p;
            break;
        }
    }
    if (!my_sim_player)
        return -std::numeric_limits<double>::infinity(); // Should not happen
    if (my_sim_player->eliminated)
        return -100000.0; // Heavy penalty if eliminated

    for (int r = 0; r < gs.M; ++r)
    {
        for (int c = 0; c < gs.N; ++c)
        {
            if (gs.grid[r][c] == my_actual_color_char)
            {
                my_tiles++;
            }
            else if (gs.grid[r][c] >= 'A' && gs.grid[r][c] <= 'D')
            { // Opponent's color
                opponent_tiles++;
            }
        }
    }
    score = my_tiles - opponent_tiles; // Simple score: tile difference

    // Add heuristics from original evaluate_move based on *my_sim_player*'s current state in gs
    // e.g., proximity to items, safety from shrinking edges for *my_sim_player->x, my_sim_player->y*
    // This part needs careful adaptation.

    // Positional score for my player
    if (!my_sim_player->eliminated)
    {
        int s_level_my_pos = std::min({my_sim_player->x, gs.M - 1 - my_sim_player->x, my_sim_player->y, gs.N - 1 - my_sim_player->y});
        if (gs.K_shrink_period > 0)
        {
            int turn_of_shrink_for_my_layer = (s_level_my_pos + 1) * gs.K_shrink_period;
            // Evaluate based on gs.current_turn (actual game turn, not simulated turn for this depth)
            // Or evaluate based on simulated turn if depth implies future turns
            int turns_until_my_layer_shrinks = turn_of_shrink_for_my_layer - gs.current_turn;
            if (turns_until_my_layer_shrinks <= 0)
                score -= 500; // About to be sealed or is sealed
            else if (turns_until_my_layer_shrinks <= gs.K_shrink_period)
                score -= (gs.K_shrink_period - turns_until_my_layer_shrinks + 1) * 5;
        }
    }

    // Item possession bonus (e.g. if my_sim_player->has_oil_slick_sim)
    if (my_sim_player->has_oil_slick_sim)
        score += 20;
    if (my_sim_player->speed_boost_turns_left_sim > 0)
        score += my_sim_player->speed_boost_turns_left_sim * 5;

    // TODO: Add evaluation for "vùng khép kín" (enclosures). This is critical.
    // If my_actual_color_char has a large enclosed area, add significant points.

    return score;
}

std::pair<int, int> choose_initial_position_minimax(GameState &gs)
{
    // For turn 0, Minimax is overkill. Use the same heuristic as before.
    int center_r = gs.M / 2;
    int center_c = gs.N / 2;
    if (gs.is_valid_for_move(center_r, center_c, 0, false) && gs.grid[center_r][center_c] == EMPTY_CELL)
        return {center_r, center_c};
    for (int r = 0; r < gs.M; ++r)
        for (int c = 0; c < gs.N; ++c)
            if (gs.is_valid_for_move(r, c, 0, false) && gs.grid[r][c] == EMPTY_CELL)
                return {r, c};
    return {gs.M / 2, gs.N / 2}; // Fallback
}

// --- Main ---
int main()
{
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    GameState current_game_state;
    current_game_state.parse_input("MAP.INP"); // Also loads my_actual_item_state

    MoveAction chosen_action; // Store the chosen move for player 0

    if (current_game_state.players.empty() || (current_game_state.players[0].eliminated && current_game_state.current_turn != 0))
    {
        chosen_action.next_x = 0;
        chosen_action.next_y = 0; // Dummy if eliminated
    }
    else if (current_game_state.current_turn == 0)
    {
        std::pair<int, int> start_pos = choose_initial_position_minimax(current_game_state);
        chosen_action.next_x = start_pos.first;
        chosen_action.next_y = start_pos.second;
        chosen_action.player_id = 0;
    }
    else
    {
        search_start_time = std::chrono::steady_clock::now();
        MinimaxResult result;
        // Iterative Deepening
        int max_depth_to_try = 5; // Adjust based on branching factor and time
        for (int d = 1; d <= max_depth_to_try; ++d)
        {
            if (is_time_up())
                break;
            // std::cerr << "Trying depth: " << d << std::endl;
            // For Minimax, player_idx_to_move = 0 (our bot, MAX player)
            // turn_number_in_sim = current_game_state.current_turn
            MinimaxResult current_depth_result = minimax_alpha_beta(
                current_game_state, d,
                -std::numeric_limits<double>::infinity(),
                std::numeric_limits<double>::infinity(),
                0, // Our player's turn (ID 0)
                current_game_state.current_turn);

            if (!is_time_up())
            { // Only accept result if search for this depth completed
                if (d == 1 || current_depth_result.score > result.score)
                { // If first valid result or better
                    result = current_depth_result;
                }
            }
            else
            {
                // std::cerr << "Time up during depth " << d << std::endl;
                break;
            }
            // If best_move is still default, and we got a valid move from shallow depth
            if (chosen_action.player_id == -1 && result.best_move.player_id != -1)
            {
                chosen_action = result.best_move;
            }
            else if (result.best_move.player_id != -1)
            { // If subsequent depth gave a valid move
                chosen_action = result.best_move;
            }
        }
        if (chosen_action.player_id == -1)
        { // Fallback if Minimax failed (e.g. timed out on depth 1)
            // Use simple greedy heuristic from original bot as fallback
            std::cerr << "Minimax fallback needed!" << std::endl;
            // This requires re-implementing the old greedy `decide_move` or a simplified version
            std::vector<MoveAction> possible_fallback_moves = generate_possible_moves_for_player(current_game_state, 0);
            if (!possible_fallback_moves.empty())
            {
                chosen_action = possible_fallback_moves[0]; // Simplest fallback: take first valid move
                // A better fallback would score these greedily.
            }
            else
            { // Totally trapped
                chosen_action.next_x = current_game_state.players[0].x;
                chosen_action.next_y = current_game_state.players[0].y;
                chosen_action.player_id = 0;
            }
        }
    }

    // Update GameState for actual items based on chosen_action (for player 0)
    // This logic should mirror parts of apply_single_player_move_action but for the *actual* item state
    if (!current_game_state.players.empty() && !current_game_state.players[0].eliminated)
    {
        bool actual_can_pickup = !(current_game_state.my_actual_speed_boost_turns_left > 0 || current_game_state.my_actual_has_oil_slick);
        if (actual_can_pickup)
        {
            auto it = current_game_state.items_on_map_sim.begin(); // Check against current map items
            while (it != current_game_state.items_on_map_sim.end())
            {
                if (it->r == chosen_action.next_x && it->c == chosen_action.next_y)
                {
                    if (it->type == 'G')
                        current_game_state.my_actual_speed_boost_turns_left = 5;
                    else if (it->type == 'E')
                        current_game_state.my_actual_paint_bomb_just_picked_up = true; // For judge, not for sim eval
                    else if (it->type == 'F')
                    {
                        current_game_state.my_actual_has_oil_slick = true;
                        current_game_state.my_actual_oil_slick_turns_to_expire = 5;
                    }
                    // Note: items_on_map_sim is not modified here, judge handles map update
                    break;
                }
                else
                {
                    ++it;
                }
            }
        }
        // Decrement actual item durations
        if (chosen_action.oil_used)
        {
            current_game_state.my_actual_has_oil_slick = false;
            current_game_state.my_actual_oil_slick_turns_to_expire = 0;
        }
        else if (current_game_state.my_actual_has_oil_slick && current_game_state.my_actual_oil_slick_turns_to_expire > 0)
        {
            current_game_state.my_actual_oil_slick_turns_to_expire--;
            if (current_game_state.my_actual_oil_slick_turns_to_expire == 0)
                current_game_state.my_actual_has_oil_slick = false;
        }
        if (current_game_state.my_actual_speed_boost_turns_left > 0)
        {
            current_game_state.my_actual_speed_boost_turns_left--;
        }
    }

    current_game_state.save_my_actual_item_state();

    std::ofstream move_out_file("MOVE.OUT");
    move_out_file << chosen_action.next_x << " " << chosen_action.next_y << std::endl;
    move_out_file.close();

    // std::cerr << "Minimax chose: " << chosen_action.next_x << " " << chosen_action.next_y
    //           << " with score " << chosen_action.score << std::endl; // chosen_action doesn't have score directly from MinimaxResult struct

    return 0;
}