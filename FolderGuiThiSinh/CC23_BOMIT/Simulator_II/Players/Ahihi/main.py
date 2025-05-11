M = N = K = T = X = Y = 0
C = '?'

with open("MAP.INP", "r") as inputFile:
    M, N, K, T = map(int, inputFile.readline()[:-1].split(" "))
    X, Y, C = inputFile.readline()[:-1].split(" ")
    if T == 0:
        X = M - 1
        Y = N - 1
    else:
        X = int(X)
        Y = int(Y)

with open("MOVE.OUT", "w") as outputFile:
    outputFile.writelines([f"{X} {Y}\n"])