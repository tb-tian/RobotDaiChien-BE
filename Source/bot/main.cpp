#include <bits/stdc++.h>

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

double evaluate_move(GameState &gs, int next_x, int next_y, int current_x, int current_y, bool used_oil_slick_for_this_move, int steps_taken);
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

double evaluate_move(GameState &gs, int next_x, int next_y, int current_x, int current_y, bool used_oil_slick_for_this_move, int steps_taken)
{
    double score = 0.0;
    char my_color = gs.my_player.color_char;

    if (!gs.is_within_bounds(next_x, next_y))
        return -std::numeric_limits<double>::infinity();

    char target_cell_on_grid_char = gs.grid[next_x][next_y];
    if (target_cell_on_grid_char == OBSTACLE_CELL || (target_cell_on_grid_char >= 'a' && target_cell_on_grid_char <= 'd'))
    {
        return -std::numeric_limits<double>::infinity();
    }
    if (gs.cell_will_be_sealed_this_turn(next_x, next_y))
    {
        return -std::numeric_limits<double>::infinity();
    }

    bool item_action_taken = false;

    // 1. Item Pickup Priority (Highest)
    bool can_pickup_new_item = !(gs.speed_boost_turns_left > 0 || gs.has_oil_slick);
    char item_at_target_type = 0; 

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
        score += 20000.0; // Massive base score for picking up any item
        item_action_taken = true;
        if (item_at_target_type == PAINT_BOMB_ITEM)
            score += 1000.0; // Additional small bonus for paint bomb
        else if (item_at_target_type == SPEED_BOOST_ITEM)
            score += 800.0;  // Additional small bonus for speed boost
        else if (item_at_target_type == OIL_SLICK_ITEM)
            score += 700.0;  // Additional small bonus for oil slick

        if (item_at_target_type == PAINT_BOMB_ITEM)
        {
            int bomb_gain = 0;
            for (int dr_bomb = -2; dr_bomb <= 2; ++dr_bomb)
            { 
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
                        { 
                            bomb_gain++;
                        }
                    }
                }
            }
            score += bomb_gain * 2.0; // Reduced multiplier, base pickup score is dominant
        }
    }

    // 2. Item Usage Priority (Second Highest)
    if (steps_taken == 2) { // Implies speed boost usage
        score += 15000.0;
        item_action_taken = true;
    }
    if (used_oil_slick_for_this_move)
    {
        score += 15000.0; // Same high score for using oil slick
        item_action_taken = true;
    }


    // // 3. Coloring Tiles (Much lower priority, only if no item action)
    // if (!item_action_taken) { // Only consider these if no item pickup/usage
    //     if (target_cell_on_grid_char == EMPTY_CELL)
    //     {
    //         score += 10.0; // Was 100.0
    //     }
    //     else if (isupper(target_cell_on_grid_char) && target_cell_on_grid_char != my_color)
    //     { // Enemy color
    //         score += 15.0; // Was 150.0
    //     }
    //     else if (target_cell_on_grid_char == my_color)
    //     {                  // My color
    //         score += 1.0; // Was 10.0
    //     }
    // }


    if (next_x == current_x && next_y == current_y)
    {                 // Staying put
        score -= 0.1; // Slight penalty, less significant now
    }

    // 4. Positional Advantages / Disadvantages (Minor impact)
    int s_level_next = std::min({next_x, gs.M - 1 - next_x, next_y, gs.N - 1 - next_y});
    if (gs.K_shrink_period > 0)
    { 
        int turn_of_shrink_for_target_layer = (s_level_next + 1) * gs.K_shrink_period;
        int turns_until_shrink = turn_of_shrink_for_target_layer - gs.current_turn; 

        if (turns_until_shrink <= 0)
        {
            // Already handled by -INF
        }
        else if (turns_until_shrink <= gs.K_shrink_period)
        {
            score -= (gs.K_shrink_period - turns_until_shrink + 1) * 0.8; // Was 8.0
        }
    }

    int dist_to_center_r = std::abs(next_x - gs.M / 2);
    int dist_to_center_c = std::abs(next_y - gs.N / 2);
    score -= (dist_to_center_r + dist_to_center_c) * 0.1; // Was 1.0

    // // 5. Aggression/Defense (Minor impact, only if no item action)
    // if (!item_action_taken) {
    //     for (const auto &op : gs.other_players)
    //     {
    //         if (!op.eliminated)
    //         {
    //             int dist_to_op = std::abs(next_x - op.x) + std::abs(next_y - op.y);
    //             if (dist_to_op == 0)
    //             { 
    //                 if (target_cell_on_grid_char == op.color_char)
    //                 {                  
    //                     score += 7.5; // Was 75.0
    //                 }
    //             }
    //             else if (dist_to_op < 3)
    //             {                                    
    //                 score += (3 - dist_to_op) * 0.5; // Was 5.0
    //             }
    //         }
    //     }
    // }
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

