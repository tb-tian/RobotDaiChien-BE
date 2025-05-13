import random
from listOfPlayers import *
from Players import *

class PowerUp:
    def __init__(self, x, y, powerup_type, timeout):
        self.x = x
        self.y = y
        self.type = powerup_type
        self.timeout = timeout

    def toDictionary(self):
        return {"x": self.x, "y": self.y, "type": self.type, "timeout": self.timeout}
    
    def reduceTimeout(self):
        self.timeout -= 1

    def checkExpired(self):
        if self.timeout == 0:
            return True
        else:
            return False
    
def findPowerUp(x: int, y: int, listOfPowerUps: list) -> str:
    for powerUp in listOfPowerUps:
        if powerUp.x == x and powerUp.y == y:
            return powerUp.type
    return None

def deletePowerUp(x: int, y: int, listOfPowerUps: list) -> None:
    for powerUp in listOfPowerUps:
        if powerUp.x == x and powerUp.y == y:
            listOfPowerUps.remove(powerUp)

def updatePowerUpTimeout(listOfPowerUps: list) -> None:
    powerUpsToCheck = list(listOfPowerUps)
    for pu in powerUpsToCheck:
        pu.reduceTimeout()
        if pu.checkExpired():
            if pu in listOfPowerUps:
                listOfPowerUps.remove(pu)

def addPowerUp(board, listOfPowerUps: list, timeout: int, listOfPlayers: ListOfPlayers) -> None:
    powerup_types = ['G', 'E', 'F']
    chosen_types = random.sample(powerup_types, 2)
    movable_cells = board.listMovableCells()
    pos1 = listOfPlayers[0].getPosition()
    pos2 = listOfPlayers[1].getPosition()

    for cell in movable_cells:
            x1, y1 = pos1
            x2, y2 = pos2
            x_cell, y_cell = cell
            dist_pos1_to_cell = abs(x1 - x_cell) + abs(y1 - y_cell)
            dist_pos2_to_cell = abs(x2 - x_cell) + abs(y2 - y_cell)
            if dist_pos1_to_cell != dist_pos2_to_cell:
                movable_cells.remove(cell)

    for powerup_type in chosen_types:
        if not movable_cells: # No more available cells
            break
        x, y = random.choice(movable_cells)
        x, y = int(x), int(y)
        if findPowerUp(x, y, listOfPowerUps) == None:
            listOfPowerUps.append(PowerUp(x, y, powerup_type, timeout))
            movable_cells.remove((x,y))
        
        