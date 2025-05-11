from player import *
from board import *
from listOfPlayers import *
from JSONlogger import *
from powerUp import *
import copy
import json

class JSONlogger:
    def __init__(self):
        self.json_content = dict()

    def writeFile(self, path : str, indent : int = 5):
        with open(path, "w") as outputFile:
            #print(json.dumps(self.json_content, indent = indent))
            outputFile.write(json.dumps(self.json_content, indent = indent))

    def addNewEvent(self, 
                    turn : int, 
                    listOfPlayers : ListOfPlayers,
                    frequency : int,
                    stopGame : bool,
                    board : Board,
                    lastPositions : list,
                    listOfPowerUps: list, 
                    initialBoard = None):
        print(f"turn {turn} is added to logger")
        key = str(turn)
        self.json_content[key] = dict()
        self.json_content[key]["players"] = copy.deepcopy(listOfPlayers.toDictionary())

        for i in range(len(lastPositions)):
            self.json_content[key]["players"][str(i)]["position"] = {"x" : lastPositions[i][0], "y" : lastPositions[i][1]}

        self.json_content[key]["frequency"] = copy.deepcopy(frequency)
        self.json_content[key]["stopGame"] = copy.deepcopy(stopGame)
        self.json_content[key]["map"] = copy.deepcopy(board.toDictionary(initialBoard)) 
        self.json_content[key]["powerups"] = [powerup.toDictionary() for powerup in listOfPowerUps]
        
