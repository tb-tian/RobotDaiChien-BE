import random
from player import *
from board import Board

class ListOfPlayers:
    def __init__(self, numberOfPlayers = None):
        self.playersList = list()
        if numberOfPlayers != None:
            x = y = -1
            for i in range(numberOfPlayers):
                color = chr(ord('A') + i)
                self.playersList.append(Player(x, y, color = color))

    def __len__(self):
        return len(self.playersList)
    
    def chooseStartingPositions(self, outputs, board : Board) -> bool:
        self.playersList = list()
        
        moveableCells = board.listMovableCells()
        random.shuffle(moveableCells)
        numberOfMovableCells = len(moveableCells)

        for i in range(len(outputs)):
            #print(outputs)
            x, y = outputs[i]
            color = chr(ord('A') + i)
            if board.checkUnmovable(x, y):
                x, y = moveableCells[random.randint(0, numberOfMovableCells - 1)]
                self.playersList.append(Player(x, y, color = color))
            else:
                self.playersList.append(Player(x, y, color = color))
        return outputs

    def toDictionary(self) -> dict:
        result = dict()
        for i in range(len(self.playersList)):
            result[str(i)] = self.playersList[i].toDictionary()
        return result
    
    def __getitem__(self, x : int) -> Player:
        if (x < 0) or (x >= len(self)):
            return None
        return self.playersList[x]
    
    def setArea(self, area : list[int]):
        for c in range(len(area)):
            self.playersList[c].setArea(area[c])

    def countAlivePlayers(self):
        result = 0
        for player in self.playersList:
            if player.checkAlive():
                result += 1
        return result

    def moveNext(self, outputs, board = None) -> list:
        result = list()
        for i in range(len(self.playersList)):
            #print("Player", i)
            #print(self.playersList[i].getPosition())
            #print(outputs[i])
            result.append(self.playersList[i].moveNext(outputs[i], board))
            #print(self.playersList[i].getPosition())
        return result