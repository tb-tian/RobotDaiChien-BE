#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>
#include <memory> // For std::shared_ptr, std::unique_ptr
#include <chrono>
#include <limits>
#include <random> // For random playouts
#include <cmath>  // For sqrt, log in UCT
#include <queue>
#include <set>

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

// --- Constants & Basic Structs (Player, ItemOnMap, Direction, MoveAction) ---
// ... (Assume these are defined as in previous C++ examples) ...
// Player struct needs id, x, y, color_char, eliminated, item simulation states
// MoveAction needs player_id, next_x, next_y, oil_used, steps

// Forward declarations
class GameState;
class MCTSNode; // New class for MCTS nodes

std::vector<MoveAction> generate_possible_moves_for_player_mcts(const GameState &gs, int player_id_to_move);
// Playout policy: How to make moves during simulation (can be random or slightly heuristic)
MoveAction default_policy_get_move(const GameState &gs, int player_id_to_move, std::mt19937 &rng);
// Simulate a full turn from a state where each player makes one move using default policy
// Returns the scores for all players after this simulated turn.
void simulate_one_full_turn_using_default_policy(GameState &current_state, int sim_turn_number, std::mt19937 &rng);
// The main game simulation logic for end-of-turn effects (coloring, enclosure, shrink)
void simulate_full_end_of_turn_mcts(GameState &gs, int turn_being_concluded);
void apply_paint_bomb_effect(GameState &gs, int center_r, int center_c, char player_color);

class GameState
{ // Mostly same as MaxN version, but needs careful copying
public:
    int M, N, K_shrink_period, game_turn_number;
    std::vector<Player> players;
    std::vector<std::vector<char>> grid;
    std::vector<ItemOnMap> items_on_map;

    // Actual item states for our bot (players[0])
    int my_actual_speed_boost_turns_left;
    int my_actual_oil_slick_turns_to_expire;
    bool my_actual_has_oil_slick;

    GameState() : M(0), N(0), K_shrink_period(0), game_turn_number(0),
                  my_actual_speed_boost_turns_left(0),
                  my_actual_oil_slick_turns_to_expire(0),
                  my_actual_has_oil_slick(false) {}
    GameState(const GameState &other) = default; // Ensure deep copies are handled

    // ... (Methods: parse_input, save/load_my_actual_item_state, sync_my_actual_items_to_player0_sim,
    // is_within_bounds, cell_will_be_sealed_at_end_of_turn, is_target_cell_valid_for_landing,
    // apply_chosen_move_for_player_in_sim (for applying ONE player's move to THIS state)) ...
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

    // Helper to check if game is over in simulation
    bool is_terminal_mcts() const
    {
        int active_players = 0;
        for (const auto &p : players)
        {
            if (!p.eliminated)
                active_players++;
        }
        return active_players <= 1; // Game ends if 1 or 0 players left
    }

