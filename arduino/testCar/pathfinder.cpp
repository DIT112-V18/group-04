#include "pathfinder.h"

/**
 * Constructor of a PathFinder car using a HeadingCar and a Bluetooth connection
 * for control and communication.
 * 
 * @param car        HeadingCar used for steering the PathFinder
 * @param blue       Bluetooth connection used for communication
 * @param leftOdo    Pointer to the left odometer installed on the car
 * @param rightOdo   Pointer to the right odometer installed on the car
 * @param ultrasound Pointer to the distance sensor installed on the car
 * @param pos        initial position of the PathFinder
 * @param speed      speed used when driving
 */
PathFinder::PathFinder(const HeadingCar& car, const HardwareSerial *blue, const DirectionlessOdometer *leftOdo, const DirectionlessOdometer *rightOdo, const SR04 *ultrasound, Point pos, int speed=SPEED) :
    mCar(car),  
    mPos(pos.getX(), pos.getY()),
    mPrev(pos.getX(), pos.getY()) {    
      mConnection = blue;
      mLeftOdo = leftOdo;
      mRightOdo = rightOdo;  
      mFrontDist = ultrasound;
      mSpeed = smartcarlib::utils::getAbsolute(speed);
    }


/**
 * Initialise the car. This function is used to implement behaviour that should 
 * be executed once after creation.
 */
void PathFinder::init() {
  // initialise the serial connection
  mConnection->begin(BAUD_RATE);
  
  #ifdef DEBUG
  Serial.begin(BAUD_RATE);    // initialise serial connection for usb-debug
  #endif
}

/**
 * Function to update the state of the PathFinder. Should be called repeatedly.
 * 
 * Polling this function allows to determine whether the PathFinder has completed
 * it's job and can be stopped until the next destination is set.
 */
void PathFinder::update() {
  mCar.update();     // update to integrate the latest heading sensor readings
  mHeading = mCar.getHeading();   // in the scale of 0 to 360
  
  readSerial();

  if (!checkDist()) {
    if (mTurn) {
      // stop turning if the heading is in an acceptable range
      int diff =  abs(mTargetHeading - mHeading);
      if (diff < ANGLE_TOLERANCE || diff > DEG_IN_CIRCLE - ANGLE_TOLERANCE) {
        mCar.setSpeed(0);
        mTurn = false;
        if (mDrive) {
          moveForward(mTargetDistance);        
        }
      }
      
    } else if (mDrive) {
      // stop heading forward if the required distance has been passed
      updatePosition();    
  
      if (mDistance > mTargetDistance) {
        Point p = mPath[mReadPosition - 1];
        mPos.set(p.getX(), p.getY());
        stopCar();
        publishPosition();
        mPublishPos = false;
      }
      
    } else {
      // set the next goal if the current goal has been reached
      setNextGoal();
    }
  }

  #ifdef DEBUG
  if (mPublishPos == true) {
    updatePosition();
    publishPosition();
    mPublishPos = false;    
  }
  #endif
}

/**
 * Calculate the current position of the PathFinder
 */
void PathFinder::updatePosition() {
  mDistance = mRightOdo->getDistance() + mLeftOdo->getDistance();
  mDistance *= SCALE;
  double radHead = ((double) mTargetHeading) * M_PI / 180.0;

  double dist = (double) mDistance;
  dist *= 0.5;
  double dx = dist * sin(radHead); 
  double dy = dist * cos(radHead);

  double x = mPrev.getX() + dx;
  double y = mPrev.getY() + dy;
  
  mPos.set(x, y);
}

/**
 * Check the current reading of the ultrasound sensor and react to it
 * 
 * @return    boolean specifying whether there is a proximity alert
 */
bool PathFinder::checkDist() {
  int dist = mFrontDist->getDistance();
  if (!mProximityAlert) {
    // stop  car and set alert if necessary
    if (dist < US_SAFETY_DIST && dist > 0) {
      int speed = STOP_SPEED;
      mCar.setSpeed(speed);
      mProximityAlert = true;
    }
  } else {
    if (dist > US_SAFETY_DIST) {
      if (mHasDestination) {
        goToPoint(mDestination);
      }
      mProximityAlert = false;
    }
  }
  return mProximityAlert;
}

/**
 * Publish the current position of the PathFinder
 */
