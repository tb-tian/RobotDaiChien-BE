#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm> // For std::min, std::max, std::sort
#include <map>
#include <tuple>  // Potentially for items or move options if not using structs
#include <random> // For random choice if needed for tie-breaking
#include <limits> // For std::numeric_limits

// --- Constants ---
const char EMPTY_CELL = '.';
const char OBSTACLE_CELL = '#';
const char SPEED_BOOST_ITEM = 'G';
const char PAINT_BOMB_ITEM = 'E';
const char OIL_SLICK_ITEM = 'F';

// --- Helper Structs/Classes ---

// Directions (dr, dc)
struct Direction
{
    int dr, dc;
    // std::string name; // Optional, for debugging
};

const std::vector<Direction> DIRECTIONS = {
    {-1, 0}, {1, 0}, {0, -1}, {0, 1}, {0, 0} // UP, DOWN, LEFT, RIGHT, STAY
};

struct Player
{
    int x, y;
    char color_char;
    bool eliminated;

    Player(int _x = -1, int _y = -1, char _color = ' ') : x(_x), y(_y), color_char(_color)
    {
        eliminated = (x == -1 && y == -1);
    }
};

struct ItemOnMap
{
    int r, c;
    char type;
};

// Forward declaration
class GameState;
struct MoveOption;

double evaluate_move(GameState &gs, int next_x, int next_y, int current_x, int current_y, bool used_oil_slick_for_this_move);
MoveOption decide_move(GameState &gs);
std::pair<int, int> choose_initial_position(GameState &gs);

class GameState
{
public:
    int M, N, K_shrink_period, current_turn;
    Player my_player;
    std::vector<Player> other_players;
    std::vector<std::vector<char>> grid;
    std::vector<ItemOnMap> items_on_map; // Parsed from input

    // My active items state
    int speed_boost_turns_left;
    bool paint_bomb_just_picked_up; // True if picked up this turn, for eval_move
    int oil_slick_turns_to_expire;  // If has_oil_slick, how many turns until it vanishes if not used
    bool has_oil_slick;             // True if player possesses an oil slick item

    GameState() : M(0), N(0), K_shrink_period(0), current_turn(0),
                  speed_boost_turns_left(0), paint_bomb_just_picked_up(false),
                  oil_slick_turns_to_expire(0), has_oil_slick(false) {}

    void parse_input(const std::string &filename = "MAP.INP")
    {
        std::ifstream ifs(filename);
        if (!ifs.is_open())
        {
            std::cerr << "Error: Could not open " << filename << std::endl;
            // In a real contest, might need to exit or throw
            return;
        }

        ifs >> M >> N >> K_shrink_period >> current_turn;

        int my_x, my_y;
        char my_color_char;
        ifs >> my_x >> my_y >> my_color_char;
        my_player = Player(my_x, my_y, my_color_char);

        int num_other_players;
        ifs >> num_other_players;
        other_players.clear(); // Clear before resizing/populating
        for (int i = 0; i < num_other_players; ++i)
        {
            int p_x, p_y;
            char p_color;
            ifs >> p_x >> p_y >> p_color;
            other_players.emplace_back(p_x, p_y, p_color);
        }

        grid.assign(M, std::vector<char>(N)); // Resize and initialize
        for (int i = 0; i < M; ++i)
        {
            for (int j = 0; j < N; ++j)
            {
                ifs >> grid[i][j];
            }
        }

        int num_map_items;
        ifs >> num_map_items;
        items_on_map.clear(); // Clear before populating
        for (int i = 0; i < num_map_items; ++i)
        {
            ItemOnMap item;
            ifs >> item.r >> item.c >> item.type;
            items_on_map.push_back(item);
        }
        ifs.close();
        load_my_item_state(); // Load persistent state after parsing current turn's map
    }