    // Get scores for all players in this terminal state (or based on a heuristic if not truly terminal but at sim depth limit)
    std::vector<double> get_terminal_scores_mcts() const
    {
        std::vector<double> scores(players.size(), 0.0);
        // This should use a static evaluation like static_evaluate_state_all_players
        // For simplicity in the MCTS structure, let's assume it's a simplified score based on tiles.
        for (size_t i = 0; i < players.size(); ++i)
        {
            if (players[i].eliminated)
            {
                scores[i] = -1e7; // Very bad
                continue;
            }
            int tiles = 0;
            for (int r = 0; r < M; ++r)
            {
                for (int c = 0; c < N; ++c)
                {
                    if (grid[r][c] == players[i].color_char)
                        tiles++;
                }
            }
            scores[i] = static_cast<double>(tiles); // Example: score is tile count
        }
        return scores;
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

// --- MCTS Node ---
class MCTSNode : public std::enable_shared_from_this<MCTSNode>
{
public:
    std::shared_ptr<MCTSNode> parent;
    std::vector<std::shared_ptr<MCTSNode>> children;

    GameState state;                                      // The game state this node represents
    MoveAction move_that_led_to_this_node;                // Action taken by parent to reach this state
    int player_id_whose_turn_it_is_to_act_from_this_node; // Whose turn is it AFTER move_that_led_to_this_node

    int visits;
    // For multi-player MCTS, Q needs to store scores for all players, or we use player-specific Q values.
    // Let's store cumulative scores for the player *whose turn it was that led to this node being expanded*.
    // Or, more general for N-player, store sum of scores for player P achieved in playouts starting from this node.
    std::vector<double> sum_scores_from_playouts; // Index by player_id

    std::vector<MoveAction> untried_moves_for_current_player; // Moves not yet expanded from this node

    MCTSNode(const GameState &s, std::shared_ptr<MCTSNode> p = nullptr, MoveAction move = MoveAction(), int player_to_act_next = 0) : parent(p), state(s), move_that_led_to_this_node(move),
                                                                                                                                      player_id_whose_turn_it_is_to_act_from_this_node(player_to_act_next),
                                                                                                                                      visits(0)
    {
        sum_scores_from_playouts.assign(s.players.size(), 0.0); // Initialize scores
        if (!state.is_terminal_mcts())
        {
            // This logic is tricky. Untried moves are for the player whose turn it *will be*
            // at this state node to make a decision.
            // If player_id_whose_turn_it_is_to_act_from_this_node is set correctly upon creation:
            if (player_id_whose_turn_it_is_to_act_from_this_node < state.players.size() &&
                !state.players[player_id_whose_turn_it_is_to_act_from_this_node].eliminated)
            {
                untried_moves_for_current_player = generate_possible_moves_for_player_mcts(state, player_id_whose_turn_it_is_to_act_from_this_node);
            }
        }
    }

    bool is_fully_expanded() const
    {
        return untried_moves_for_current_player.empty();
    }

    bool is_terminal() const
    {
        return state.is_terminal_mcts();
    }

    // UCT selection policy
    std::shared_ptr<MCTSNode> select_child_uct(double exploration_constant = 1.414) const
    {
        std::shared_ptr<MCTSNode> best_child = nullptr;
        double best_uct_value = -std::numeric_limits<double>::infinity();

        for (const auto &child : children)
        {
            if (child->visits == 0)
            { // Prefer unvisited children first for initial exploration
                return child;
            }
            // UCT: Q_i / N_i + C * sqrt(log(N_p) / N_i)
            // Q_i is the average score for the player whose turn it is at the child node.
            // This needs care: Q_i should be from the perspective of the player making the decision AT THIS PARENT NODE.
            // If parent is player P, and child represents state after P's move, then Q_i for child should reflect P's score.

            double q_avg_for_child_perspective = 0;
            // The score to maximize is for the player who *made the move* leading to the child (i.e., parent's player_id)
            // OR the player whose turn it is *at the parent node*.
            int parent_player_id = this->player_id_whose_turn_it_is_to_act_from_this_node;
            if (parent_player_id < child->sum_scores_from_playouts.size())
            {
                q_avg_for_child_perspective = child->sum_scores_from_playouts[parent_player_id] / static_cast<double>(child->visits);
            }

            double uct_value = q_avg_for_child_perspective +
                               exploration_constant * std::sqrt(std::log(static_cast<double>(this->visits)) / static_cast<double>(child->visits));

            if (uct_value > best_uct_value)
            {
                best_uct_value = uct_value;
                best_child = child;
            }
        }
        return best_child;
    }

    std::shared_ptr<MCTSNode> expand()
    {
        if (untried_moves_for_current_player.empty())
        {
            return nullptr; // Should not happen if not fully expanded
        }
        MoveAction move_to_expand = untried_moves_for_current_player.back();
        untried_moves_for_current_player.pop_back();

        GameState next_sim_state = this->state; // Copy current state
        // Apply this single move to next_sim_state
        // This is tricky: expand happens for one player's move.
        // The full turn simulation happens *during the playout* or if MCTS nodes represent full turns.
        // Let's assume MCTS nodes represent states where it's ONE player's turn to act.
        // After this player acts, it becomes the next player's turn in the *same game turn*.

        next_sim_state.apply_chosen_move_for_player_in_sim(move_to_expand);

        int next_player_to_act = (this->player_id_whose_turn_it_is_to_act_from_this_node + 1) % next_sim_state.players.size();
        // Skip eliminated players for next turn
        while (next_player_to_act < next_sim_state.players.size() && next_sim_state.players[next_player_to_act].eliminated)
        {
            next_player_to_act = (next_player_to_act + 1) % next_sim_state.players.size();
            // Infinite loop possible if all are eliminated - is_terminal should catch this.
        }

        // If player_id_whose_turn... was the last player for the turn, then next player is 0 and game turn advances.
        // This state transition logic for player turns within a game turn is critical.
        // For now, assume simple cycling. This is a BIG simplification.
        // A better MCTS would have nodes represent "state + whose turn it is to complete the current game turn" OR
        // nodes represent states *after* a full game turn. The latter is simpler to manage for playouts.

        // Let's try nodes represent states *before* a full game turn simulation.
        // Expansion adds a child representing the state *after* the current player makes ONE move.
        // The playout then simulates the rest of that turn + subsequent turns.

        std::shared_ptr<MCTSNode> new_child = std::make_shared<MCTSNode>(next_sim_state,
                                                                         this->shared_from_this(), // Need to enable_shared_from_this
                                                                         move_to_expand,
                                                                         next_player_to_act);
        children.push_back(new_child);
        return new_child;
    }

    // Backpropagate results from a playout
    void backpropagate(const std::vector<double> &playout_scores)
    {
        MCTSNode *current_node = this;
        while (current_node != nullptr)
        {
            current_node->visits++;
            for (size_t i = 0; i < playout_scores.size() && i < current_node->sum_scores_from_playouts.size(); ++i)
            {
                current_node->sum_scores_from_playouts[i] += playout_scores[i];
            }
            current_node = current_node->parent.get();
        }
    }
};

// Enable shared_from_this for MCTSNode for parent pointer in expand()
// class MCTSNode : public std::enable_shared_from_this<MCTSNode>
// { /* ... rest of MCTSNode ... */
// };

// --- MCTS Main Algorithm ---
MoveAction mcts_get_best_move(const GameState &root_state, int iterations_limit, int our_bot_id, std::mt19937 &rng)
{
    // The root node represents the state where it's OUR bot's turn to make the first move of the current game turn.
    // The MCTS process will explore sequences of moves for ALL players for several game turns deep.

    // Determine whose turn it is at the root_state (should be our_bot_id for the first action of the turn)
    int root_player_to_act = our_bot_id;
    // A better way: root_state has a "current_player_to_act_in_this_turn" field.
    // For now, assume MCTS is called when it's our bot's turn to decide its action for the current game turn.

    auto root_node = std::make_shared<MCTSNode>(root_state, nullptr, MoveAction(), root_player_to_act);

    for (int i = 0; i < iterations_limit; ++i)
    {
        if (is_time_up_global())
            break; // Check global time limit

        // 1. Selection
        std::shared_ptr<MCTSNode> promising_node = root_node;
        std::vector<std::shared_ptr<MCTSNode>> path_to_promising_node;
        path_to_promising_node.push_back(promising_node);

        while (!promising_node->is_terminal() && promising_node->is_fully_expanded())
        {
            promising_node = promising_node->select_child_uct();
            if (!promising_node)
                break; // Should not happen if select_child_UCT is robust
            path_to_promising_node.push_back(promising_node);
        }
        if (!promising_node)
            continue; // Safety for UCT returning null

        // 2. Expansion
        std::shared_ptr<MCTSNode> node_to_simulate_from = promising_node;
        if (!promising_node->is_terminal() && !promising_node->is_fully_expanded())
        {
            node_to_simulate_from = promising_node->expand();
            if (node_to_simulate_from)
            {                                                            // If expansion was successful
                path_to_promising_node.push_back(node_to_simulate_from); // Add expanded node to path
            }
            else
            {                                           // Expansion failed (e.g. no untried moves left but not marked fully expanded - logic error)
                node_to_simulate_from = promising_node; // Simulate from parent if expansion fails
            }
        }
        if (!node_to_simulate_from)
            node_to_simulate_from = promising_node; // Ultimate fallback

        // 3. Simulation (Playout)
        GameState sim_state = node_to_simulate_from->state; // Start simulation from this state
        std::vector<double> playout_scores;
        int sim_depth_limit = 10;                               // Max number of full game turns to simulate in playout
        int current_sim_game_turn = sim_state.game_turn_number; // The turn number IN THE SIMULATION

        // The node_to_simulate_from->state is after ONE player (who expanded it) has moved.
        // The playout needs to simulate the rest of THAT turn, then subsequent turns.
        int player_to_act_in_sim_turn = node_to_simulate_from->player_id_whose_turn_it_is_to_act_from_this_node;

        for (int d = 0; d < sim_depth_limit && !sim_state.is_terminal_mcts(); ++d)
        {
            // Simulate one full turn for all players using default policy
            std::vector<MoveAction> turn_actions_for_sim;
            GameState temp_state_for_turn_actions = sim_state; // Actions based on state at start of this sim turn

            for (size_t p_idx_sim = 0; p_idx_sim < temp_state_for_turn_actions.players.size(); ++p_idx_sim)
            {
                int actual_player_id_to_move = (player_to_act_in_sim_turn + p_idx_sim) % temp_state_for_turn_actions.players.size();
                while (actual_player_id_to_move < temp_state_for_turn_actions.players.size() && temp_state_for_turn_actions.players[actual_player_id_to_move].eliminated)
                {
                    actual_player_id_to_move = (actual_player_id_to_move + 1) % temp_state_for_turn_actions.players.size();
                    // Add check to break if all remaining are eliminated
                }
                if (actual_player_id_to_move >= temp_state_for_turn_actions.players.size())
                    break; // all further are eliminated or error

                if (!temp_state_for_turn_actions.players[actual_player_id_to_move].eliminated)
                {
                    MoveAction sim_move = default_policy_get_move(temp_state_for_turn_actions, actual_player_id_to_move, rng);
                    if (sim_move.is_valid())
                    {
                        turn_actions_for_sim.push_back(sim_move);
                        temp_state_for_turn_actions.apply_chosen_move_for_player_in_sim(sim_move); // Apply to temp state to inform next player's default policy choice
                    }
                    else
                    { // No valid move for player
                        turn_actions_for_sim.push_back(MoveAction(actual_player_id_to_move, temp_state_for_turn_actions.players[actual_player_id_to_move].x, temp_state_for_turn_actions.players[actual_player_id_to_move].y));
                    }
                }
                else
                {
                    turn_actions_for_sim.push_back(MoveAction(actual_player_id_to_move, -1, -1)); // Placeholder
                }
            }
            // Apply collected moves to sim_state and simulate end of turn
            for (const auto &action : turn_actions_for_sim)
            {
                if (action.is_valid())
                    sim_state.apply_chosen_move_for_player_in_sim(action);
            }
            simulate_full_end_of_turn_mcts(sim_state, current_sim_game_turn);
            current_sim_game_turn++;
            player_to_act_in_sim_turn = 0; // Next sim turn starts with player 0 (or first active)
        }
        playout_scores = sim_state.get_terminal_scores_mcts(); // Get scores from final sim state

        // 4. Backpropagation
        // Backpropagate from the node where the simulation started, which is node_to_simulate_from
        // The path used for selection might be different if expansion happened.
        // The update should go up from node_to_simulate_from.
        node_to_simulate_from->backpropagate(playout_scores);
    }

    // Select best move from root's children
    std::shared_ptr<MCTSNode> best_root_child = nullptr;
    int max_visits = -1;
    // Or, choose child that maximizes score for our_bot_id
    double best_avg_score_for_bot = -std::numeric_limits<double>::infinity();

    if (root_node->children.empty())
    { // No moves explored, or only root exists
        // Fallback: generate moves for root and pick one (e.g. random or simple heuristic)
        std::vector<MoveAction> fallback_moves = generate_possible_moves_for_player_mcts(root_state, our_bot_id);
        if (!fallback_moves.empty())
            return fallback_moves[0];                                                                      // Simplest fallback
        return MoveAction(our_bot_id, root_state.players[our_bot_id].x, root_state.players[our_bot_id].y); // Stay
    }

    for (const auto &child : root_node->children)
    {
        if (child->visits > 0)
        { // Ensure child was visited
            double avg_score_for_bot_this_child = 0;
            if (our_bot_id < child->sum_scores_from_playouts.size())
            {
                avg_score_for_bot_this_child = child->sum_scores_from_playouts[our_bot_id] / static_cast<double>(child->visits);
            }

            if (avg_score_for_bot_this_child > best_avg_score_for_bot)
            {
                best_avg_score_for_bot = avg_score_for_bot_this_child;
                best_root_child = child;
            }
            // Or use max visits:
            // if (child->visits > max_visits) {
            //    max_visits = child->visits;
            //    best_root_child = child;
            // }
        }
    }

    if (best_root_child)
    {
        return best_root_child->move_that_led_to_this_node;
    }
    else
    { // Fallback if no children were properly evaluated (e.g. all had 0 visits)
        std::vector<MoveAction> fallback_moves = generate_possible_moves_for_player_mcts(root_state, our_bot_id);
        if (!fallback_moves.empty())
            return fallback_moves[0];
        return MoveAction(our_bot_id, root_state.players[our_bot_id].x, root_state.players[our_bot_id].y); // Stay
    }
}

// --- Playout Policy (Default Policy) ---
MoveAction default_policy_get_move(const GameState &gs, int player_id_to_move, std::mt19937 &rng)
{
    std::vector<MoveAction> possible_moves = generate_possible_moves_for_player_mcts(gs, player_id_to_move);
    if (possible_moves.empty())
    {
        // Return a "stay" move or invalid move if trapped
        return MoveAction(player_id_to_move,
                          gs.players[player_id_to_move].x,
                          gs.players[player_id_to_move].y, false, 0);
    }
    // For true random:
    std::uniform_int_distribution<> dist(0, possible_moves.size() - 1);
    return possible_moves[dist(rng)];

    // For a slightly more heuristic playout (e.g. prefer coloring empty cells):
    // You could score moves lightly here. For now, random.
}

// --- GameState methods and other functions (parse_input, save_state, static_evaluate, etc.) ---
// ... (These would be similar to the MaxN version, but ensure GameState copy is efficient) ...
// ... (generate_possible_moves_for_player_mcts, simulate_full_end_of_turn_mcts are crucial) ...
// ... (apply_chosen_move_for_player_in_sim, color_tiles, enclosures, shrink_map, items) ...
// Need to ensure these are available and correctly implemented.
// The ones from previous "full MaxN" can be adapted.
std::vector<MoveAction> generate_possible_moves_for_player_mcts(const GameState &gs, int player_id_to_move)
{
    std::vector<MoveAction> moves;
    // Using a set to store tuples representing moves to avoid duplicates if different paths lead to same logical move
    // std::set<std::tuple<int, int, bool, int>> unique_move_tuples; // {next_x, next_y, oil_used, steps}

    if (player_id_to_move < 0 || player_id_to_move >= gs.players.size() || gs.players[player_id_to_move].eliminated)
    {
        return moves; // No moves if player invalid or eliminated
    }

    const Player &p = gs.players[player_id_to_move];
    int current_x = p.x;
    int current_y = p.y;

    // --- 1-step moves (including stay) ---
    for (const auto &dir : DIRECTIONS)
    {
        int next_x = current_x + dir.dr;
        int next_y = current_y + dir.dc;
        int steps = (dir.dr == 0 && dir.dc == 0) ? 0 : 1;

        if (gs.is_target_cell_valid_for_landing(next_x, next_y, gs.game_turn_number))
        {
            // auto move_tuple = std::make_tuple(next_x, next_y, false, steps);
            // if (unique_move_tuples.find(move_tuple) == unique_move_tuples.end()) {
            moves.emplace_back(player_id_to_move, next_x, next_y, false, steps);
            //    unique_move_tuples.insert(move_tuple);
            // }
        }
    }

    // --- 2-step moves (Speed Boost) ---
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
            bool inter_passable_no_oil = gs.is_within_bounds(inter_x, inter_y) &&
                                         gs.grid[inter_x][inter_y] != OBSTACLE_CELL &&
                                         !(gs.grid[inter_x][inter_y] >= 'a' && gs.grid[inter_x][inter_y] <= 'd') &&     // Not sealed
                                         !gs.cell_will_be_sealed_at_end_of_turn(inter_x, inter_y, gs.game_turn_number); // Not becoming sealed

            if (inter_passable_no_oil && gs.is_target_cell_valid_for_landing(final_x, final_y, gs.game_turn_number))
            {
                // auto move_tuple = std::make_tuple(final_x, final_y, false, 2);
                // if (unique_move_tuples.find(move_tuple) == unique_move_tuples.end()) {
                moves.emplace_back(player_id_to_move, final_x, final_y, false, 2);
                //    unique_move_tuples.insert(move_tuple);
                // }
            }
            // Path 2: Using Oil Slick for the intermediate step
            else if (p.has_oil_slick_sim && p.oil_slick_turns_to_expire_sim > 0)
            {
                // Intermediate cell IS an obstacle/sealed, but can pass with oil
                bool inter_is_obstacle_type = gs.is_within_bounds(inter_x, inter_y) &&
                                              (gs.grid[inter_x][inter_y] == OBSTACLE_CELL || (gs.grid[inter_x][inter_y] >= 'a' && gs.grid[inter_x][inter_y] <= 'd'));

                bool inter_safe_to_pass_through = inter_is_obstacle_type &&
                                                  gs.is_within_bounds(inter_x, inter_y) &&                                       // Redundant but safe
                                                  !gs.cell_will_be_sealed_at_end_of_turn(inter_x, inter_y, gs.game_turn_number); // Crucial: cannot pass through a cell that will disappear

                if (inter_safe_to_pass_through && gs.is_target_cell_valid_for_landing(final_x, final_y, gs.game_turn_number))
                {
                    // auto move_tuple = std::make_tuple(final_x, final_y, true, 2);
                    // if (unique_move_tuples.find(move_tuple) == unique_move_tuples.end()) {
                    moves.emplace_back(player_id_to_move, final_x, final_y, true, 2);
                    //    unique_move_tuples.insert(move_tuple);
                    // }
                }
            }
        }
    }

