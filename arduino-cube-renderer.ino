#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);

//function/class declarations
class Vec3f
{
public:
  float x;
  float y;
  float z;

  Vec3f(float x, float y, float z);
  Vec3f(void);
  Vec3f operator+(Vec3f addend);
  Vec3f operator-(Vec3f subtrahend);
  Vec3f operator*(float factor);
  Vec3f operator*(int factor);
  Vec3f operator+=(Vec3f addend);
  Vec3f operator-=(Vec3f subtrahend);
};

class Vec3i
{
  public:
  int x;
  int y;
  int z;

  Vec3i(int x, int y, int z);
  Vec3i(void);
};

void initRenderer(void);
void drawCube(void);
Vec3i castPointToScreen(Vec3f);
bool coordsOnScreen(Vec3i);
bool coordsInFrontOfScreen(Vec3i);
Vec3f rotatePointAboutXAxis(Vec3f,float);
Vec3f rotatePointAboutZAxis(Vec3f,float);
void handleInput(void);


//globals
const bool gFlippedJoystick = true;
const int gScreenWidth = 128;
const int gScreenHeight = 64;
const float gCubeSize = 0.7f;
const int gFOV = 90;
float gFocalDistance = 1.f;
float gViewWidth;
float gViewHeight;
Vec3f gViewPos = Vec3f(0,-1,1);
Vec3f gViewAngles = Vec3f(-30,0,0);
int gLastMillis = 0;
int gLastFrameTime = 0;
int gJoystickMode = 0; // 0 for looking, 1 for movement, 2 for cube rotation/vertical movement
const int gJoystickCentreX = 2015;
const int gJoystickCentreY = 1970;
const int gJoystickDeadzone = 30;
const int gMovementSpeed = 3; //units per second
const int gLookSpeed = 270; //degrees per second
bool gLastButtonState = false;
float gCubeRotation = 0.f;
float gCubeHeight = 0.f;

//pin definitions
const int joystickX = 25;
const int joystickY = 33;
const int joystickButton = 32;

void setup(void) {
  u8g2.begin();
  Serial.begin(115200);

  pinMode(joystickX, INPUT);
  pinMode(joystickY, INPUT);
  pinMode(joystickButton, INPUT_PULLUP);

  initRenderer();
}

void loop(void) {
  //calculate the time the last frame took to run in ms
  int ms = millis();
  gLastFrameTime = ms-gLastMillis;
  gLastMillis = ms;
  
  handleInput();
  
  u8g2.clearBuffer();
  drawCube();
  u8g2.sendBuffer();

  //print fps
  Serial.println(1000.f/gLastFrameTime);
}

Vec3f::Vec3f(float x, float y, float z)
{
  this->x = x;
  this->y = y;
  this->z = z;
}

Vec3f::Vec3f(void)
{
  Vec3f(0,0,0);
}

Vec3f Vec3f::operator+(Vec3f addend)
{
  return Vec3f(x+addend.x, y+addend.y, z+addend.z);
}
  
 Vec3f Vec3f::operator-(Vec3f subtrahend)
{
  return Vec3f(x-subtrahend.x, y-subtrahend.y, z-subtrahend.z);
}

Vec3f Vec3f::operator*(float factor)
{
  return Vec3f(x*factor, y*factor, z*factor);
}

Vec3f Vec3f::operator*(int factor)
{
  return Vec3f(x*factor, y*factor, z*factor);
}

Vec3f Vec3f::operator+=(Vec3f addend)
{
  x += addend.x;
  y += addend.y;
  z += addend.z;
  return *this;
}

Vec3f Vec3f::operator-=(Vec3f subtrahend)
{
  x -= subtrahend.x;
  y -= subtrahend.y;
  z -= subtrahend.z;
  return *this;
}

Vec3i::Vec3i(int x, int y, int z)
{
  this->x = x;
  this->y = y;
  this->z = z;
}

Vec3i::Vec3i(void)
{
  Vec3i(0,0,0);
}

