import json
import os
import random
import subprocess
import sys
from tqdm import tqdm
from subprocess import STDOUT, check_output
from copy import deepcopy 
import psutil
import shutil

from player import *
from board import *
from listOfPlayers import *
from JSONlogger import *
from FileInteractor import *

def readGameBeforeStarting(path : str):
    inputFile = open(f"./map/{path}", "r")

    #print(f"./map/{path}")

    M, N, frequency = map(int, inputFile.readline().split())
    
    board = Board()
    board.setNumberOfRows(M)
    board.setNumberOfColumns(N)
    for x in range(M):
        board.setRow(x, "".join(inputFile.readline()[:-1].split()))
    
    inputFile.close()
    print("Reading configuration of the game before start is completed")
    return board, frequency

def readNames():
    map_file_path = input("Please type the path of the map file: ")
    number_of_players = int(input("Please type the number of team: "))
    names_of_teams = ["PATH_FILE" for i in range(number_of_players)]
    for i in range(number_of_players):
        names_of_teams[i] = input("Please enter name of team " + str(i) + ": ")
    return map_file_path, names_of_teams

def main():
    map_file_path, names_of_teams = readNames()
    logger = JSONlogger()
    board, frequency = readGameBeforeStarting(map_file_path)
    listOfPlayers = ListOfPlayers(len(names_of_teams)) 
    fileInterator = FileInteractor()
    stopGame = False
    turn = radius = 0
    lastPositions = [(-1, -1) for i in range(len(names_of_teams))]
    initialFiles = [[] for i in range(len(names_of_teams))]
    textLog = []
    
    #listOfPlayers.chooseStartingPositions(len(names_of_teams), board.numberOfRows, board.numberOfColumns)
    #for i in range(len(listOfPlayers)):
    #    board.setCell(listOfPlayers[i].getPositionX(), listOfPlayers[i].getPositionY(), str(listOfPlayers[i].getColor()))

    for i in range(len(names_of_teams)):
        if os.path.exists(f"./Players/{names_of_teams[i]}/"):
            for fileName in os.listdir(f"./Players/{names_of_teams[i]}/"):
                p = fileName.rfind('.')
                if (p < 0):
                    continue
                extenstion = fileName[p + 1:]
                #print(extenstion)
                
                if not (extenstion in ["dll", "exe", "py"]):
                    try:
                        os.remove(f"./Players/{names_of_teams[i]}/{fileName}")
                    except:
                        pass
                    continue

                initialFiles[i].append(fileName)
            #print(i)
            #print(os.listdir(f"./Players/{names_of_teams[i]}/"))
            
            initialFiles[i].append("STATE.DAT")
            initialFiles[i].append("MOVE.OUT")
            initialFiles[i].append("MAP.INP")

            #print(initialFiles[i])

    while not stopGame:

        if turn % 20 == 0:
            print(f"turn: {turn}")

        textLog.append(f"turn: {turn}\n")

        logger.addNewEvent(turn, listOfPlayers, frequency, stopGame, board, lastPositions)

        fileInterator.writeInputFilesForPlayers(listOfPlayers, board, frequency, turn, names_of_teams)
        
        for i in range(len(names_of_teams)):
            destinationPath = f"./Match/Players/{names_of_teams[i]}/turn{turn}/"
            sourcePath = f"./Players/{names_of_teams[i]}/"
            if not os.path.exists(destinationPath):
                os.makedirs(destinationPath)
            try:
                if os.path.exists(sourcePath + "MAP.INP"):
                    result = shutil.copyfile(sourcePath + "MAP.INP", destinationPath + "MAP.INP")
                    #print(result)
            except:
                pass

        for i in range(len(names_of_teams)):
            try:
                if os.path.exists(f'./Players/{names_of_teams[i]}/main.py'):
                    command = f'python main.py > log.txt'
                elif os.path.exists(f'./Players/{names_of_teams[i]}/main.exe'):
                    command = f'main.exe > log.txt'
                else:
                    raise Exception(f'[{names_of_teams[i]}][ERROR] No executable file found.')
                command = command.split()
                subprocess.check_call(command, timeout = 2, cwd = f"./Players/{names_of_teams[i]}/", shell = True, stderr=subprocess.STDOUT)
            except Exception as e:
                #print("This is an important warning!!! STOP THE PROGRAM!!!")
                #print(e)
                textLog.append(f"Error is encountered | {names_of_teams[i]}\n")
                textLog.append(f"{str(e)}\n")
                #exit(0)
        
        outputs = fileInterator.readOutputFilesOfPlayers(names_of_teams)

        for i in range(len(names_of_teams)):
            destinationPath = f"./Match/Players/{names_of_teams[i]}/turn{turn}/"
            sourcePath = f"./Players/{names_of_teams[i]}/"
            if not os.path.exists(destinationPath):
                os.makedirs(destinationPath)
            try:
                if os.path.exists(sourcePath + "MOVE.OUT"):
                    result = shutil.copyfile(sourcePath + "MOVE.OUT", destinationPath + "MOVE.OUT")
                    #print(result)
            except:
                pass

            try:
                if os.path.exists(sourcePath + "STATE.DAT"):
                    result = shutil.copyfile(sourcePath + "STATE.DAT", destinationPath + "STATE.DAT")
                    #print(result)
            except:
                pass

        if turn > 0:
            listOfPlayers.moveNext(outputs, board)
        else:
            listOfPlayers.chooseStartingPositions(outputs, board)

        for i in range(len(names_of_teams)):
            (x, y) = listOfPlayers[i].getPosition()
            if (x < 0) or (y < 0):
                continue
            lastPositions[i] = (x, y)

        cells = [[[] for y in range(board.getNumberOfColumns())] for x in range(board.getNumberOfRows())]

        for i in range(len(listOfPlayers)):
            if not listOfPlayers[i].checkAlive():
                continue
            x, y = listOfPlayers[i].getPosition()
            cells[x][y].append(i)

        for x in range(board.getNumberOfRows()):
            for y in range(board.getNumberOfColumns()):
                if board.checkUnmovable(x, y):
                    continue
                cell = board.getCell(x, y)
                numberOfPlayersOnCell = len(cells[x][y])
                if numberOfPlayersOnCell >= 2:
                    continue
                if numberOfPlayersOnCell == 1:
                    if cell != '.':
                        id = ord(cell) - ord('A')
                        listOfPlayers[id].increaseArea(-1)
                    id = cells[x][y][0]
                    cell = listOfPlayers[id].getColor()
                    listOfPlayers[id].increaseArea()
                    board.setCell(x, y, cell)
            
        numberOfColors = len(listOfPlayers)
        updatedArea = board.updateCoveredArea(numberOfColors)

        for i in range(len(listOfPlayers)):
            if listOfPlayers[i].getPosition() in updatedArea:
                listOfPlayers[i].getKilled()

        listOfPlayers.setArea(board.findColoredArea(numberOfColors))

        if (turn > 0) and ((turn % frequency == 0) or (listOfPlayers.countAlivePlayers() <= 0)):
            board.shrink(radius)
            radius += 1
            print("Board Shrinks!!!")
            for i in range(len(listOfPlayers)):
                x, y = listOfPlayers[i].getPosition()
                if board.checkUnmovable(x, y):
                    listOfPlayers[i].getKilled()

        turn += 1

        stopGame = board.checkGameStopped()

        for i in range(len(names_of_teams)):        
            if os.path.exists(f"./Players/{names_of_teams[i]}/"):
                for fileName in os.listdir(f"./Players/{names_of_teams[i]}/"):
                    if fileName in initialFiles[i]:
                        continue
                    try:
                        os.remove(f"./Players/{names_of_teams[i]}/{fileName}") 
                    except:
                        pass

    logger.addNewEvent(turn, listOfPlayers, frequency, stopGame, board, lastPositions)

    logger.writeFile("./Match/log.json")

    with open("./Match/log.txt", "w") as log:
        log.writelines(textLog)

if __name__ == "__main__":
    main()
