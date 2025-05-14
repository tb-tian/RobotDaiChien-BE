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
import argparse

from player import *
from board import *
from listOfPlayers import *
from JSONlogger import *
from FileInteractor import *
from powerUp import *

def readGameBeforeStarting(path : str):
    inputFile = open(f"./Map/{path}", "r")

    #print(f"./map/{path}")

    M, N, frequency = map(int, inputFile.readline().split())
    
    board = Board()
    board.setNumberOfRows(M)
    board.setNumberOfColumns(N)
    for x in range(M):
        board.setRow(x, "".join(inputFile.readline()[:-1].split()))

    print(M, N)
    print(board.grid)

    inputFile.close()
    print("Reading configuration of the game before start is completed")
    return board, frequency

def parseArguments():
    parser = argparse.ArgumentParser(description="CC25 simulator v3")
    parser.add_argument("map", help="Path to the map file, must be in Map directory")
    parser.add_argument("--players", "-p", nargs='+', help="A list of players name, must be in Players directory")
    parserArgs = parser.parse_args()
    return parserArgs.map, parserArgs.players

def main():
    map_file_path, names_of_teams = parseArguments()
    logger = JSONlogger()
    board, frequency = readGameBeforeStarting(map_file_path)
    listOfPlayers = ListOfPlayers(len(names_of_teams)) 
    fileInterator = FileInteractor()
    listOfPowerUps = []
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

    initialBoard = board.getGrid()

    while not stopGame:

        if turn % 20 == 0:
            print(f"turn: {turn}")

        textLog.append(f"turn: {turn}\n")

        logger.addNewEvent(turn, listOfPlayers, frequency, stopGame, board, lastPositions, listOfPowerUps,initialBoard)

        fileInterator.writeInputFilesForPlayers(listOfPlayers, board, frequency, turn, names_of_teams, listOfPowerUps)
        
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
            if not listOfPlayers[i].checkAlive():
                continue
            try:
                if os.path.exists(f'./Players/{names_of_teams[i]}/main.py'):
                    command = f'python main.py > log.txt'
                elif os.path.exists(f'./Players/{names_of_teams[i]}/main.exe'):
                    command = f'main.exe > log.txt'
                elif os.path.exists(f'./Players/{names_of_teams[i]}/main'):
                    command = f'./main > log.txt'
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
                PROC_NAME = [f"main.exe", 'procgov64.exe']

                for proc in psutil.process_iter():
                    # check whether the process to kill name matches
                    #print(proc.name())
                    if proc.name() in PROC_NAME:
                        proc.kill()
                        #pass
        
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

        cellsAffectedByE = {}
        cellsAffectedByTangToc = {}
        for x in range(board.getNumberOfRows()):
            for y in range(board.getNumberOfColumns()):
                if board.checkUnmovable(x, y):
                    continue
                cell = board.getCell(x, y)
                numberOfPlayersOnCell = len(cells[x][y])
                if numberOfPlayersOnCell >= 2:
                    continue
                if numberOfPlayersOnCell == 1:
                    if cell == '.' and listOfPlayers[cells[x][y][0]].getPowerUp() != 'tangtoc':
                        id = cells[x][y][0]
                        cell_char_to_set = listOfPlayers[id].getColor() 
                        listOfPlayers[id].increaseArea()
                        board.setCell(x, y, cell_char_to_set)
                    if cell == 'A' or cell == 'B' or cell == 'C'or cell == 'D' and listOfPlayers[cells[x][y][0]].getPowerUp() != 'tangtoc':
                        id = ord(cell) - ord('A')
                        listOfPlayers[id].increaseArea(-1)
                        id = cells[x][y][0]
                        cell_char_to_set = listOfPlayers[id].getColor() 
                        listOfPlayers[id].increaseArea()
                        board.setCell(x, y, cell_char_to_set)
                    
                    if listOfPlayers[cells[x][y][0]].getPowerUp() == 'tangtoc':
                        id = cells[x][y][0]
                        if (x,y) not in cellsAffectedByTangToc:
                            cellsAffectedByTangToc[(x,y)] = []
                        cellsAffectedByTangToc[(x,y)].append(id)

                    if findPowerUp(x, y, listOfPowerUps) == 'G' and listOfPlayers[cells[x][y][0]].checkActivePowerUp() == False:
                        id = cells[x][y][0]
                        listOfPlayers[id].setTangToc()
                        deletePowerUp(x, y, listOfPowerUps)
                    
                    if findPowerUp(x, y, listOfPowerUps) == 'E' and listOfPlayers[cells[x][y][0]].checkActivePowerUp() == False:
                        id_player_on_E = cells[x][y][0]
                        player_color = listOfPlayers[id_player_on_E].getColor()
                        for i in range(max(0, x - 1), min(board.getNumberOfRows(), x + 2)):
                            for j in range(max(0, y - 1), min(board.getNumberOfColumns(), y + 2)):
                                if board.checkUnmovable(i, j):
                                    continue

                                # Check if another player is on the cell (i,j)
                                if len(cells[i][j]) > 0:
                                    if cells[i][j][0] != id_player_on_E : # if there is a player and it's not the current player
                                        listOfPlayers[cells[i][j][0]].getKilled()
                                
                                if (i,j) not in cellsAffectedByE:
                                    cellsAffectedByE[(i,j)] = []
                                cellsAffectedByE[(i,j)].append(id_player_on_E)

                                # current_cell_type = board.getCell(i,j)
                                # if 'A' <= current_cell_type <= 'D': # If it's another player's territory
                                #     owner_id = ord(current_cell_type) - ord('A')
                                #     if owner_id != id_player_on_E: # and not the current player's own territory
                                #         listOfPlayers[owner_id].increaseArea(-1)

                                # board.setCell(i, j, player_color)
                                # listOfPlayers[id_player_on_E].increaseArea() # Increase area for the player who stepped on E
                        
                        deletePowerUp(x, y, listOfPowerUps)

                    if findPowerUp(x, y, listOfPowerUps) == 'F' and listOfPlayers[cells[x][y][0]].checkActivePowerUp() == False:
                        id = cells[x][y][0]
                        listOfPlayers[id].setDauTron()
                        deletePowerUp(x, y, listOfPowerUps)

                        cell_char_to_set = listOfPlayers[id].getColor() 
                        listOfPlayers[id].increaseArea()
                        board.setCell(x, y, cell_char_to_set)
                    
            
        # Apply E cell effects
        for pos, player_ids in cellsAffectedByE.items():
            i, j = pos
            # If cell is affected by exactly one player
            if len(player_ids) == 1:
                id_player_on_E = player_ids[0]
                player_color = listOfPlayers[id_player_on_E].getColor()
                
                # Check if another player is on the cell
                if len(cells[i][j]) > 0 and cells[i][j][0] != id_player_on_E:
                    listOfPlayers[cells[i][j][0]].getKilled()
                
                current_cell_type = board.getCell(i, j)
                if 'A' <= current_cell_type <= 'D':  # If it's another player's territory
                    owner_id = ord(current_cell_type) - ord('A')
                    if owner_id != id_player_on_E:  # and not the current player's own territory
                        listOfPlayers[owner_id].increaseArea(-1)
                
                board.setCell(i, j, player_color)
                listOfPlayers[id_player_on_E].increaseArea()
            # else: cell is affected by multiple players, leave it as is

        # Track and resolve TangToc intermediate cells
        for i in range(len(listOfPlayers)):
            if not listOfPlayers[i].checkAlive():
                continue
                
            midCell = listOfPlayers[i].getLastMidCell()
            if midCell is not None:
                if midCell not in cellsAffectedByTangToc:
                    cellsAffectedByTangToc[midCell] = []
                cellsAffectedByTangToc[midCell].append(i)
                
        # Apply TangToc mid-cell effects
        for pos, player_ids in cellsAffectedByTangToc.items():
            x, y = pos
            # If cell is affected by exactly one player
            if len(player_ids) == 1:
                player_id = player_ids[0]
                player_color = listOfPlayers[player_id].getColor()
                
                current_cell_type = board.getCell(x, y)
                if 'A' <= current_cell_type <= 'D':  # If it's another player's territory
                    owner_id = ord(current_cell_type) - ord('A')
                    if owner_id != player_id:  # and not the current player's own territory
                        listOfPlayers[owner_id].increaseArea(-1)
                
                board.setCell(x, y, player_color)
                listOfPlayers[player_id].increaseArea()
            # else: cell is affected by multiple players, leave it as is

        numberOfColors = len(listOfPlayers)
        updatedArea = board.updateCoveredArea(numberOfColors)

        # Update powerup timeout
        updatePowerUpTimeout(listOfPowerUps)

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

        if turn % frequency == 0:
            addPowerUp(board, listOfPowerUps, frequency, listOfPlayers)

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

    logger.addNewEvent(turn, listOfPlayers, frequency, stopGame, board, lastPositions, listOfPowerUps, initialBoard)

    
    map_name = os.path.splitext(map_file_path)[0]
    logname = f"{map_name}_" + "_".join(names_of_teams)
    logger.writeFile(f"./Match/{logname}.json")
    
    with open(f"./Match/{logname}.txt", "w") as log:
        log.writelines(textLog)

if __name__ == "__main__":
    main()