void initRenderer(void)
{
  gViewWidth = 2 * gFocalDistance * tan(M_PI/360 * gFOV);
  gViewHeight = gViewWidth * gScreenHeight/gScreenWidth;

  Serial.print("Screen Dimensions: ");
  Serial.print(gScreenWidth);
  Serial.print("x");
  Serial.println(gScreenHeight);

  Serial.print("FOV: ");
  Serial.println(gFOV);

  Serial.print("Focal Distance: ");
  Serial.println(gFocalDistance);

  Serial.print("View Window: ");
  Serial.print(gViewWidth);
  Serial.print("x");
  Serial.println(gViewHeight);

  Serial.print("Cube Size: ");
  Serial.println();
}

void handleInput(void)
{
  //read the button
  bool buttonState = !digitalRead(joystickButton);
  //on initial button press
  if (buttonState && !gLastButtonState)
  {
    //switch the joystick mode
    gJoystickMode += 1;

    if (gJoystickMode > 2)
      gJoystickMode = 0;
  }
  gLastButtonState = buttonState;

  //read the joystick values, if they are out of the deadzone,
  //adjust their value to be centred around 0, otherwise,
  //set their value to 0 to avoid stick drift
  int xValue = analogRead(joystickX);
  if (abs(xValue-gJoystickCentreX) >= 20)
  {
    xValue -= gJoystickCentreX;
  }
  else
  {
    xValue = 0;
  }

  int yValue = analogRead(joystickY);
  if (abs(yValue-gJoystickCentreY) >= 20)
  {
    yValue -= gJoystickCentreY;
  }
  else
  {
    yValue = 0;
  }

  //invert the x and y values if the joystick is physically layed out that way
  if (gFlippedJoystick)
  {
    xValue *= -1;
    yValue *= -1;
  }

  if (gJoystickMode == 0)
  {
    //adjust view angles using joystick

    Vec3f look = Vec3f(gLookSpeed*(float)yValue/gJoystickCentreY*gLastFrameTime/1000, gLookSpeed*(float)xValue/gJoystickCentreX*gLastFrameTime/1000, 0);
    
    gViewAngles += look;

    //clamp pitch values to [-90,90]
    if (gViewAngles.x > 90)
      gViewAngles.x = 90;
    if (gViewAngles.x < -90)
      gViewAngles.x = -90;
  }
  else if (gJoystickMode == 1)
  {
    //adjust position using joystick

    // units/sec = movementSpeed => units = seconds*movementSpeed, where movementSpeed is
    //the x and y values
    Vec3f move = Vec3f(gMovementSpeed*(float)xValue/gJoystickCentreX*gLastFrameTime/1000, gMovementSpeed*(float)yValue/gJoystickCentreY*gLastFrameTime/1000, 0);

    move = rotatePointAboutZAxis(move, -gViewAngles.y*M_PI/180); //rotate the move about the Z axis to implement moving in the direction of yaw
    
    gViewPos += move;
  }
  else if (gJoystickMode == 2)
  {
    //rotate cube
    gCubeRotation += gLookSpeed*(float)xValue/gJoystickCentreX*gLastFrameTime/1000;

    //move cube vertically
    gCubeHeight += gMovementSpeed*(float)yValue/gJoystickCentreY*gLastFrameTime/1000;
  }
}