    // If no moves generated (player is trapped), they must "stay"
    // The MCTS expansion logic or default policy should handle this by effectively not changing state if move list is empty.
    // Or, we can explicitly add a stay move if the current cell is valid to stay on.
    if (moves.empty())
    {
        if (gs.is_target_cell_valid_for_landing(current_x, current_y, gs.game_turn_number))
        {
            moves.emplace_back(player_id_to_move, current_x, current_y, false, 0); // Stay action
        }
        else
        {
            // Player is trapped on a cell that will also eliminate them or is invalid.
            // Still, the "action" is to attempt to stay. The game rules/simulation will handle outcome.
            // MCTS will likely heavily penalize such a state.
            // For move generation, we still offer the "stay" conceptually.
            moves.emplace_back(player_id_to_move, current_x, current_y, false, 0);
        }
    }
    return moves;
}

void simulate_one_full_turn_using_default_policy(
    GameState &current_sim_state,
    int sim_turn_number_to_conclude,
    std::mt19937 &rng)
{

    // Store chosen actions for all players for this turn
    std::vector<MoveAction> actions_this_turn(current_sim_state.players.size(), MoveAction(-1, -1, -1)); // Init with invalid

    // --- Phase 1: Each active player chooses and conceptually makes their move ---
    // Create a temporary state to base decisions on, as moves are "simultaneous" conceptually.
    // However, the default policy might need to see the effect of previous players in the *same turn*
    // if it's a complex heuristic. For a simple random/light heuristic, basing on the start-of-turn state is often fine.
    // For this implementation, let's apply moves sequentially to 'current_sim_state' for the default policy decisions.

    for (size_t i = 0; i < current_sim_state.players.size(); ++i)
    {
        // In a real game, player order might matter or be fixed. Here, iterate by ID.
        int player_id = current_sim_state.players[i].id; // Assuming player_id matches index i for simplicity
                                                         // or iterate `for(Player& p : current_sim_state.players)`
        if (!current_sim_state.players[player_id].eliminated)
        {
            MoveAction chosen_move = default_policy_get_move(current_sim_state, player_id, rng);

            if (chosen_move.is_valid())
            {
                actions_this_turn[player_id] = chosen_move; // Store the chosen move
                // Apply this player's move to 'current_sim_state' so subsequent default policy calls
                // for other players in this same turn see its effects (e.g., item picked up).
                current_sim_state.apply_chosen_move_for_player_in_sim(chosen_move);
            }
            else
            {                                                                                                                                                   // Default policy returned no valid move (should be rare if "stay" is always an option)
                actions_this_turn[player_id] = MoveAction(player_id, current_sim_state.players[player_id].x, current_sim_state.players[player_id].y, false, 0); // Stay
                // No need to call apply_chosen_move if they are already staying.
            }
        }
        else
        {
            actions_this_turn[player_id] = MoveAction(player_id, -1, -1); // Eliminated, no action
        }
    }

    // --- Phase 2: Apply all global end-of-turn effects ---
    // The 'current_sim_state' now reflects all players having moved and picked up items.
    simulate_full_end_of_turn_mcts(current_sim_state, sim_turn_number_to_conclude);

    // The game turn number in the state should advance after end-of-turn effects.
    current_sim_state.game_turn_number++; // Increment for the *next* turn being simulated
}

