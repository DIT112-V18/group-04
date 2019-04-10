import math


class Vector:

    def __init__(self, startCoordinate, endCoordinate):

        self.x = endCoordinate.getX() - startCoordinate.getX()
        self.y = endCoordinate.getY() - startCoordinate.getY()
        self.direction = self.calculateDirection()
        self.magnitude = self.calculateMagnitude()

    def calculateMagnitude(self):
        a = math.pow(self.x, 2)
        b = math.pow(self.y, 2)
        magnitude = math.sqrt(a+b)
        return magnitude

    def calculateDirection(self):
        # flipped x and y instead of subtracting pi/2
        direction = math.atan2(self.x, self.y)
        if direction < 0:
            direction += 2*math.pi

        return math.degrees(direction)
