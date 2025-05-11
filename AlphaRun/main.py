if __name__ == "__main__":
    with open("MAP.INP") as inputFile:
        M, N, K, T = map(int, inputFile.readline()[:-1].split())
        #print(M, N, K, T)
        currentX, currentY, color = inputFile.readline()[:-1].split()
        currentX = eval(currentX)
        currentY = eval(currentY)
        #print(currentX, currentY, color)
        P = int(inputFile.readline()[:-1])
        for _ in range(P):
            inputFile.readline()
        board = [[] for x in range(N)]
        for x in range(N):
            line = inputFile.readline()
            if (len(line) > 0) and (line[-1] == "\n"):
                line = line[:-1] 
            board[x] = line.split(" ")
            #print(board[x])
        