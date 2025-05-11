#include <bits/stdc++.h>

using namespace std;
const int inf = 1e9;

int M, N, K, T, currentX, currentY, P;
vector<vector<char> > board;
vector<vector<int> > value;
char color;

void input() {

    int X, Y;
    char C;

    ifstream inputFile("MAP.INP");

    inputFile >> M >> N >> K >> T >> currentX >> currentY >> color >> P;

    for (int i = 0; i < P; ++i)
        inputFile >> X >> Y >> C;

    board.resize(M, vector<char>(N, '.'));
    value.resize(M, vector<int> (N, inf));
    for (int x = 0, y; x < M; ++x)
        for (y = 0; y < N; ++y){
            inputFile >> board[x][y];
        }
    inputFile.close();

}

int manhattan(int x, int y, int u, int v){
    return abs(x - u) + abs(y - v);
}

int calc(int u, int v, char ch, int currentX, int currentY){
    if (ch == '#' || ('a' <= ch && ch <= 'z')) return inf;
    int val = abs(7 - manhattan(currentX, currentY, u, v));
    if ('A' <= ch && ch <= 'D') val -= 1;
    if (ch != color){
        val -= 1;
        vector<pair<short, short> > directions({{-1, 0}, {0, -1}, {0, 1}, {1, 0}});
        bool isGood = false;
        for (const auto &[dx, dy] : directions) {
            int nextX = u + dx;
            int nextY = v + dy;
            if (nextX < 0 || nextY < 0 || nextX >= M || nextY >= N || board[nextX][nextY] == '#')
                continue;
            if (board[nextX][nextY] == color) isGood = true;
        }
        val -= isGood;
    }
    return val;
}

vector <vector<int>> bfs(int currentX, int currentY){
    vector <vector<int> > dist;
    dist.resize(M, vector<int> (N, inf));
    vector<pair<short, short> > directions({{-1, 0}, {0, -1}, {0, 1}, {1, 0}});
    queue <pair<int, int>> q;

    q.push(make_pair(currentX, currentY));
    dist[currentX][currentY] = 0;
    while (!q.empty()){
        int curX = q.front().first;
        int curY = q.front().second;
        q.pop();
        for (const auto &[dx, dy] : directions) {
            int nextX = curX + dx;
            int nextY = curY + dy;
            if (nextX < 0 || nextY < 0 || nextX >= M || nextY >= N || value[nextX][nextY] == inf)
                continue;
            if (dist[nextX][nextY] > dist[curX][curY] + 1){
                q.push(make_pair(nextX, nextY));
                dist[nextX][nextY] = dist[curX][curY] + 1;
            }
        }
    }
    return dist;

}

bool findPath(int currentX, int currentY, int endX, int endY, vector <vector<int>> dist, vector <pair<int, int>> &path){
    vector<pair<short, short> > directions({{-1, 0}, {0, -1}, {0, 1}, {1, 0}});
    path.clear();

    if (dist[endX][endY] == inf) return false;
    if (currentX != endX || currentY != endY)
        path.push_back(make_pair(endX, endY));
    while (endX != currentX || endY != currentY){
        //cerr << currentX << " " << currentY << " " << endX << " " << endY << endl;
        for (const auto &[dx, dy] : directions) {
            int nextX = endX + dx;
            int nextY = endY + dy;
            if (nextX < 0 || nextY < 0 || nextX >= M || nextY >= N || value[nextX][nextY] == inf)
                continue;
            if (dist[nextX][nextY] + 1 == dist[endX][endY]){
                endX = nextX;
                endY = nextY;
                break;
            }
        }
        if (currentX != endX || currentY != endY)
            path.push_back(make_pair(endX, endY));
    }
    return true;
}

void generateNewPath(vector <pair<int, int>> path){
    ofstream outputFile("STATE.DAT");
    for (const auto &[nextX, nextY] : path){
        outputFile << nextX << " " << nextY << endl;
    }
    outputFile.close();
}

bool is_empty_file(std::ifstream& pFile)
{
    if (!pFile) return true;
    return pFile.peek() == std::ifstream::traits_type::eof();
}

pair<int, int> solve() {
    if (currentX == -1 && currentY == -1) {
        return make_pair( M / 2 + 1, N / 2 + 1);
        // vector<pair<int, int> > positions;

        // for (int x = 0, y; x < M; ++x)
        //     for (y = 0; y < N; ++y)
        //         if (board[x][y] != '#')
        //             positions.emplace_back(x, y);

        // random_shuffle(positions.begin(), positions.end());

        // if (positions.empty())
        //     return make_pair(0, 0);

        // return positions.back();
    }
    vector <pair<int, int>> path;
    ifstream inputFile("STATE.DAT");
    if (!is_empty_file(inputFile)){
        int nextX = -1, nextY = -1;
        while (inputFile >> nextX >> nextY){
            path.push_back(make_pair(nextX, nextY));
        }
    }
    inputFile.close();

    if (path.empty()){
        priority_queue<pair<int, pair<int, int>>, vector<pair<int, pair<int, int>>>, greater<pair<int, pair<int, int>>>> PQ;
               for (int x = 0; x < M; ++x){
            for (int y = 0; y < N; ++y){
                value[x][y] = calc(x, y, board[x][y], currentX, currentY);
                PQ.push(make_pair(value[x][y], make_pair(x, y)));
                //cout << dist[x][y] << " ";
            }
            //cout << endl;
        }
        vector <vector<int> > dist = bfs(currentX, currentY);
        while (!PQ.empty()){
            int nextX = PQ.top().second.first;
            int nextY = PQ.top().second.second;
            //cerr << nextX << " " << nextY << " " << value[nextX][nextY] << endl;
            PQ.pop();
            if (findPath(currentX, currentY, nextX, nextY, dist, path)){
                break;
            }
        }
        //return make_pair(0, 0);
    }
    if (path.empty()) return make_pair(-1, -1);

    int nextX = path.back().first, nextY = path.back().second;
    path.pop_back();

    generateNewPath(path);
    return make_pair(nextX, nextY);
}

void output(const pair<int, int> &nextPositions) {
    ofstream outputFile("MOVE.OUT");
    outputFile << nextPositions.first << ' ' << nextPositions.second << '\n';
    outputFile.close();
}

int main() {

    srand(time(NULL));
    cin.tie(0) -> sync_with_stdio(0);
    cout.tie(0);

    input();
    output(solve());

    return 0;
}