void PathFinder::publishPosition() {
  int x = (int) (mPos.getX() + 0.5);
  int y = (int) (mPos.getY() + 0.5);
  char outString [BUFFER_SIZE];
  sprintf(outString, "%c%d%c%d%c\n", APPENDER, x, SEPARATOR, y, CLOSER);
  mConnection->print(outString);
}

/**
 * Read and process incoming data from the serial connection
 */
void PathFinder::readSerial() {
  while (mConnection->available() > 0) {
    char data = mConnection->read();
    if (data == '\n' || mPosition > BUFFER_SIZE-1) {
      parseCommand(mBuffer, mPosition);
      memset(&mBuffer[0], 0, sizeof(mBuffer));    // erase the buffer's content
      mPosition = 0;    // reset pointer
    } else {
      // extend the buffer with the incoming data
      mBuffer[mPosition] = data;
      mPosition++;
    }
  }
}

/**
 * Given a command in the format defined by our group 
 * for sending information from the server to the app,
 * find out what is expected to be done by the car.
 * 
 * @param command   pointer to the char array containing the command
 * @param length    length of the command contained in the char array
 */
void PathFinder::parseCommand(char *command, int length) {
  String str(command);
  char first = command[0];
  Serial.println(str);
  if (first == 'F') {
    if (length == 4 && strcmp(CLEAR_CMD, command) == 0) {
      clearPath();
    }
  }
  if (first == APPENDER) {
    char last = str.charAt(length - 1);
    Serial.println(last);
    if (last == CLOSER) {
      // remove starter and ender of the command
      String sub;
      for (int i = 0; i < length - 2; i++) {
        sub += command[i+1];
      }

      // split the command by the delimiter
      int delimiter = sub.indexOf(SEPARATOR);
      String xstr;
      String ystr;

      xstr = sub.substring(0, delimiter);
      ystr = sub.substring(delimiter+1);
      
      // parse to int
      int x = xstr.toInt();
      int y = ystr.toInt();

      Serial.println(x);
      Serial.println(y);

      addPoint(Point(x, y));
      //delete[] sub;
    }
  }
}

/**
 * Rotate the car on spot at the specified degrees with the certain speed
 * @param degrees   The degrees to rotate on spot. Positive values for clockwise
 *                  negative for counter-clockwise.
 */
void PathFinder::rotateOnSpot(int targetDegrees) {
  targetDegrees %= 360; //put it on a (-360,360) scale
  if (!targetDegrees) return; //if the target degrees is 0, don't bother doing anything
  /* Let's set opposite speed on each side of the car, so it rotates on spot */
  if (targetDegrees > 0) { //positive value means we should rotate clockwise
    mCar.overrideMotorSpeed(mSpeed, -mSpeed); // left motors spin forward, right motors spin backward
  } else { //rotate counter clockwise
    mCar.overrideMotorSpeed(-mSpeed, mSpeed); // left motors spin backward, right motors spin forward
  }
  unsigned int initialHeading = mCar.getHeading(); //the initial heading we'll use as offset to calculate the absolute displacement
  int degreesTurnedSoFar = 0; //this variable will hold the absolute displacement from the beginning of the rotation
  while (abs(degreesTurnedSoFar) < abs(targetDegrees)) { //while absolute displacement hasn't reached the (absolute) target, keep turning 
    int currentHeading = getHeading(); 
    if ((targetDegrees < 0) && (currentHeading > initialHeading)) { //if we are turning left and the current heading is larger than the
      //initial one (e.g. started at 10 degrees and now we are at 350), we need to substract 360, so to eventually get a signed
      currentHeading -= 360; //displacement from the initial heading (-20)
    } else if ((targetDegrees > 0) && (currentHeading < initialHeading)) { //if we are turning right and the heading is smaller than the
      //initial one (e.g. started at 350 degrees and now we are at 20), so to get a signed displacement (+30)
      currentHeading += 360;
    }
    degreesTurnedSoFar = initialHeading - currentHeading; //degrees turned so far is initial heading minus current (initial heading
    //is at least 0 and at most 360. To handle the "edge" cases we substracted or added 360 to currentHeading)
  }
  mCar.setSpeed(0); //we have reached the target, so stop the car
}


/**
 * Rotate the car on spot to the specified heading using the internal speed.
 * 
 * Do this without blocking the thread (no loop)
 * @param targetHeading   The final heading to rotate to on spot. Calculate whether to rotate 
 *                        clockwise or counter-clockwise depending on the current heading.
 */
