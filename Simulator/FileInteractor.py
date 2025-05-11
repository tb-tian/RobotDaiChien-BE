from player import *
from board import *
from listOfPlayers import *
from JSONlogger import *

class FileInteractor:
    def __init__(self):
        pass
    def writeInputFilesForPlayers(self,
                                  players : ListOfPlayers, 
                                  board : Board, 
                                  frequency : int, 
                                  turn : int,
                                  names_of_teams : list,
                                  list_of_powerup: list):
    
        M = board.getNumberOfRows()
        N = board.getNumberOfColumns()
        P = len(players) - 1

        for i in range(P + 1):
            try:
                with open(f"./Players/{names_of_teams[i]}/MAP.INP", "w") as outputFile:
                    lines = list()
                    lines.append(f"{M} {N} {frequency} {turn}\n")
                    
                    X = players[i].getPositionX()
                    Y = players[i].getPositionY()
                    C = players[i].getColor()

                    lines.append(f"{X} {Y} {C}\n")

                    lines.append(f"{P}\n")

                    for j in range(len(players)):
                        if i == j:
                            continue
                        X = players[j].getPositionX()
                        Y = players[j].getPositionY()
                        C = players[j].getColor()
                        lines.append(f"{X} {Y} {C}\n")
                    for line in board.grid:
                        lines.append(" ".join(list(line)) + "\n")

                    lines.append(f"{len(list_of_powerup)}\n")

                    for j in range(len(list_of_powerup)):
                        lines.append(f"{list_of_powerup[j].x} {list_of_powerup[j].y} {list_of_powerup[j].type}\n")

                    outputFile.writelines(lines)
            except:
                pass

    def readOutputFilesOfPlayers(self, names_of_teams : list):
        line = None
        outputs = []
        
        for i in range(len(names_of_teams)):
            try:
                with open(f"./Players/{names_of_teams[i]}/MOVE.OUT", "r") as outputFile:
                    line = outputFile.readline()
                    outputs.append(tuple(map(int, line[:-1].split(" "))))
            except Exception as e:
                outputs.append((-1, -1))

        return outputs
