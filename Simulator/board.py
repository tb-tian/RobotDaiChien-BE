import collections
import copy

class Board:
    def __init__(self):
        self.numberOfRows = self.numberOfColumns = 0
        self.grid = list()

    def setNumberOfRows(self, numberOfRows : int) -> bool:
        if numberOfRows < 0:
            return False
        if numberOfRows == self.numberOfRows:
            return True
        
        if numberOfRows < self.numberOfRows:
            self.grid = self.grid[:numberOfRows]
        elif numberOfRows > self.numberOfRows:
            for i in range(self.numberOfRows, numberOfRows):
                self.grid.append("." * self.numberOfColumns)

        self.numberOfRows = numberOfRows

        return True

    def setNumberOfColumns(self, numberOfColumns : int) -> bool:
        if numberOfColumns < 0:
            return False
        
        delta = numberOfColumns - self.numberOfColumns

        if delta == 0:
            return True
        
        self.numberOfColumns = numberOfColumns

        if delta < 0:
            for x in range(self.numberOfRows):
                self.grid[x] = self.grid[x][:numberOfColumns]
        elif delta > 0:
            for x in range(self.numberOfRows):
                self.grid[x] += "." * delta

        return True

    def appendRow(self, row : str) -> bool:
        
        if (len(row) != self.numberOfColumns):
            return False

        self.append(row)
        self.numberOfRows += 1

        return True
    
    def appendColumn(self, column : str) -> bool:
        
        if (len(column) != self.numberOfRows):
            return False

        for x in range(self.numberOfRows):
            self.grid[x] += column[x]

        return True

    def setRow(self, x : int, row : str) -> bool:
        if (x < 0) or (x >= self.numberOfRows) or (len(row) != self.numberOfColumns):
            return False
        self.grid[x] = row
        return True
    
    def setColumn(self, y : int, column : str) -> bool:
        if (y < 0) or (y >= self.numberOfColumns) or (len(column) != self.setNumberOfRows):
            return False
        for x in range(self.numberOfRows):
            self[x][y] = column[x]
        return True
    
    def setCell(self, x : int, y : int, s : str) -> bool:
        if (len(s) != 1) or (y < 0) or (y >= self.numberOfColumns):
            return False
        self.grid[x] = self.grid[x][:y] + s + self.grid[x][(y + 1):] 

        return True

    def shrink(self, length : int) -> bool:
        if (length < 0):
            return False
        
        for x in range(0, self.numberOfRows):
            for y in (length, self.numberOfColumns - 1 - length):
                currentCell = self.getCell(x, y)
                if currentCell == '.':
                    self.setCell(x, y, '#')
                elif currentCell.isupper() and currentCell.isalpha():
                    self.setCell(x, y, currentCell.lower())

        for y in range(0, self.numberOfColumns):
            for x in (length, self.numberOfRows - 1 - length):
                currentCell = self.getCell(x, y)
                if currentCell == '.':
                    self.setCell(x, y, '#')
                elif currentCell.isupper() and currentCell.isalpha():
                    self.setCell(x, y, currentCell.lower())

        return True

    def getCell(self, x : int, y : int) -> str:
        return self.grid[x][y]
    
    def checkMovable(self, x : int, y : int) -> bool:
        if (x < 0) or (y < 0) or (x >= self.numberOfRows) or (y >= self.numberOfColumns):
            return False
        return (self.grid[x][y] == ".") or (self.grid[x][y].isalpha() and self.grid[x][y].isupper())

    def listMovableCells(self) -> list:
        result = []
        for x in range(self.numberOfRows):
            for y in range(self.numberOfColumns):
                if self.checkUnmovable(x, y):
                    continue
                result.append((x, y))
        return result

    def checkUnmovable(self, x : int, y : int):
        if (x < 0) or (y < 0) or (x >= self.numberOfRows) or (y >= self.numberOfColumns):
            return True 
        return (self.grid[x][y] == '#') or (self.grid[x][y].isalpha() and self.grid[x][y].islower()) 

    def getNumberOfRows(self):
        return self.numberOfRows
    
    def getNumberOfColumns(self):
        return self.numberOfColumns

    def getShape(self):
        return (self.numberOfRows, self.numberOfColumns)

    def toDictionary(self, initialBoard = None) -> dict:
        result = dict()
        result["rows"] = self.numberOfRows
        result["columns"] = self.numberOfColumns
        
        if (initialBoard == None):
            result["grid"] = copy.deepcopy(self.grid)
        else:
            grid = copy.deepcopy(initialBoard)
            for x in range(self.numberOfRows):
                for y in range(self.numberOfColumns):
                    if (self.grid[x][y] == '#') and (grid[x][y] != '#'):
                        grid[x][y] = '*'
                    else:
                        grid[x][y] = self.grid[x][y]
                grid[x] = "".join(grid[x])
            result["grid"] = grid
        return result
    
    def checkAtBorder(self, x : int, y : int) -> bool:
        return (x == 0) or (y == 0) or (x == self.numberOfRows - 1) or (y == self.numberOfColumns - 1)
    
    def checkInsideBoard(self, x : int, y : int) -> bool:
        return (0 <= x) and (0 <= y) and (x < self.numberOfRows) and (y < self.numberOfColumns)
    
    def checkOutsideBoard(self, x : int, y : int) -> bool:
        return (x < 0) or (y < 0) or (x >= self.numberOfRows) or (y >= self.numberOfColumns)

    def checkBorderReachable(self, x : int, y : int, c : str) -> bool:
        if (x < 0) or (y < 0) or (x >= self.numberOfRows) or (y >= self.numberOfColumns) or self.checkUnmovable(x, y):
            return False
        c = c.upper()
        if (self.grid[x][y] == c):
            return False
        if (self.checkAtBorder(x, y)):
            return True
        visited = [[False for y in range(self.numberOfColumns)] for x in range(self.numberOfRows)]
        q = collections.deque()
        q.append((x, y))
        visited[x][y] = True
        while len(q) >= 1:
            x, y = q.popleft()
            for (dx, dy) in ((-1, 0), (0, -1), (0, 1), (1, 0)):
                nextX = dx + x
                nextY = dy + y
                #if (nextX < 0) or (nextY < 0) or (nextX >= self.numberOfRows) or (nextY >= self.numberOfColumns) or visited[nextX][nextY]:
                #    continue
                #if self.checkUnmovable(nextX, nextY) or visited[nextX][nextY] or self.grid[nextX][nextY] == c:
                #    continue
                ### Allow "go through" obstacles
                if self.checkOutsideBoard(nextX, nextY) or visited[nextX][nextY] or self.grid[nextX][nextY].upper() == c:
                    continue
                if self.checkAtBorder(nextX, nextY):
                    return True
                q.append((nextX, nextY))
                visited[nextX][nextY] = True
        return False

    def updateCoveredArea(self, numberOfColors : int):
        result = []
        for c in range(numberOfColors):
            color = chr(c + ord('A'))
            for x in range(self.numberOfRows):
                for y in range(self.numberOfColumns):
                    
                    previousColor = self.grid[x][y].upper()

                    if self.checkUnmovable(x, y) or self.checkBorderReachable(x, y, color):
                        continue
                    self.setCell(x, y, color)

                    if self.grid[x][y].upper() != previousColor:
                        result.append((x, y))

        return result

    def findColoredArea(self, numberOfColors : int) -> list:
        result = [0 for c in range(numberOfColors)]
        for c in range(numberOfColors):
            color = chr(c + ord('A'))
            for x in range(self.numberOfRows):
                for y in range(self.numberOfColumns):
                    if self.grid[x][y].upper() == color:
                        result[c] += 1
        return result
    
    def getGrid(self):
        return [[copy.deepcopy(cell) for cell in row] for row in self.grid]

    def checkGameStopped(self) -> bool:
        for row in self.grid:
            for cell in row:
                if (cell == '.') or (cell.isalpha() and cell.isupper()):
                    return False
        return True