void color_tiles_from_player_positions_mcts(GameState &gs)
{
    std::map<std::pair<int, int>, int> players_on_cell_count;
    std::map<std::pair<int, int>, char> first_player_color_on_cell; // If multiple, for reference

    for (const auto &p : gs.players)
    {
        if (!p.eliminated && gs.is_within_bounds(p.x, p.y))
        {
            players_on_cell_count[{p.x, p.y}]++;
            if (players_on_cell_count[{p.x, p.y}] == 1)
            {
                first_player_color_on_cell[{p.x, p.y}] = p.color_char;
            }
        }
    }

    for (const auto &p_entry : players_on_cell_count)
    {
        std::pair<int, int> cell_coords = p_entry.first;
        int count = p_entry.second;
        int r = cell_coords.first;
        int c = cell_coords.second;

        if (count == 1)
        { // Only one player on the cell
            char player_color_to_paint = first_player_color_on_cell[cell_coords];
            char current_cell_char = gs.grid[r][c];
            if (current_cell_char != OBSTACLE_CELL && !(current_cell_char >= 'a' && current_cell_char <= 'd'))
            {
                gs.grid[r][c] = player_color_to_paint;
            }
        }
        // If count > 1, "màu của ô đó giữ nguyên như trước không thay đổi"
    }
}

