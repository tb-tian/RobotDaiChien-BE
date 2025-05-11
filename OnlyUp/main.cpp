#include <bits/stdc++.h>

using namespace std;

int M, N, K, T, currentX, currentY, P;
vector<vector<char> > board;
char color;

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

}

pair<int, int> solve() {
    if (currentX == -1 && currentY == -1) {
        return make_pair(M - 1, N - 1);
    }
    return make_pair(currentX - 1, currentY);
}

void output(const pair<int, int> &nextPositions) {
    ofstream outputFile("MOVE.OUT");
    outputFile << nextPositions.first << ' ' << nextPositions.second << '\n';
    outputFile.close();
}

int main() {

    srand(time(NULL) ^ 20230505);
    cin.tie(0) -> sync_with_stdio(0);
    cout.tie(0);

    input();
    output(solve());

    return 0;
}