    void save_my_item_state(const std::string &filename = "STATE.DAT")
    {
        std::ofstream ofs(filename);
        if (!ofs.is_open())
        {
            std::cerr << "Warning: Could not write to " << filename << std::endl;
            return;
        }
        ofs << speed_boost_turns_left << std::endl;
        ofs << oil_slick_turns_to_expire << std::endl;
        ofs << (has_oil_slick ? 1 : 0) << std::endl;
        ofs.close();
    }

    void load_my_item_state(const std::string &filename = "STATE.DAT")
    {
        std::ifstream ifs(filename);
        if (!ifs.is_open())
        {
            // File not found (e.g., first turn) or unreadable
            speed_boost_turns_left = 0;
            oil_slick_turns_to_expire = 0;
            has_oil_slick = false;
            paint_bomb_just_picked_up = false; // Ensure reset
            return;
        }
        int has_oil_slick_int = 0; // Default to 0 if read fails
        ifs >> speed_boost_turns_left;
        ifs >> oil_slick_turns_to_expire;
        if (ifs >> has_oil_slick_int)
        { // Check if read was successful
            has_oil_slick = (has_oil_slick_int == 1);
        }
        else
        { // Read failed, reset
            has_oil_slick = false;
            oil_slick_turns_to_expire = 0; // Also reset related timer
        }
        paint_bomb_just_picked_up = false; // Reset this flag at the start of each turn's state load
        ifs.close();
    }

    bool is_within_bounds(int r, int c) const
    {
        return r >= 0 && r < M && c >= 0 && c < N;
    }

    // Checks if a cell (r, c) will be sealed at the END of the current turn
    bool cell_will_be_sealed_this_turn(int r, int c) const
    {
        if (current_turn > 0 && K_shrink_period > 0 && current_turn % K_shrink_period == 0)
        {
            int sealed_layer_index = (current_turn / K_shrink_period) - 1;
            // Ensure sealed_layer_index is not negative, though (current_turn / K_shrink_period) should be >= 1 here
            if (sealed_layer_index < 0)
                sealed_layer_index = 0;

            return (r == sealed_layer_index || r == M - 1 - sealed_layer_index ||
                    c == sealed_layer_index || c == N - 1 - sealed_layer_index);
        }
        return false;
    }

    bool is_valid_for_move(int r, int c, bool can_pass_one_obstacle = false) const
    {
        if (!is_within_bounds(r, c))
        {
            return false;
        }

        char cell_content = grid[r][c];
        bool is_obstacle_type = (cell_content == OBSTACLE_CELL || (cell_content >= 'a' && cell_content <= 'd'));

        if (is_obstacle_type)
        {
            return can_pass_one_obstacle; // Allow if oil slick is active for this specific step
        }

        // Crucial: Check if landing on this cell means elimination due to shrinking THIS turn
        if (cell_will_be_sealed_this_turn(r, c))
        {
            return false;
        }
        return true;
    }

    void check_and_handle_item_pickup(int next_x, int next_y)
    {
        // Rule: "Mỗi người chơi chỉ có thể nhặt 1 vật phẩm."
        // This means if we have speed boost active OR have an oil slick, we can't pick up another.
        bool can_pickup_new_item = !(speed_boost_turns_left > 0 || has_oil_slick);
        if (!can_pickup_new_item)
        {
            return;
        }

        char picked_up_item_char = 0; // Use 0 or a specific null char to indicate no item
        // Find item on map (the game engine removes it from map for next turn's MAP.INP, we just update our status)
        for (const auto &item : items_on_map)
        {
            if (item.r == next_x && item.c == next_y)
            {
                picked_up_item_char = item.type;
                break; // Found the item at the target location
            }
        }

        if (picked_up_item_char != 0)
        { // If an item was found at target and we can pick it up
            // std::cerr << "DEBUG: Picked up " << picked_up_item_char << " at (" << next_x << "," << next_y << ")" << std::endl;
            if (picked_up_item_char == SPEED_BOOST_ITEM)
            {
                speed_boost_turns_left = 5; // "tự động kích hoạt ... hiệu lực sau 5 lượt"
            }
            else if (picked_up_item_char == PAINT_BOMB_ITEM)
            {
                // "kích hoạt 1 lần duy nhất, ngay từ lúc người chơi nhặt vật phẩm."
                paint_bomb_just_picked_up = true; // Signal for evaluate_move for *this current turn's decision*
            }
            else if (picked_up_item_char == OIL_SLICK_ITEM)
            {
                has_oil_slick = true;
                oil_slick_turns_to_expire = 5; // "nếu sau 5 lượt ... không kích hoạt thì ... biến mất"
            }
        }
    }

