#include <bits/stdc++.h>

using namespace std;

int M, N, K, T, currentX, currentY, P;
vector<vector<char> > board;
char color;
int tangtocTurns = 0; // Added for Tangtoc power-up
int dautronTurns = 0; // Added for DauTron power-up

void input() {

    int X, Y;
    char C;

    ifstream inputFile("MAP.INP");

    inputFile >> M >> N >> K >> T >> currentX >> currentY >> color >> P;

    for (int i = 0; i < P; ++i)
        inputFile >> X >> Y >> C;

    board.resize(M, vector<char>(N, '.'));

    for (int x = 0, y; x < M; ++x)
        for (y = 0; y < N; ++y)
            inputFile >> board[x][y];

    inputFile.close();

    ifstream stateFile("STATE.DAT");
    if (stateFile.is_open()) {
        stateFile >> tangtocTurns;
        if (!(stateFile >> dautronTurns)) { // Try to read dautronTurns
            dautronTurns = 0; // If it fails (e.g., older file), default to 0
        }
        stateFile.close();
    } else {
        tangtocTurns = 0; // Default if file doesn't exist or cannot be opened
        dautronTurns = 0; // Default for dautronTurns as well
    }
}

pair<int, int> solve() {
    if (currentX == -1 && currentY == -1) { // Initial placement
        vector<pair<int, int> > positions;
        for (int x = 0; x < M; ++x) {
            for (int y = 0; y < N; ++y) {
                if (board[x][y] != '#') {
                    positions.emplace_back(x, y);
                }
            }
        }
        random_shuffle(positions.begin(), positions.end());

        if (positions.empty()) {
            return make_pair(0, 0); // Should ideally not happen on a valid map
        }
        // Initial placement on 'G' doesn't activate Tangtoc; it activates after a move lands on 'G'.
        return positions.back();
    }

    pair<int, int> next_move_candidate = make_pair(currentX, currentY); // Default to staying put
    bool move_made = false;

    bool tangtoc_active_at_turn_start = (tangtocTurns > 0);
    if (tangtoc_active_at_turn_start) {
        tangtocTurns--; // Consume one turn of the power-up
    }

    bool dautron_active_at_turn_start = (dautronTurns > 0);
    if (dautron_active_at_turn_start) {
        dautronTurns--; // Consume one turn of DauTron
    }

    vector<pair<short, short> > directions({{-1, 0}, {0, -1}, {0, 1}, {1, 0}});
    random_shuffle(directions.begin(), directions.end());

    // Attempt Tangtoc (2-step) move if power-up was active at the start of this turn
    if (tangtoc_active_at_turn_start) {
        for (const auto &[dx, dy] : directions) {
            int x1_intermediate = currentX + dx;
            int y1_intermediate = currentY + dy;
            int x2_final = currentX + 2 * dx;
            int y2_final = currentY + 2 * dy;

            // Check intermediate step: in bounds and not an obstacle
            if (x1_intermediate >= 0 && x1_intermediate < M && 
                y1_intermediate >= 0 && y1_intermediate < N && 
                board[x1_intermediate][y1_intermediate] != '#') {
                
                // Check final 2-step destination: in bounds, not an obstacle
                // and (not own color OR dautron is active)
                if (x2_final >= 0 && x2_final < M && 
                    y2_final >= 0 && y2_final < N && 
                    board[x2_final][y2_final] != '#' && 
                    (dautron_active_at_turn_start || board[x2_final][y2_final] != color)) {
                    
                    next_move_candidate = make_pair(x2_final, y2_final);
                    move_made = true;
                    break; // Found a 2-step move
                }
            }
        }
    }

    // If no 2-step move was made (or Tangtoc wasn't active), attempt a 1-step move
    if (!move_made) {
        for (const auto &[dx, dy] : directions) { // Can reuse the same shuffled directions
            int nextX = currentX + dx;
            int nextY = currentY + dy;

            // Check 1-step destination: in bounds, not an obstacle
            // and (not own color OR dautron is active)
            if (nextX >= 0 && nextX < M && 
                nextY >= 0 && nextY < N && 
                board[nextX][nextY] != '#' && 
                (dautron_active_at_turn_start || board[nextX][nextY] != color)) {
                
                next_move_candidate = make_pair(nextX, nextY);
                move_made = true;
                break; // Found a 1-step move
            }
        }
    }
    
    // If no move was made by either Tangtoc or 1-step logic, next_move_candidate remains (currentX, currentY)

    // Check if the bot landed on a 'G' cell to activate/reset Tangtoc power-up
    // This check uses the coordinates of the cell the bot is moving to.
    if (board[next_move_candidate.first][next_move_candidate.second] == 'G') {
        tangtocTurns = 5; // Activate Tangtoc for the next 5 turns
    }
    // Check if the bot landed on a 'D' cell to activate/reset DauTron power-up
    if (board[next_move_candidate.first][next_move_candidate.second] == 'D') {
        dautronTurns = 3; // Activate DauTron for the next 3 turns
    }

    return next_move_candidate;
}

void output(const pair<int, int> &nextPositions) {
    ofstream outputFile("MOVE.OUT");
    outputFile << nextPositions.first << ' ' << nextPositions.second << '\n';
    outputFile.close();

    ofstream stateFile("STATE.DAT");
    if (stateFile.is_open()) {
        stateFile << tangtocTurns << endl;
        stateFile << dautronTurns << endl; // Save dautronTurns
        stateFile.close();
    }
    cout << tangtocTurns << " " << dautronTurns << endl; // Updated debug output
}

int main() {

    srand(time(NULL) ^ 20230505);
    cin.tie(0) -> sync_with_stdio(0);
    cout.tie(0);

    input();
    output(solve());

    return 0;
}
