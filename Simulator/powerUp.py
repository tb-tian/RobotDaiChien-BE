import random

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

def addPowerUp(board, listOfPowerUps: list, timeout: int) -> None:
    powerup_types = ['G', 'E', 'F']
    chosen_types = random.sample(powerup_types, 2)

    movable_cells = board.listMovableCells()

    for powerup_type in chosen_types:
        if not movable_cells: # No more available cells
            break
        x, y = random.choice(movable_cells)
        x, y = int(x), int(y)
        if findPowerUp(x, y, listOfPowerUps) == None:
            listOfPowerUps.append(PowerUp(x, y, powerup_type, timeout))
            movable_cells.remove((x,y))