    void decrement_item_durations(bool oil_slick_was_activated_this_turn_for_move)
    {
        if (speed_boost_turns_left > 0)
        {
            speed_boost_turns_left--;
        }

        if (has_oil_slick)
        { // If player possesses an oil slick item
            if (oil_slick_was_activated_this_turn_for_move)
            {
                has_oil_slick = false; // Item is consumed upon activation
                oil_slick_turns_to_expire = 0;
            }
            else if (oil_slick_turns_to_expire > 0)
            { // Not activated, but held
                oil_slick_turns_to_expire--;
                if (oil_slick_turns_to_expire == 0)
                {
                    has_oil_slick = false; // Expired by time if not used
                }
            }
            else
            { // oil_slick_turns_to_expire is 0 but has_oil_slick was somehow true (should be caught by above)
                has_oil_slick = false;
            }
        }
        // paint_bomb_just_picked_up is a flag for the current turn's evaluation.
        // It should be reset after evaluation or before the next turn's state load.
        // It's reset in load_my_item_state or can be reset here too.
        paint_bomb_just_picked_up = false;
    }
};

// --- Core Bot Logic (Functions) ---

std::pair<int, int> choose_initial_position(GameState &gs)
{
    // Generate random valid starting position
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist_x(0, gs.M - 1);
    std::uniform_int_distribution<int> dist_y(0, gs.N - 1);
    
    // Try to find a valid random position
    int max_attempts = 100; // Avoid infinite loop
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        int rand_x = dist_x(gen);
        int rand_y = dist_y(gen);
        
        // Check if this position is valid
        if (gs.grid[rand_x][rand_y] == EMPTY_CELL && 
            gs.is_valid_for_move(rand_x, rand_y, false)) {
            return {rand_x, rand_y};
        }
    }
    
    // Fallback: If no valid random position found after attempts, 
    // search systematically for any valid position
    for (int r = 0; r < gs.M; ++r) {
        for (int c = 0; c < gs.N; ++c) {
            if (gs.grid[r][c] == EMPTY_CELL && 
                gs.is_valid_for_move(r, c, false)) {
                return {r, c};
            }
        }
    }
    
    // Last resort: return center position
    return {gs.M / 2, gs.N / 2};
}

