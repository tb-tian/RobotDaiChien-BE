from board import *
import math

class Player:
    def __init__(self, x : int, y : int, alive : bool = True, area : int = 0, color : str = "?"):
        self.x = x
        self.y = y
        self.alive = alive
        self.area = area
        self.color = color
    
    def getColor(self) -> str:
        return self.color

    def toDictionary(self) -> dict:
        result = dict()
        result["position"] = {"x" : self.x, "y" : self.y}
        result["alive"] = self.alive
        result["area"] = self.area
        result["color"] = self.color
        return result
    
    def checkAlive(self) -> bool:
        return self.alive
    
    def getKilled(self) -> bool:
        self.alive = False
        self.x = self.y = -1
        return self.alive

    def getPositionX(self) -> int:
        if not self.alive:
            return -1
        return self.x

    def getPositionY(self) -> int:
        if not self.alive:
            return -1
        return self.y 
    
    def getPosition(self) -> list:
        if not self.alive:
            return (-1, -1)
        return (self.x, self.y)

    def increaseArea(self, delta : int = 1) -> int:
        if self.area + delta < 0:
            return self.area
        self.area += delta
        return self.area

    def setArea(self, area : int) -> int:
        if area < 0:
            return self.area
        self.area = area
        return area

    def setPosition(self, x : int, y : int) -> tuple:
        if not self.alive:
            return (-1, -1)

        self.x = x
        self.y = y
        return (x, y)

    def moveNext(self, intendedPosition, board = None) -> tuple:
        if not self.alive:
            return (-1, -1)
        
        try:
            bound = board.getShape()
            nextX, nextY = intendedPosition
            #print(self.x, self.y, nextX, nextY)
            if (not (isinstance(nextX, int) and isinstance(nextY, int))):
                return (self.x, self.y)
            #print("first pass")
            if ((self.x < 0) or (self.y < 0) or (abs(self.x - nextX) + abs(self.y - nextY) > 1)):
                return (self.x, self.y)
            #print("second pass")
            if (board != None):
                #print(bound, self.x, self.y)
                if ((nextX >= bound[0]) or (nextY >= bound[1])):
                    return (self.x, self.y)
                if (board.checkUnmovable(nextX, nextY)):
                    return (self.x, self.y)
                
            self.x = nextX
            self.y = nextY
        except Exception as e:
            pass
        return (self.x, self.y)