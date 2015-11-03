#define shiftClock	0
#define shiftLatch	1
#define shiftLamp0	2
#define shiftLamp1	3
#define	shiftSol	4
#define	shiftLED	5
#define shiftInput	31

#define strobeSpeed 100							//Number of millis before the strobe advances
#define blinkSpeed0 200							//Number of millis before the blink changes
#define blinkSpeed1 400							//Number of millis before the blink changes

								//0, 1,  2,  3,   4,  5,  6,  7,   8,   9,   b,	 A,   L,  P,   Y,  H,  G,  N
unsigned char digitMaker[18] = {126, 48, 109, 121, 51, 91, 95, 112, 127, 123, 31, 119, 14, 103, 39, 55, 94, 118};

int displayCycle = 0;								//Show score, then ball #, then player # (if more than 1 player)


unsigned char strobeAmount[32];
unsigned int strobePos[32];							//Which lamp the stobe is on (0 = target, 2 = third)
unsigned char lampState[32];						//What state each light is in 0 = standard, 1 = blink, 2 = strobe + 3, 4 = pulsate
unsigned int strobeTimer = 0;
unsigned char lightSpeed = 1;						//How fast blinks, pulsates and strobes occur. Depends on kernel speed too. Default = 1
unsigned int blinkTimer = 0;						//Timer for blinking the lights

unsigned short cabinet = 0;							//The last switch reading

unsigned long highScore = 0;						//What current high score is
unsigned long playerScore[5];						//Holds the score
unsigned char player = 0;							//Which player is active
unsigned char numPlayers = 0;      					//Total # of players in the game
int scoreMultiplier = 1;							//Default multiplier
unsigned char ball = 0;								//Which ball we're on
unsigned char drainState = 0;						//Keeps track if the ball is drained or not
unsigned char run = 0;								//What state the game is in

int saveTimer = 0;									//Save time per ball

//Mode variables

unsigned char mode = 0;								//Shots score points / make noise, no progress
unsigned char hubbleProgress[5];					//Mode 1
unsigned char issProgress[5];						//Mode 2
unsigned char asProgress[5];						//Mode 3
unsigned char satProgress[5];						//Mode 4

unsigned char leftOrbitCount[5];							//What to show in Mode 0
unsigned char leftRampCount[5];
unsigned char rightRampCount[5];
unsigned char rightOrbitCount[5];

unsigned char targetState = 1;						//What state the target is in. We check this at start of game / each ball
unsigned char modeMiss = 0;							//Miss X number of shots and it'll remind you what to do

int leftLoopTimer = 0;
int leftRampTimer = 0;
int rightRampTimer = 0;
int rightLoopTimer = 0;
int startButtonTimer = 0;

#define loopDebounce	1000
#define rampDebounce	1500

#define LEDbright	0x0A


//Inputs

#define leftRamp 1 << 15
#define rightRamp 1 << 14
#define leftLoop 1 << 13
#define rightLoop 1 << 12
#define drain 1 << 11

#define targetDown	1 << 8
#define lowerLimit	1 << 7
#define upperLimit	1 << 6

#define rightButton 1 << 2
#define leftButton	1 << 1
#define startButton 1



//Coils

#define leftCoil 	14
#define	rightCoil	15
#define loadCoil	13
#define gateCoil	12

#define rightFlipper 9
#define leftFlipper 8

#define flipperGo	8

#define motorDown	1
#define motorUp		0


int motorState = 0;



int LFlipTime = -1;							//Timer for flipper high current
int RFlipTime = -1;							//Timer for flipper high current

int LholdTime = 0;							//Timers for hold coil PWM
int RholdTime = 0;							//Timers for hold coil PWM

unsigned short FlipPower = 1; 						//Default flipper high power winding ON time, in cycles

int leftDebounce = 0;						//Flipper buttons don't use the built-in Cabinet Button Debounce
int rightDebounce = 0;						//These variables do it manually

#define flipperDebounce 5

#define holdTop			100 //250					//Used to PWM the hold coil on flippers
#define holdHalf		50 //125					//Save a calculation later	