double evaluate_move(GameState &gs, int next_x, int next_y, int current_x, int current_y, bool used_oil_slick_for_this_move)
{
    double score = 0.0;
    char my_color = gs.my_player.color_char;

    // Basic validity: landing on obstacle/sealed or out of bounds is infinitely bad.
    // This should ideally be filtered by `decide_move` before calling `evaluate_move`.
    if (!gs.is_within_bounds(next_x, next_y))
        return -std::numeric_limits<double>::infinity();

    char target_cell_on_grid_char = gs.grid[next_x][next_y];
    if (target_cell_on_grid_char == OBSTACLE_CELL || (target_cell_on_grid_char >= 'a' && target_cell_on_grid_char <= 'd'))
    {
        // Cannot land on an obstacle or sealed cell, even if oil slick was used for the path
        return -std::numeric_limits<double>::infinity();
    }
    // Avoid moving to a cell that will get player eliminated by shrinking this turn
    if (gs.cell_will_be_sealed_this_turn(next_x, next_y))
    {
        return -std::numeric_limits<double>::infinity();
    }

    // 1. Item Pickup Priority
    bool can_pickup_new_item = !(gs.speed_boost_turns_left > 0 || gs.has_oil_slick);
    char item_at_target_type = 0; // 0 indicates no item or no pickuppable item

    if (can_pickup_new_item)
    {
        for (const auto &item_on_map : gs.items_on_map)
        {
            if (item_on_map.r == next_x && item_on_map.c == next_y)
            {
                item_at_target_type = item_on_map.type;
                break;
            }
        }
    }

    if (item_at_target_type != 0)
    {
        if (item_at_target_type == PAINT_BOMB_ITEM)
            score += 1000.0;
        else if (item_at_target_type == SPEED_BOOST_ITEM)
            score += 800.0;
        else if (item_at_target_type == OIL_SLICK_ITEM)
            score += 700.0;

        // If Paint Bomb *is picked up by this specific move*:
        if (item_at_target_type == PAINT_BOMB_ITEM)
        {
            int bomb_gain = 0;
            for (int dr_bomb = -2; dr_bomb <= 2; ++dr_bomb)
            { // 5x5 area
                for (int dc_bomb = -2; dc_bomb <= 2; ++dc_bomb)
                {
                    int bomb_r = next_x + dr_bomb;
                    int bomb_c = next_y + dc_bomb;
                    if (gs.is_within_bounds(bomb_r, bomb_c))
                    {
                        char cell_char_in_bomb_area = gs.grid[bomb_r][bomb_c];
                        bool cell_is_paintable_by_bomb = true;

                        if (cell_char_in_bomb_area == OBSTACLE_CELL || (cell_char_in_bomb_area >= 'a' && cell_char_in_bomb_area <= 'd'))
                        {
                            cell_is_paintable_by_bomb = false;
                        }

                        // Rule: "Nếu trong vùng có người chơi khác, các ô đó không bị tô"
                        // Rule: "Nếu một ô có nhiều hơn một người chơi thì màu của ô đó giữ nguyên"
                        // Interpretation: if any *other* player is on the cell, or if multiple players (even if one is me) are on the cell.
                        int players_on_bombed_cell = 0;
                        bool other_player_on_bombed_cell = false;

                        if (!gs.my_player.eliminated && gs.my_player.x == bomb_r && gs.my_player.y == bomb_c)
                        {
                            players_on_bombed_cell++;
                        }
                        for (const auto &op : gs.other_players)
                        {
                            if (!op.eliminated && op.x == bomb_r && op.y == bomb_c)
                            {
                                players_on_bombed_cell++;
                                other_player_on_bombed_cell = true;
                            }
                        }

                        if (other_player_on_bombed_cell || players_on_bombed_cell > 1)
                        {
                            cell_is_paintable_by_bomb = false;
                        }

                        if (cell_is_paintable_by_bomb && cell_char_in_bomb_area != my_color)
                        { // Counts empty and enemy cells
                            bomb_gain++;
                        }
                    }
                }
            }
            score += bomb_gain * 20.0; // Each cell from bomb is valuable
        }
    }

    // 2. Coloring Tiles (based on what's on grid *before* this move)
    if (target_cell_on_grid_char == EMPTY_CELL)
    {
        score += 100.0;
    }
    else if (isupper(target_cell_on_grid_char) && target_cell_on_grid_char != my_color)
    { // Enemy color
        score += 150.0;
    }
    else if (target_cell_on_grid_char == my_color)
    {                  // My color
        score += 10.0; // Moving to an already owned cell (less good, but ok for repositioning)
    }

    if (next_x == current_x && next_y == current_y)
    {                 // Staying put
        score -= 1.0; // Slight penalty for not actively expanding, unless it's a strategic hold.
    }

    // 3. Positional Advantages / Disadvantages
    //    - Avoid edges that will shrink soon
    int s_level_next = std::min({next_x, gs.M - 1 - next_x, next_y, gs.N - 1 - next_y});
    if (gs.K_shrink_period > 0)
    { // Avoid division by zero if K is somehow 0
        int turn_of_shrink_for_target_layer = (s_level_next + 1) * gs.K_shrink_period;
        int turns_until_shrink = turn_of_shrink_for_target_layer - gs.current_turn; // turns from START of this turn

        if (turns_until_shrink <= 0)
        {
            // This case should be caught by cell_will_be_sealed_this_turn giving -INF.
            // If somehow missed, apply heavy penalty.
            // score -= 500.0;
        }
        else if (turns_until_shrink <= gs.K_shrink_period)
        {
            score -= (gs.K_shrink_period - turns_until_shrink + 1) * 8.0; // Penalty gets worse closer to shrink
        }
    }

    // Prefer cells closer to map center
    int dist_to_center_r = std::abs(next_x - gs.M / 2);
    int dist_to_center_c = std::abs(next_y - gs.N / 2);
    score -= (dist_to_center_r + dist_to_center_c) * 1.0; // Small penalty for being far from center

    // 4. Aggression/Defense (simple version)
    for (const auto &op : gs.other_players)
    {
        if (!op.eliminated)
        {
            int dist_to_op = std::abs(next_x - op.x) + std::abs(next_y - op.y);
            if (dist_to_op == 0)
            { // Moving onto an opponent's current tile
                if (target_cell_on_grid_char == op.color_char)
                {                  // Stealing their colored tile
                    score += 75.0; // Significant bonus for direct capture of territory
                }
            }
            else if (dist_to_op < 3)
            {                                    // If moving near an opponent
                score += (3 - dist_to_op) * 5.0; // Small bonus for being near opponents (potential future captures)
            }
        }
    }

    if (used_oil_slick_for_this_move)
    {
        score += 50.0; // Bonus for using a limited resource effectively
    }

    return score;
}