// Find the first step toward the nearest item using BFS
std::pair<bool, std::pair<int, int>> find_path_to_nearest_item(const GameState &gs) {
    int my_x = gs.my_player.x;
    int my_y = gs.my_player.y;
    
    // If we can't pick up a new item or there are no items, don't bother
    bool can_pickup_new_item = !(gs.speed_boost_turns_left > 0 || gs.has_oil_slick);
    if (!can_pickup_new_item || gs.items_on_map.empty()) {
        return {false, {0, 0}};
    }
    
    // BFS queue stores: {x, y, parent_x, parent_y}
    std::queue<std::tuple<int, int, int, int>> q;
    std::vector<std::vector<bool>> visited(gs.M, std::vector<bool>(gs.N, false));
    std::vector<std::vector<std::pair<int, int>>> parent(gs.M, std::vector<std::pair<int, int>>(gs.N, {-1, -1}));
    
    // Start BFS
    q.push({my_x, my_y, -1, -1});
    visited[my_x][my_y] = true;
    
    std::pair<int, int> item_pos = {-1, -1};
    
    // BFS to find the nearest item
    while (!q.empty() && item_pos.first == -1) {
        auto [curr_x, curr_y, par_x, par_y] = q.front();
        q.pop();
        
        // Check if current cell has an item
        for (const auto &item : gs.items_on_map) {
            if (item.r == curr_x && item.c == curr_y) {
                item_pos = {curr_x, curr_y};
                parent[curr_x][curr_y] = {par_x, par_y};
                break;
            }
        }
        
        if (item_pos.first != -1) break; // Item found
        
        // Try all directions
        for (const auto &dir : DIRECTIONS) {
            if (dir.dr == 0 && dir.dc == 0) continue; // Skip staying put
            
            int next_x = curr_x + dir.dr;
            int next_y = curr_y + dir.dc;
            
            if (gs.is_valid_for_move(next_x, next_y, false) && !visited[next_x][next_y]) {
                q.push({next_x, next_y, curr_x, curr_y});
                parent[next_x][next_y] = {curr_x, curr_y};
                visited[next_x][next_y] = true;
            }
        }
    }
    
    // If item found, trace back to find first step
    if (item_pos.first != -1) {
        int x = item_pos.first;
        int y = item_pos.second;
        
        // Trace back to find the first step from our position
        while (parent[x][y].first != my_x || parent[x][y].second != my_y) {
            // If we somehow hit the start (shouldn't happen if BFS found a path)
            if (parent[x][y].first == -1 || parent[x][y].second == -1) {
                return {false, {0, 0}};
            }
            
            // Move back one step
            int new_x = parent[x][y].first;
            int new_y = parent[x][y].second;
            x = new_x;
            y = new_y;
        }
        
        // We now have the first step from our position
        return {true, {x, y}};
    }
    
    return {false, {0, 0}}; // No path found
}