// --- Helper: Enclosure Detection (BFS-based) ---
struct BFS_Cell_State
{
    int r, c;
};

void detect_and_fill_enclosures_mcts(GameState &gs, int turn_being_concluded)
{
    std::vector<std::vector<bool>> globally_checked_empty_cells(gs.M, std::vector<bool>(gs.N, false));

    // Determine current playable border (after previous shrinks)
    int current_shrink_depth = 0; // How many layers are *already* effectively obstacles
    if (gs.K_shrink_period > 0 && turn_being_concluded > gs.K_shrink_period)
    {                                                                           // check if any shrink could have happened
        current_shrink_depth = (turn_being_concluded - 1) / gs.K_shrink_period; // layers sealed *before* this turn's potential shrink
    }
    else if (gs.K_shrink_period > 0 && turn_being_concluded == gs.K_shrink_period)
    {
        current_shrink_depth = 0; // First shrink about to happen, current border is absolute map edge
    }

    for (int r_start = 0; r_start < gs.M; ++r_start)
    {
        for (int c_start = 0; c_start < gs.N; ++c_start)
        {
            if (globally_checked_empty_cells[r_start][c_start] || gs.grid[r_start][c_start] != EMPTY_CELL)
            {
                continue; // Already processed or not an empty cell to start BFS from
            }

            std::queue<BFS_Cell_State> q;
            q.push({r_start, c_start});

            std::vector<BFS_Cell_State> component_cells; // Cells in this potential enclosure
            std::set<char> bordering_player_colors_found;
            bool touches_open_border = false;
            std::vector<std::vector<bool>> visited_this_bfs(gs.M, std::vector<bool>(gs.N, false));

            visited_this_bfs[r_start][c_start] = true;
            component_cells.push_back({r_start, c_start});

            while (!q.empty())
            {
                BFS_Cell_State curr = q.front();
                q.pop();
                globally_checked_empty_cells[curr.r][curr.c] = true; // Mark as processed globally

                // Check if this cell is on the "true" open border of the playable area
                int cell_layer = std::min({curr.r, gs.M - 1 - curr.r, curr.c, gs.N - 1 - curr.c});
                if (cell_layer == current_shrink_depth)
                { // It's on the outermost non-shrunk layer
                    touches_open_border = true;
                }
                // If already touched border, can potentially stop early for this component's border check
                // but continue BFS to mark all cells of this component in globally_checked_empty_cells.

                for (const auto &dir : DIRECTIONS)
                { // 4 cardinal directions
                    if (dir.dr == 0 && dir.dc == 0)
                        continue;
                    int nr = curr.r + dir.dr;
                    int nc = curr.c + dir.dc;

                    if (!gs.is_within_bounds(nr, nc))
                    {
                        // Touched absolute map edge. If this edge is not yet shrunk past, it's an open border.
                        if (cell_layer <= current_shrink_depth)
                        { // current cell is on a layer that is or will be shrunk
                          // This logic is tricky. If out of bounds, it's effectively open unless that boundary is already "sealed" by definition.
                        }
                        // For simplicity: if it's out of bounds, it means the component could escape if that edge is not shrunk.
                        // This is covered by the cell_layer check above. If curr.r is 0 and layer 0 is not shrunk, touches_open_border.
                        continue;
                    }

                    if (visited_this_bfs[nr][nc])
                        continue;

                    char neighbor_char = gs.grid[nr][nc];
                    if (neighbor_char == EMPTY_CELL)
                    {
                        visited_this_bfs[nr][nc] = true;
                        q.push({nr, nc});
                        component_cells.push_back({nr, nc});
                    }
                    else if (neighbor_char >= 'A' && neighbor_char <= 'D')
                    { // Player color
                        bordering_player_colors_found.insert(neighbor_char);
                    }
                    else if (neighbor_char == OBSTACLE_CELL || (neighbor_char >= 'a' && neighbor_char <= 'd'))
                    {
                        // Obstacle or already sealed cell, forms part of the boundary
                    }
                }
            } // End BFS for one component of empty cells

            if (!touches_open_border && bordering_player_colors_found.size() == 1)
            {
                char enclosing_color = *bordering_player_colors_found.begin();
                // Fill component_cells with enclosing_color
                for (const auto &cell_to_fill : component_cells)
                {
                    gs.grid[cell_to_fill.r][cell_to_fill.c] = enclosing_color;
                }
                // Eliminate players of other colors inside this newly filled region
                for (auto &p_check : gs.players)
                {
                    if (!p_check.eliminated && p_check.color_char != enclosing_color)
                    {
                        for (const auto &filled_cell : component_cells)
                        {
                            if (p_check.x == filled_cell.r && p_check.y == filled_cell.c)
                            {
                                p_check.eliminated = true;
                                // std::cerr << "Player " << p_check.color_char << " eliminated by enclosure of " << enclosing_color << std::endl;
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

// --- Helper: shrink_map_and_eliminate_players ---
void shrink_map_and_eliminate_players_mcts(GameState &gs, int turn_being_concluded)
{
    if (turn_being_concluded <= 0 || gs.K_shrink_period <= 0 || turn_being_concluded % gs.K_shrink_period != 0)
    {
        return; // No shrink this turn
    }
    int shrink_iteration = turn_being_concluded / gs.K_shrink_period; // This is 'i' from "i * K"
    int layer_to_seal_idx = shrink_iteration - 1;                     // Layer index is i-1

    if (layer_to_seal_idx < 0)
        return; // Should not happen if turn_being_concluded is valid multiple

    for (int r = 0; r < gs.M; ++r)
    {
        for (int c = 0; c < gs.N; ++c)
        {
            // A cell is part of layer 'k' if min(r, M-1-r, c, N-1-c) == k
            int cell_layer = std::min({r, gs.M - 1 - r, c, gs.N - 1 - c});

            if (cell_layer == layer_to_seal_idx)
            { // This cell is on the exact layer being sealed now
                // Eliminate players on this cell
                for (auto &p : gs.players)
                {
                    if (!p.eliminated && p.x == r && p.y == c)
                    {
                        p.eliminated = true;
                        // std::cerr << "Player " << p.color_char << " eliminated by map shrink at (" << r << "," << c << ")" << std::endl;
                    }
                }
                // Update grid cell char:
                // "Các ô bị phong ấn sẽ tương tự như các ô cấm, ngoại trừ việc các ô này sẽ giữ nguyên màu của nó ngay trước khi bị phong ấn."
                // "Nếu ô (i, j) là ô trống sẽ trở thành ‘#’."
                // "Nếu ô (i, j) đã được tô màu C trước đó thì sẽ trở thành chữ cái thường c"
                if (gs.grid[r][c] == EMPTY_CELL)
                {
                    gs.grid[r][c] = OBSTACLE_CELL;
                }
                else if (gs.grid[r][c] >= 'A' && gs.grid[r][c] <= 'D')
                { // Player color
                    gs.grid[r][c] = tolower(gs.grid[r][c]);
                }
                // If already OBSTACLE_CELL or lowercase (sealed in previous shrink), it remains.
            }
        }
    }
}

// --- Helper: decrement_item_durations_and_despawn ---
void decrement_item_durations_and_despawn_mcts(GameState &gs, int turn_being_concluded)
{
    // Player item durations (speed boost, oil slick expiry if not used)
    for (auto &p : gs.players)
    {
        if (p.id < 0)
            continue; // Skip invalid player entries if any

        if (p.speed_boost_turns_left_sim > 0)
        {
            p.speed_boost_turns_left_sim--;
        }
        // Oil slick expiry: if player has it and turns_to_expire > 0, decrement.
        // If it becomes 0, has_oil_slick_sim becomes false.
        // This is usually handled when player considers using it or in apply_chosen_move.
        // For a general end-of-turn decrement for *held but unused* oil:
        if (p.has_oil_slick_sim && p.oil_slick_turns_to_expire_sim > 0)
        {
            // The rule "nếu sau 5 lượt mà người chơi không kích hoạt thì vật phẩm sẽ hết hiệu lực"
            // This implies the timer ticks from pickup. If activated, it's gone.
            // apply_chosen_move_for_player_in_sim handles if it's *used*.
            // Here, we handle expiry if *held and not used*.
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
                       [&](ItemOnMap &item) {       // Pass by reference to modify
                           item.turns_to_despawn--; // Assuming this is called once per game turn
                           return item.turns_to_despawn <= 0;
                       }),
        gs.items_on_map.end());

    // Item Spawning (Turn 10 for example)
    // This makes the simulation non-deterministic if items/locations are random.
    // For MCTS, usually you either don't simulate future random events deeply, or average over many possibilities.
    // Simplification: Skip new item spawning *within a single MCTS playout* to keep it deterministic.
    // The *actual* game state (root_node) will have new items from MAP.INP.
    if (gs.game_turn_number == 9 && turn_being_concluded == 10)
    { // If turn 10 is *just concluding*
      // This is where you'd add logic to spawn 2 of 3 item types randomly
      // on valid empty/non-player cells. This is complex.
      // For now, we acknowledge it but won't implement the random spawning in this sim.
      // std::cerr << "DEBUG: Turn 10 concluded, item spawn would occur here in a full game engine." << std::endl;
    }
}

// --- Main Simulation Function for End of Turn ---
void simulate_full_end_of_turn_mcts(GameState &gs, int turn_being_concluded)
{
    // Order based on problem description:
    // 1. Player moves and their immediate item effects (like paint bomb pickup)
    //    are assumed to have been applied to 'gs' by `apply_chosen_move_for_player_in_sim`
    //    for each player during the `simulate_one_full_turn_using_default_policy` or MCTS expansion.

    // 2. Standard tile coloring from player positions
    color_tiles_from_player_positions_mcts(gs);

    // 3. Enclosures: "Tại cuối mỗi lượt, nếu các ô cùng một màu COLOR tạo thành một vùng khép kín..."
    detect_and_fill_enclosures_mcts(gs, turn_being_concluded);

    // It's possible an enclosure fill changes the cell a player is on (e.g. was empty, now their color).
    // Re-evaluate tile coloring if needed, or ensure enclosure logic is comprehensive.
    // The rules are a bit ambiguous on the exact micro-sequencing of coloring vs. enclosure benefits.
    // Assuming enclosure fill takes precedence for the cells it fills.
    // Players on those newly filled cells might now "re-color" them if alone, but that seems redundant if already filled by enclosure.

    // 4. Map Shrinking: "Khi kết thúc lượt i * K..."
    shrink_map_and_eliminate_players_mcts(gs, turn_being_concluded);

    // 5. Item duration decrements and on-map item despawning / potential spawning
    decrement_item_durations_and_despawn_mcts(gs, turn_being_concluded);
}
// --- Main ---
int main()
{
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);
    std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count()); // Random number generator

    GameState root_game_state;
    root_game_state.parse_input("MAP.INP");

    MoveAction final_action_to_take;
    final_action_to_take.player_id = -1;

    if (root_game_state.players.empty() || (root_game_state.game_turn_number > 0 && root_game_state.players[0].eliminated))
    {
        final_action_to_take = MoveAction(0, 0, 0);
    }
    else if (root_game_state.game_turn_number == 0)
    {
        // Turn 0 placement (same heuristic as before)
        // ... (copy turn 0 placement logic from corrected MaxN main) ...
        std::vector<std::pair<int, int>> valid_spawn_points;
        for (int r = 0; r < root_game_state.M; ++r)
            for (int c = 0; c < root_game_state.N; ++c)
                if (root_game_state.grid[r][c] == EMPTY_CELL && !root_game_state.cell_will_be_sealed_at_end_of_turn(r, c, 0))
                    valid_spawn_points.push_back({r, c});
        if (!valid_spawn_points.empty())
        {
            int best_r = root_game_state.M / 2, best_c = root_game_state.N / 2;
            double min_d = 1e9;
            bool found = false;
            for (const auto &sp : valid_spawn_points)
            {
                double d = pow(sp.first - best_r, 2) + pow(sp.second - best_c, 2);
                if (!found || d < min_d)
                {
                    min_d = d;
                    final_action_to_take = MoveAction(0, sp.first, sp.second);
                    found = true;
                }
            }
        }
        else
            final_action_to_take = MoveAction(0, root_game_state.M / 2, root_game_state.N / 2);
    }
    else
    {
        search_start_time_global = std::chrono::steady_clock::now();

        // Ensure player 0's simulated item states are current before search
        root_game_state.sync_my_actual_items_to_player0_sim();

        // MCTS iterations: adjust based on time limit and complexity
        // A common approach is to run MCTS for nearly the full time_limit_seconds_global
        int mcts_iterations = 0;                      // Iterations counter
        const int MAX_MCTS_ITERATIONS_TARGET = 50000; // Example target, will be cut by time

        // MCTS call. The number of iterations will be controlled by time_limit_seconds_global inside mcts_get_best_move.
        // The iterations_limit passed here is more of a conceptual cap if time wasn't an issue.
        final_action_to_take = mcts_get_best_move(root_game_state, MAX_MCTS_ITERATIONS_TARGET, 0 /* our_bot_id */, rng);

        auto time_taken = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - search_start_time_global);
        std::cerr << "MCTS finished in " << time_taken.count() << " ms." << std::endl; // For debugging

        if (!final_action_to_take.is_valid())
        { // Fallback if MCTS returns invalid
            std::cerr << "MCTS FALLBACK! No valid move from search." << std::endl;
            std::vector<MoveAction> fallback_options = generate_possible_moves_for_player_mcts(root_game_state, 0);
            if (!fallback_options.empty())
            {
                final_action_to_take = fallback_options[0]; // Simplest
            }
            else
            {
                final_action_to_take = MoveAction(0, root_game_state.players[0].x, root_game_state.players[0].y);
            }
        }
    }

    // Update actual item state for player 0 based on final_action_to_take
    // ... (Same item update logic as in previous main functions) ...
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
    move_out_file << final_action_to_take.next_x << " " << final_action_to_take.next_y << std::endl;
    move_out_file.close();

    return 0;
}