struct MoveOption
{
    int x, y;
    bool oil_used;
    int steps; // 0 for stay, 1 for 1-step, 2 for 2-step
    double score;

    MoveOption(int _x = -1, int _y = -1, bool _oil = false, int _s = 0, double _scr = -std::numeric_limits<double>::infinity())
        : x(_x), y(_y), oil_used(_oil), steps(_s), score(_scr) {}
};

MoveOption decide_move(GameState &gs)
{
    int my_current_x = gs.my_player.x;
    int my_current_y = gs.my_player.y;

    std::vector<MoveOption> candidate_options;

    // --- 1-step moves (including stay) ---
    for (const auto &dir : DIRECTIONS)
    {
        int next_r = my_current_x + dir.dr;
        int next_c = my_current_y + dir.dc;
        int steps = (dir.dr == 0 && dir.dc == 0) ? 0 : 1;

        // Option 1: Standard move, no oil
        if (gs.is_valid_for_move(next_r, next_c, false))
        {
            candidate_options.emplace_back(next_r, next_c, false, steps);
        }
        // Oil slick for 1-step: "đi xuyên qua một ô cấm... phải kết thúc hành trình ở một ô khác hợp lệ."
        // For a 1-step move, there's no "intermediate" obstacle to pass through.
        // So, oil slick is not applicable for making a 1-step move onto an obstacle.
    }

    // --- 2-step moves (if speed boost active) ---
    if (gs.speed_boost_turns_left > 0)
    {
        for (const auto &dir : DIRECTIONS)
        {
            if (dir.dr == 0 && dir.dc == 0)
                continue; // Cannot "speed boost" staying put

            int inter_r = my_current_x + dir.dr; // Intermediate cell
            int inter_c = my_current_y + dir.dc;
            int final_r = my_current_x + 2 * dir.dr; // Final landing cell
            int final_c = my_current_y + 2 * dir.dc;

            // Path 1: No oil slick needed for the 2 steps
            if (gs.is_valid_for_move(inter_r, inter_c, false) && // Intermediate step must be valid
                gs.is_valid_for_move(final_r, final_c, false))
            { // Final step must be valid
                candidate_options.emplace_back(final_r, final_c, false, 2);
            }
            // Path 2: Use oil slick (if available and not expired) for one of the 2 steps
            // "vượt qua đúng 1 ô cấm duy nhất"
            else if (gs.has_oil_slick && gs.oil_slick_turns_to_expire > 0)
            {
                // Case A: Intermediate cell (inter_r, inter_c) IS the obstacle, final_r,c is clear
                // Here, is_valid_for_move(inter_r, inter_c, true) means we check if it's an obstacle we can pass
                // And final_r, final_c must be clear without oil.
                bool inter_is_passable_obstacle = false;
                if (gs.is_within_bounds(inter_r, inter_c))
                {
                    char inter_cell_char = gs.grid[inter_r][inter_c];
                    inter_is_passable_obstacle = (inter_cell_char == OBSTACLE_CELL || (inter_cell_char >= 'a' && inter_cell_char <= 'd'));
                }

                if (inter_is_passable_obstacle && gs.is_valid_for_move(final_r, final_c, false))
                {
                    // Check if inter_r, inter_c itself will be sealed this turn - if so, passing through it is risky/invalid if it disappears
                    if (!gs.cell_will_be_sealed_this_turn(inter_r, inter_c))
                    {
                        candidate_options.emplace_back(final_r, final_c, true, 2); // Oil used
                    }
                }
            }
        }
    }

    // Filter out moves that land on obstacles or self-eliminating shrink zones.
    // This is a crucial sanitization step.
    std::vector<MoveOption> valid_landings_options;
    for (const auto &opt : candidate_options)
    {
        if (!gs.is_within_bounds(opt.x, opt.y))
            continue;

        char landing_cell_char = gs.grid[opt.x][opt.y];
        if (landing_cell_char == OBSTACLE_CELL || (landing_cell_char >= 'a' && landing_cell_char <= 'd'))
        {
            continue; // Cannot land on an obstacle or sealed cell
        }
        if (gs.cell_will_be_sealed_this_turn(opt.x, opt.y))
        {
            continue; // Cannot land on a cell that will eliminate player this turn
        }
        valid_landings_options.push_back(opt);
    }

    if (valid_landings_options.empty())
    {
        // If no valid moves at all (e.g., completely trapped and current spot will also seal)
        // The game rule is "stand still". Output current position.
        // The judge will handle elimination if current_pos is also bad.
        return MoveOption(my_current_x, my_current_y, false, 0, -std::numeric_limits<double>::infinity());
    }

    // Evaluate all valid candidate options
    for (auto &opt : valid_landings_options)
    { // Use reference to modify score in place
        opt.score = evaluate_move(gs, opt.x, opt.y, my_current_x, my_current_y, opt.oil_used);
    }

    // Sort to find the best move: higher score first.
    // Tie-breaking: prefer not using oil, then prefer shorter moves.
    std::sort(valid_landings_options.begin(), valid_landings_options.end(),
              [](const MoveOption &a, const MoveOption &b)
              {
                  if (a.score != b.score)
                  {
                      return a.score > b.score; // Higher score is better
                  }
                  if (a.oil_used != b.oil_used)
                  {
                      return !a.oil_used; // Prefer not using oil (false comes before true)
                  }
                  return a.steps < b.steps; // Prefer shorter moves
              });

    // If all evaluated moves are infinitely bad (e.g. lead to elimination) default to staying.
    // This check might be redundant if the filtering above is perfect, but good as a safeguard.
    if (valid_landings_options.empty() || valid_landings_options[0].score <= -std::numeric_limits<double>::infinity() + 1.0)
    { // check against actual -INF
        // Evaluate staying put if it wasn't considered or had a bad score before
        double stay_score = evaluate_move(gs, my_current_x, my_current_y, my_current_x, my_current_y, false);
        // Check if current spot is actually valid to stay on
        if (gs.is_valid_for_move(my_current_x, my_current_y, false))
        {
            return MoveOption(my_current_x, my_current_y, false, 0, stay_score);
        }
        else
        { // Current spot is also bad, this is a tough situation, just output current.
            return MoveOption(my_current_x, my_current_y, false, 0, -std::numeric_limits<double>::infinity());
        }
    }

    return valid_landings_options[0]; // The best option after sorting
}