MoveOption decide_move(GameState &gs)
{
    int my_current_x = gs.my_player.x;
    int my_current_y = gs.my_player.y;

    std::vector<MoveOption> candidate_options;

    // First, find the best path to the nearest item using BFS
    auto [found_path, next_step] = find_path_to_nearest_item(gs);

    // --- 1-step moves (including stay) ---
    for (const auto &dir : DIRECTIONS)
    {
        int next_r = my_current_x + dir.dr;
        int next_c = my_current_y + dir.dc;
        int steps = (dir.dr == 0 && dir.dc == 0) ? 0 : 1;

        // Option 1: Standard move, no oil
        if (gs.is_valid_for_move(next_r, next_c, false))
        {
            // If this is the first step toward the nearest item, give it a massive score boost
            double bonus_score = 0;
            if (found_path && next_r == next_step.first && next_c == next_step.second) {
                bonus_score = 1000000.0; // Extremely high priority for moving toward items
            }
            candidate_options.emplace_back(next_r, next_c, false, steps, bonus_score);
        }
        
        // Handle oil slick for 1-step moves through obstacles
        if (gs.has_oil_slick && gs.oil_slick_turns_to_expire > 0) {
            // Check if this direction would go through an obstacle
            if (gs.is_within_bounds(next_r, next_c)) {
                char cell = gs.grid[next_r][next_c];
                bool is_obstacle = (cell == OBSTACLE_CELL || (cell >= 'a' && cell <= 'd'));
                
                if (is_obstacle && !gs.cell_will_be_sealed_this_turn(next_r, next_c)) {
                    // Try to find a valid landing spot after the obstacle
                    int final_r = next_r + dir.dr;
                    int final_c = next_c + dir.dc;
                    
                    if (gs.is_valid_for_move(final_r, final_c, false)) {
                        // Check if this gets us to or closer to an item
                        double bonus_score = 0;
                        if (found_path && (final_r == next_step.first && final_c == next_step.second)) {
                            bonus_score = 1000000.0;
                        }
                        candidate_options.emplace_back(final_r, final_c, true, 2, bonus_score);
                    }
                }
            }
        }
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
                double bonus_score = 0;
                if (found_path && final_r == next_step.first && final_c == next_step.second) {
                    bonus_score = 1000000.0; // Prioritize moves that reach the item faster
                }
                candidate_options.emplace_back(final_r, final_c, false, 2, bonus_score);
            }
            // ...existing code for oil slick usage...
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
        // Add the evaluation score to any existing bonus score from pathfinding
        opt.score += evaluate_move(gs, opt.x, opt.y, my_current_x, my_current_y, opt.oil_used, opt.steps);
    }

    // Sort to find the best move: higher score first.
    // Tie-breaking: prefer using items (oil, speed boost), then other criteria.
    std::sort(valid_landings_options.begin(), valid_landings_options.end(),
              [](const MoveOption &a, const MoveOption &b)
              {
                  if (a.score != b.score)
                  {
                      return a.score > b.score; // Higher score is better
                  }
                  // Tie-breaking: Prefer using oil if scores are identical
                  if (a.oil_used != b.oil_used)
                  {
                      return a.oil_used; // true (used oil) > false (didn't use oil)
                  }
                  // Tie-breaking: Prefer 2-step moves (using speed boost) if scores are identical
                  if (a.steps != b.steps) {
                      return a.steps > b.steps; // 2 steps > 1 step > 0 steps
                  }
                  // Final tie-breaker: prefer not staying put
                  bool a_is_stay = (a.steps == 0);
                  bool b_is_stay = (b.steps == 0);
                  if (a_is_stay != b_is_stay) {
                      return b_is_stay; // prefer not staying (false for is_stay is better)
                  }
                  return false; // Keep stable or add more criteria
              });

    // If all evaluated moves are infinitely bad (e.g. lead to elimination) default to staying.
    // This check might be redundant if the filtering above is perfect, but good as a safeguard.
    if (valid_landings_options.empty() || valid_landings_options[0].score <= -std::numeric_limits<double>::infinity() + 1.0)
    { // check against actual -INF
        // Evaluate staying put if it wasn't considered or had a bad score before
        double stay_score = evaluate_move(gs, my_current_x, my_current_y, my_current_x, my_current_y, false, 0);
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