void PathFinder::rotateToHeading(int targetHeading) {
  mCar.setSpeed(0);    // make sure to stop the car initially
  targetHeading = trimHeading(targetHeading);   // trim the heading into the desired range
  int currentHeading = getHeading();

  int diff = targetHeading - currentHeading;
  // always turn the shortest direction
  if (diff > 180) {   
    diff -= DEG_IN_CIRCLE;      
  } else if (diff < -180) {
    diff += DEG_IN_CIRCLE;
  }

  // set the car to turning mode
  mTurn = true;
  mTargetHeading = targetHeading;

  // set the motors 
  if (diff > 0) { //positive value means we should rotate clockwise
    mCar.overrideMotorSpeed(mSpeed, -mSpeed); // left motors spin forward, right motors spin backward
  } else { //rotate counter clockwise
    mCar.overrideMotorSpeed(-mSpeed, mSpeed); // left motors spin backward, right motors spin forward
  }
}

/**
 * Make the car move forward in a straight line using the internal speed.
 * 
 * Do this witout blocking th thread (no loop)
 * @param distance    The distance for which the car is supposed to move forward.
 */
void PathFinder::moveForward(int distance) {
  // reset odometers
  mRightOdo->reset();
  mLeftOdo->reset();
  
  // set the car to driving
  mDrive = true;
  int currentDist = mRightOdo->getDistance() + mLeftOdo->getDistance();
  mTargetDistance = currentDist + 2*distance;  // save twice the distance to check with the simple sum of the odometers later
  mCar.setSpeed(mSpeed);
}

/**
 * Make the PathFinder go to the desired destination.
 * 
 * @param destination   The point which the PathFinder is supposed to go to
 */
void PathFinder::goToPoint(Point destination) {
  mDestination = destination;
  mHasDestination = true;
  
  // calculate angle and turn needed for this operation
  double dx = destination.getX() - mPos.getX();
  double dy = destination.getY() - mPos.getY();
  int targetHeading = (int) (atan2(dx, dy) * 180.0 / M_PI + 0.5);       // switch x and y to count clockwise from North
  
  double targetDistance = sqrt(pow(dx, 2) + pow(dy, 2));

  // initiate the journey to the target
  rotateToHeading(targetHeading);
  mTargetDistance = (int) targetDistance;
  mDrive = true;
}

/**
 * Stop the car
 */
void PathFinder::stopCar() {
  mCar.setSpeed(0);
  mTurn = false;
  mDrive = false;
  mDistance = 0;
  mPrev.set(mPos.getX(), mPos.getY());
}


/**
 * Function to delete the current path of the path-finder.
 */
void PathFinder::clearPath() {
  stopCar();
  
  for (int i = 0; i < MAX_PATH_LENGTH; i++) {
    mPath[i] = Point(0, 0);
  }
  mReadPosition = 0;
  mWritePosition = 0;  
  mHasDestination = false;
}

/**
 * Add a point to the end of the path.
 * 
 * @param *point    pointer to the Point that should be appended
 */
void PathFinder::addPoint(const Point point) {
  if (mWritePosition < MAX_PATH_LENGTH) {
    mPath[mWritePosition] = point;
    mWritePosition++;
  } else {
    #ifdef DEBUG
    mConnection->println("The path has maximum length. No more points can be added to it.");
    #endif
  }
}

/**
 * Set the PathFinder to go to the next point from the list if available.
 * 
 * This function sets up the next turn and drive if there are still further points in the path.
 */
void PathFinder::setNextGoal() {
  // check whether destinations are left unhandled and go there if so
  if (mReadPosition < mWritePosition && !mDrive && !mTurn) {

    #ifdef DEBUG
    mConnection->println("Setting next goal!");
    #endif
    
    Point point = mPath[mReadPosition];
    mReadPosition++;
    goToPoint(point);
  }
}


/**
 * Method to move any heading [deg] into the allowed range (0 -- DEG_IN_CIRCLE)
 * 
 * This method is not specific to the path finder but rather a mathematical 
 * operation on our coordinate system.
 * @param heading     Heading value in degrees before trimming
 */ 
int trimHeading(int heading) {
  //TODO: migrate this function into a class defining the used coordinate system 
  // move negative heading into the range 0 -- DEG_IN_CIRCLE
  // modulo does not do this as desired
  while (heading < 0) {
    heading = heading + DEG_IN_CIRCLE;
  }

  // if the heading is too big, bring it back into the desired range
  while (heading >= DEG_IN_CIRCLE) {
    heading = heading - DEG_IN_CIRCLE;
  }

  return heading;
}