// --- Main Execution ---
int main()
{
    // Optional: For faster I/O in competitive programming, though less critical for file I/O
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL); // If reading from cin, not used here

    GameState gs;
    gs.parse_input("MAP.INP"); // Reads from MAP.INP and loads STATE.DAT

    int final_next_x = -1, final_next_y = -1;
    bool decided_to_use_oil_this_turn = false;

    if (gs.my_player.eliminated && gs.current_turn != 0)
    {
        final_next_x = 0; // Dummy valid output as per problem spec
        final_next_y = 0;
    }
    else if (gs.current_turn == 0)
    {
        std::pair<int, int> start_pos = choose_initial_position(gs);
        final_next_x = start_pos.first;
        final_next_y = start_pos.second;
        // No oil slick can be used or active on turn 0 for placement
    }
    else
    {
        MoveOption best_move = decide_move(gs);
        final_next_x = best_move.x;
        final_next_y = best_move.y;
        decided_to_use_oil_this_turn = best_move.oil_used;
    }

    // After deciding the move (final_next_x, final_next_y):
    // 1. Check for item pickup at the destination IF player is not eliminated and moving to a valid spot
    if (!gs.my_player.eliminated && gs.is_within_bounds(final_next_x, final_next_y))
    {
        gs.check_and_handle_item_pickup(final_next_x, final_next_y);
        // check_and_handle_item_pickup updates gs.has_oil_slick, gs.speed_boost_turns_left, etc.
        // If an oil slick was picked up, decided_to_use_oil_this_turn should remain false
        // unless the *move itself* was planned to use a *previously held* oil slick.
        // If we pick up oil slick AND use it in same turn, logic needs to be very specific.
        // Current logic assumes `decided_to_use_oil_this_turn` is about using a *pre-existing* oil slick.
        // If picking up oil allows its immediate use, `decide_move` would need to consider this.
        // For now, rule: "Vật phẩm được kích hoạt 1 lần duy nhất, khi người chơi tiến hành kích hoạt vật phẩm."
        // implies oil slick is picked up, then on a *subsequent* action/turn it's activated.
        // Or for speed/bomb, it's auto.
    }

    // 2. Decrement active item durations. Pass whether oil was *activated* for the chosen move.
    gs.decrement_item_durations(decided_to_use_oil_this_turn);

    // 3. Save persistent item state for the *next* turn
    gs.save_my_item_state();

    std::ofstream move_out_file("MOVE.OUT");
    if (move_out_file.is_open())
    {
        move_out_file << final_next_x << " " << final_next_y << std::endl;
        move_out_file.close();
    }
    else
    {
        std::cerr << "Error: Could not open MOVE.OUT for writing." << std::endl;
    }

    // Optional: Debugging output to stderr (remove or comment out for submission)
    /*
    std::cerr << "Turn: " << gs.current_turn << std::endl;
    std::cerr << "My Pos: (" << gs.my_player.x << "," << gs.my_player.y << ") Color: " << gs.my_player.color_char
              << " -> MoveTo: (" << final_next_x << "," << final_next_y << ")" << std::endl;
    std::cerr << "Items on map: " << gs.items_on_map.size() << std::endl;
    // for(const auto& item : gs.items_on_map) std::cerr << "  Item: " << item.r << " " << item.c << " " << item.type << std::endl;
    std::cerr << "SpeedBoostLeft: " << gs.speed_boost_turns_left
              << ", HasOilSlick: " << (gs.has_oil_slick ? "YES" : "NO")
              << ", OilExpiresIn: " << gs.oil_slick_turns_to_expire
              << ", OilUsedThisMove: " << (decided_to_use_oil_this_turn ? "YES" : "NO") << std::endl;
    std::cerr << "PaintBombJustPickedUpFlag (after pickup check): " << (gs.paint_bomb_just_picked_up ? "YES" : "NO") << std::endl;
    */

    return 0;
}