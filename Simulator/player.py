from board import *
import math

class Player:
    def __init__(self, x : int, y : int, alive : bool = True, area : int = 0, color : str = "?"):
        self.x = x
        self.y = y
        self.alive = alive
        self.area = area
        self.color = color
        self.tangtoc = 0
        self.dautron = 0 # New attribute for F power-up
        self.powerup = None
        self.lastMidCell = None
    
    def getColor(self) -> str:
        return self.color

    def toDictionary(self) -> dict:
        result = dict()
        result["position"] = {"x" : self.x, "y" : self.y}
        result["alive"] = self.alive
        result["area"] = self.area
        result["color"] = self.color
        result["tangtoc"] = self.tangtoc
        result["dautron"] = self.dautron # Add dautron to dictionary
        result["powerup"] = self.powerup
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

    def checkActivePowerUp(self) -> bool:
        return self.powerup != None
    
    def getPowerUp(self) -> str:
        return self.powerup

    def setTangToc(self) -> None:
        self.tangtoc = 5
        self.dautron = 0 # Reset dautron if tangtoc is activated
        self.powerup = "tangtoc"
    
    def resetTangToc(self) -> None:
        self.tangtoc = 0
        if self.powerup == "tangtoc": # Only nullify powerup if it was tangtoc
            self.powerup = None

    def setDauTron(self) -> None: # New method for F power-up
        self.dautron = 5
        self.tangtoc = 0 # Reset tangtoc if dautron is activated
        self.powerup = "dautron"

    def resetDauTron(self) -> None: # New method for F power-up
        self.dautron = 0
        if self.powerup == "dautron": # Only nullify powerup if it was dautron
            self.powerup = None

    def getLastMidCell(self) -> tuple:
        return getattr(self, 'lastMidCell', None)

    def moveNext(self, intendedPosition, board = None) -> tuple:
        if not self.alive:
            return (-1, -1)
        
        try:
            nextX, nextY = intendedPosition
            # Validate input type for intendedPosition
            if not (isinstance(nextX, int) and isinstance(nextY, int)):
                # If input is invalid, decrement timers if they are active, then return current position
                if self.powerup == "tangtoc" and self.tangtoc > 0:
                    self.tangtoc -=1
                    if self.tangtoc == 0: self.resetTangToc()
                if self.powerup == "dautron" and self.dautron > 0:
                    self.dautron -=1
                    if self.dautron == 0: self.resetDauTron()
                return (self.x, self.y)

            if (self.x < 0) or (self.y < 0): # Player not on board (e.g. previously killed)
                return (-1, -1)

            bound = board.getShape()
            prev_x, prev_y = self.x, self.y

            # --- Power-up Timers Update ---
            dautron_active_at_turn_start = (self.powerup == "dautron" and self.dautron > 0)
            tangtoc_active_at_turn_start = (self.powerup == "tangtoc" and self.tangtoc > 0)

            if dautron_active_at_turn_start:
                self.dautron -= 1
            if tangtoc_active_at_turn_start:
                self.tangtoc -= 1
            
            # --- Move Calculation & Validation ---
            dx_abs = abs(self.x - nextX)
            dy_abs = abs(self.y - nextY)
            dist = dx_abs + dy_abs

            if dist == 0: # No actual move attempted
                if dautron_active_at_turn_start and self.dautron == 0: self.resetDauTron()
                if tangtoc_active_at_turn_start and self.tangtoc == 0: self.resetTangToc()
                return (self.x, self.y)

            # 1. DauTron Special Move: step onto a '#' cell
            if dautron_active_at_turn_start:
                if dist == 1: # Must be a 1-step move
                    # Check bounds before accessing board cell content
                    if not ((nextX < 0) or (nextY < 0) or (nextX >= bound[0]) or (nextY >= bound[1])):
                        # DauTron allows moving specifically onto a '#' cell
                        # Assuming board.getCell(x, y) returns the character at that cell.
                        if board.getCell(nextX, nextY) == '#':
                            self.x = nextX
                            self.y = nextY
                            self.resetDauTron() 
                            if tangtoc_active_at_turn_start and self.tangtoc == 0 and self.powerup != "tangtoc":
                                 self.resetTangToc()
                            return (self.x, self.y)
            
            # 2. Validate move distance (TangToc or normal)
            max_allowed_dist = 1
            is_valid_tangtoc_path = False
            if tangtoc_active_at_turn_start:
                max_allowed_dist = 2
                if (dx_abs <= max_allowed_dist and dy_abs == 0) or \
                   (dx_abs == 0 and dy_abs <= max_allowed_dist): # Straight line for 1 or 2 steps
                    is_valid_tangtoc_path = True
                else: 
                    if dautron_active_at_turn_start and self.dautron == 0: self.resetDauTron()
                    if tangtoc_active_at_turn_start and self.tangtoc == 0: self.resetTangToc()
                    return (prev_x, prev_y) # Invalid path for TangToc
            
            if dist > max_allowed_dist:
                if dautron_active_at_turn_start and self.dautron == 0: self.resetDauTron()
                if tangtoc_active_at_turn_start and self.tangtoc == 0: self.resetTangToc()
                return (prev_x, prev_y) # Moved too far

            # 3. Board boundary check
            if (nextX < 0) or (nextY < 0) or (nextX >= bound[0]) or (nextY >= bound[1]):
                if dautron_active_at_turn_start and self.dautron == 0: self.resetDauTron()
                if tangtoc_active_at_turn_start and self.tangtoc == 0: self.resetTangToc()
                return (prev_x, prev_y)

            # 4. Target cell unmovable (DauTron special move already handled this possibility)
            if board.checkUnmovable(nextX, nextY):
                if dautron_active_at_turn_start and self.dautron == 0: self.resetDauTron()
                if tangtoc_active_at_turn_start and self.tangtoc == 0: self.resetTangToc()
                return (prev_x, prev_y)

            # 5. TangToc mid-cell processing (if it's a 2-step TangToc move)
            if tangtoc_active_at_turn_start and is_valid_tangtoc_path and dist == 2:
                midX, midY = -1, -1
                if dx_abs == 2: midX, midY = (self.x + nextX) // 2, self.y
                elif dy_abs == 2: midX, midY = self.x, (self.y + nextY) // 2
                
                if midX != -1: 
                    if board.checkUnmovable(midX, midY):
                        if dautron_active_at_turn_start and self.dautron == 0: self.resetDauTron()
                        if tangtoc_active_at_turn_start and self.tangtoc == 0: self.resetTangToc()
                        return (prev_x, prev_y) 
                    self.lastMidCell = (midX, midY)
                    # board.setCell(midX, midY, self.color) 
                else:
                    self.lastMidCell = None
            else:
                self.lastMidCell = None

            # --- Finalize Move ---
            self.x = nextX
            self.y = nextY

            # --- Post-Move Timer Expiry Check ---
            if dautron_active_at_turn_start and self.dautron == 0 and self.powerup == "dautron":
                self.resetDauTron()
            if tangtoc_active_at_turn_start and self.tangtoc == 0 and self.powerup == "tangtoc":
                self.resetTangToc()
            
        except Exception as e:
            print(f"Exception caught in moveNext for player {self.color}: {e}")
            # Fallback: attempt to reset timers if they might have expired due to an error state
            # and return current known position or default if attributes are missing.
            current_x = getattr(self, 'x', -1)
            current_y = getattr(self, 'y', -1)
            if hasattr(self, 'powerup') and self.powerup == "dautron" and hasattr(self, 'dautron') and self.dautron == 0: self.resetDauTron()
            if hasattr(self, 'powerup') and self.powerup == "tangtoc" and hasattr(self, 'tangtoc') and self.tangtoc == 0: self.resetTangToc()
            return (current_x, current_y)

        return (self.x, self.y)