void drawCube(void)
{
  //screen plane is xz plane

  //constant definitions for vertices of a cube with side lengths 1 and its left front bottom vertex at the origin
  //lfb, rfb, lft, rft, lbb, rbb, lbt, rbt
  const Vec3f cubeVertices[8] = { Vec3f(0,0,0), Vec3f(1,0,0), Vec3f(0,0,1), Vec3f(1,0,1), Vec3f(0,1,0), Vec3f(1,1,0), Vec3f(0,1,1), Vec3f(1,1,1) };

  //array where each element corresponds to a vertex of the cube, each bit from right to left is true if a line should be drawn from the vertex
  //to the index of the bit
  const unsigned char adjacentVertices[8] = { 0b00010110, 0b00101000, 0b01001000, 0b10000000, 0b01100000, 0b10000000, 0b10000000, 0b00000000 };

  Vec3i projectedVertices[8] = {};

  for (int i = 0; i < 8; i++)
  {
    //offset the cube to be centered around the origin
    Vec3f cubeVertex = cubeVertices[i];
    cubeVertex.x -= 0.5f;
    cubeVertex.y -= 0.5f;
    cubeVertex.z += gCubeHeight;
    cubeVertex = cubeVertex * gCubeSize;

    //rotate the cube
    cubeVertex = rotatePointAboutZAxis(cubeVertex, gCubeRotation*M_PI/180);
    
    //project the current vertex of the cube
    projectedVertices[i] = castPointToScreen(cubeVertex);
    //projectedVertices[i] = castPointToScreen(cubeVertices[i]*gCubeSize);

    //draw the vertex to the screen
    if (coordsOnScreen(projectedVertices[i]))
      u8g2.drawPixel(projectedVertices[i].x, projectedVertices[i].z);
  }

  //draw lines
  for (int i = 0; i < 8; i++)
  {
    for (int j = 0; j < 8; j++)
    {
      if ((adjacentVertices[i] >> j) & 1) //if bit j (LSB first) is 1, draw a line between vertex i and vertex j - see comment on adjacentVertices definition
      {
        if (coordsOnScreen(projectedVertices[i]) && coordsOnScreen(projectedVertices[j]) && coordsInFrontOfScreen(projectedVertices[i]) && coordsInFrontOfScreen(projectedVertices[j])) //if both points are in front of the screen plane
        {
          //draw the line
          u8g2.drawLine(projectedVertices[i].x, projectedVertices[i].z, projectedVertices[j].x, projectedVertices[j].z);
        }
      }
    }
  }
}

Vec3i castPointToScreen(Vec3f p)
{
  //set the focal point to be {focal distance} behind the camera
  Vec3f focalPoint = gViewPos;
  focalPoint.y -= gFocalDistance;
  
  //adjust the point to have the view pos as its origin
  p -= gViewPos;

  //rotate the point about the z axis to implement yaw
  p = rotatePointAboutZAxis(p, gViewAngles.y * M_PI/180);

  //rotate the point about the x axis to implement pitch
  p = rotatePointAboutXAxis(p, -gViewAngles.x * M_PI/180);

  //undo the origin adjustment
  p += gViewPos;
  
  //project the point to the screen (xz) plane
  Vec3f worldPos = p + (focalPoint-p) * ((gViewPos.y-p.y) / (focalPoint.y-p.y));

  Vec3i screenCoords;

  //convert the world position into screen position (map [-gViewWidth/2,gViewWidth/2] to [0,gScreenWidth] and [-gViewHeight/2,gViewHeight/2] to [gScreenHeight,0])
  screenCoords.x = ((worldPos.x - gViewPos.x) / gViewWidth + 0.5f) * gScreenWidth;
  screenCoords.z = gScreenHeight - 1 - ((worldPos.z - gViewPos.z) / gViewHeight + 0.5f) * gScreenHeight;

  //use the y value to represent the distance between the point and the screen plane
  screenCoords.y = p.y - gViewPos.y;

  return screenCoords;
}

bool coordsOnScreen(Vec3i p)
{
  //returns true if x is in [0,gScreenWidth), z is in [0,gScreenHeight)
  return p.x >= 0 && p.x < gScreenWidth && p.z >= 0 && p.z < gScreenHeight;
}

bool coordsInFrontOfScreen(Vec3i p)
{
  return p.y >= 0;
}

Vec3f rotatePointAboutXAxis(Vec3f p, float angle)
{
  Vec3f result;

  //using a rotation matrix
  
  float cosValue = cos(angle);
  float sinValue = sin(angle);

  result.x = p.x;
  result.y = p.y * cosValue - p.z * sinValue;
  result.z = p.y * sinValue + p.z * cosValue;

  return result;
}

Vec3f rotatePointAboutZAxis(Vec3f p, float angle)
{
  Vec3f result;

  //using a rotation matrix
  float cosValue = cos(angle);
  float sinValue = sin(angle);

  result.x = p.x * cosValue - p.y * sinValue;
  result.y = p.x * sinValue + p.y * cosValue;
  result.z = p.z;

  return result;
}
