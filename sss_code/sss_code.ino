#include "pinball.h"
#include "kinetis.h"
#include "pins_arduino.h"
#include <EEPROM.h>

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

volatile byte lamp[32];
volatile byte solenoid[16];
volatile unsigned short LEDcommand = 0;
volatile unsigned char whichDigit = 0;												//Which digit we're sending to the MAX7221
volatile char scoreString[7] = {0, 0, 0, 0, 0, 0, 0};								//Used to build the digits to go on the LED
volatile char tempString[7] = {0, 0, 0, 0, 0, 0, 0};								//Used to make temp Score String to right-align value
volatile unsigned short digitData = 0;												//Used to build data going to the MAX7221	
volatile unsigned short cabTemp = 0;												//Used to collect the cabinet switch data
byte LEDglow = 0;
byte LEDglowDir = 0;

unsigned long currentMillis = 0;
char sfxFilename[8] = {'x', 'x', 'x', '.', 'W', 'A', 'V', 0};
byte priority[2];
byte musicPlaying = 0;
char currentMusic[3] = {'x', 'x', 'x'};

int timerX = 0;

AudioPlaySdWav           playWav[2];					//Set up two channel stereo audio mixed into a stereo signal
AudioMixer4              mixer2;
AudioMixer4              mixer1;
AudioOutputI2S           i2s1;
AudioConnection          patchCord1(playWav[0], 0, mixer1, 0);
AudioConnection          patchCord2(playWav[0], 1, mixer2, 0);
AudioConnection          patchCord3(playWav[1], 0, mixer1, 1);
AudioConnection          patchCord4(playWav[1], 1, mixer2, 1);
AudioConnection          patchCord5(mixer2, 0, i2s1, 1);
AudioConnection          patchCord6(mixer1, 0, i2s1, 0);
AudioControlSGTL5000     sgtl5000_1;

IntervalTimer pollIOtimer;								//Set up a timer

void setup() {                

    Serial.begin(9600);
	
	pinMode(shiftClock, OUTPUT);						//Master clock  
	pinMode(shiftLatch, OUTPUT); 						//Master latch
	pinMode(shiftLamp0, OUTPUT); 						//LED driver 0 (lamps 0-15)
	pinMode(shiftLamp1, OUTPUT); 						//LED driver 1(lamps 16-31)
	pinMode(shiftSol, OUTPUT); 							//Solenoid drivers 0-15
	pinMode(shiftLED, OUTPUT); 							//MAX7221 7 segment LED driver
	pinMode(shiftInput, INPUT);							//Shift register switch input line
	
	pollIOtimer.begin(pollIO, 4000);					//Do the PollIO routine 250Hz (or every 4000 microseconds)

	AudioMemory(32);									//Twice of the max, which seems to be 6

	sgtl5000_1.enable();
	sgtl5000_1.volume(1);

	SPI.setMOSI(7);
	SPI.setSCK(14);
	
	if (!(SD.begin(10))) {	
		while (1) {
		  Serial.println("Unable to access the SD card");	// stop here, but print a message repetitively 
		  delay(500);
		}
	}
	else {
		Serial.println("Connected to SD card!");
		delay(500);      
	}


	resetLED();											//Reset the display
	
}

void loop() {

	machineReset();

	while (run == 0) {
	
		Serial.println(cabinet, BIN);	
	
		//Attract Mode Shit
	
		if (cabinet & startButton) {

			if (cabinet & drain) {	//Ball must be in drain to start!
				startButtonTimer = 250;
				run = 1;											//Advance state if START pressed
			}
			else {
				scoreString[0] = digitMaker[17]; 	//N
				scoreString[1] = digitMaker[0];		//0	
				scoreString[2] = 0; 				//space	
				scoreString[3] = digitMaker[10]; 	//B
				scoreString[4] = digitMaker[11];	//A		
				scoreString[5] = digitMaker[12];	//L
				scoreString[6] = digitMaker[12];	//L	
				displayCycle = 0;					//So we'll see message
			}
		}

		displayCycle += 1;

		switch(displayCycle) {
			
			case 1500:
				blankLED();
				scoreString[0] = digitMaker[15]; 	//H
				scoreString[1] = digitMaker[1];		//1		
				scoreString[2] = digitMaker[16];	//G
				scoreString[3] = digitMaker[15];	//H
			break;
			case 3000:
				makeScore(highScore);	//Show score, and repeat	
				displayCycle = 0;
			break;		
		}
		
		houseKeeping();
	
	}

	startGame();
	
	while (ball < 4) {
	
		gameLoop();
	}
	
	gameOver();
	
}

void gameLoop() {

	timers();
	
	switchCheck();
	
	houseKeeping();							//1 ms cycle lock and Music Loop check performed in this function

}

void timers() {

	if (motorState) {						//Motor is supposed to be doing something?
		doMotor();							//Go do it
	}
	
	if (leftLoopTimer) {		
		leftLoopTimer -= 1;	
	}	
	
	if (leftRampTimer) {		
		leftRampTimer -= 1;	
	}	
	
	if (rightRampTimer) {		
		rightRampTimer -= 1;	
	}
	
	if (rightLoopTimer) {		
		rightLoopTimer -= 1;	
	}	

	if (startButtonTimer and (cabinet & startButton) == 0) {	
		startButtonTimer -= 1;		
	}
	
	displayCycle += 1;

	switch(displayCycle) {
		
		case 2000:
		if (numPlayers > 1) {
			blankLED();
			scoreString[0] = digitMaker[13];		//P
			scoreString[1] = digitMaker[12];		//L	
			scoreString[2] = digitMaker[11];		//A				
			scoreString[3] = digitMaker[14];		//Y skip one space...
			scoreString[5] = digitMaker[player];	//Player #			
		}		
		else {
			displayCycle = 2999;		//If only 1 player, don't show player # just skip to score
		}
		break;
		case 3000:		
			blankLED();
			scoreString[1] = digitMaker[10]; 	//B
			scoreString[2] = digitMaker[11];	//A		
			scoreString[3] = digitMaker[12];	//L
			scoreString[4] = digitMaker[12];	//L skip one space then	
			scoreString[6] = digitMaker[ball];	//Ball #

		break;	
		case 4000:
			makeScore(playerScore[player]);	//Show score, and repeat	
			displayCycle = 1000;
		break;		
	}
	
}

void switchCheck() {

	//FLIPPERS GO HERE!

	flippers();
	
	if ((cabinet & startButton) == 1 and startButtonTimer == 0) {	
		startButtonTimer = 250;
		if (numPlayers < 4 and ball == 1) {						//Can only add players on Ball 1
			numPlayers += 1;			//Increment player #		
			playSFX(0, 'S', 'B', '0' + numPlayers, 255);		//Add player 2-4		
		}	
	}

	if ((cabinet & drain) and drainState == 0) {
	
		drainBall();
	
	}

	if ((cabinet & leftLoop) and leftLoopTimer == 0 and rightLoopTimer == 0) {
		leftLoopTimer = loopDebounce;
	
		if (mode == 0) {					//No mode?
		
			modePrompt(2);					//Play generic sound for orbits, with START MODE prompts
			
			leftOrbitCount[player] += 1;
			
			if (leftOrbitCount[player] > 3) {
				leftOrbitCount[player] = 0;
				addScore(2000);
			}
			else {
				addScore(1000);
			}
			
			playerLights();
	
		}
		else {								//Some mode is active...
		
			if (mode == 1) {				//Hubble mode? correct shot
			
				hubbleProgress[player] += 1;
				playSFX(0, 'A', 'A' + hubbleProgress[player], '1' + random(3), 255);	//Progress dialog

				if (hubbleProgress[player] == 3) {				
					addScore(50000);
					winMode();
				}
				else {
					addScore(5000);
				}
				playerLights();
				
			}
			else {
			
				modePrompt(2);				//Play generic sound for orbits, with MODE PROGRESS prompts
				addScore(2000);	
			}
			
		}

	}
	
	if ((cabinet & leftRamp) and leftRampTimer == 0) {
		leftRampTimer = rampDebounce;
				
		if (mode == 0) {					//No mode?
		
			modePrompt(1);					//Play generic sound for orbits, with START MODE prompts
			leftRampCount[player] += 1;
			
			if (leftRampCount[player] > 3) {
				leftRampCount[player] = 0;
				addScore(4000);
			}
			else {
				addScore(2000);
			}
			
			playerLights();
		}
		else {								//Some mode is active...
		
			if (mode == 2) {				//ISS mode? correct shot
			
				issProgress[player] += 1;
				playSFX(0, 'B', 'A' + issProgress[player], '1' + random(3), 255);	//Progress dialog

				if (issProgress[player] == 3) {				
					addScore(50000);
					winMode();
				}
				else {
					addScore(5000);
				}
				playerLights();	
				
			}
			else {
			
				modePrompt(1);				//Play generic sound for orbits, with MODE PROGRESS prompts
				addScore(2000);
			}
			
		}
	}
	
	if ((cabinet & targetDown) and targetState == 1 and motorState == 0) {	//Switch hit and target wasn't already hit?

		addScore(5000);
	
		light(15, 0);

		targetState = 0;							//Target down!
		
		startMode();
	}	

	if ((cabinet & rightRamp) and rightRampTimer == 0) {
	
		rightRampTimer = rampDebounce;
		
		if (mode == 0) {					//No mode?
		
			modePrompt(1);					//Play generic sound for orbits, with START MODE prompts
			rightRampCount[player] += 1;
			
			if (rightRampCount[player] > 3) {
				rightRampCount[player] = 0;
				addScore(4000);
			}
			else {
				addScore(2000);
			}
			
			playerLights();
		}
		else {								//Some mode is active...
		
			if (mode == 3) {				//Asteroid mode? correct shot
			
				asProgress[player] += 1;
				playSFX(0, 'C', 'A' + asProgress[player], '1' + random(3), 255);	//Progress dialog

				if (asProgress[player] == 3) {				
					addScore(50000);
					winMode();
				}
				else {
					addScore(5000);
				}
				playerLights();	
				
			}
			else {
			
				modePrompt(1);				//Play generic sound for orbits, with MODE PROGRESS prompts
				addScore(2000);
			}
			
		}
	
	}
	
	if ((cabinet & rightLoop) and leftLoopTimer == 0 and rightLoopTimer == 0) {
		rightLoopTimer = loopDebounce;
		
		if (mode == 0) {					//No mode?
		
			modePrompt(3);					//Play generic sound for orbits, with START MODE prompts
			rightOrbitCount[player] += 1;
			
			if (rightOrbitCount[player] > 3) {
				rightOrbitCount[player] = 0;
				addScore(2000);
			}
			else {
				addScore(1000);
			}
			
			playerLights();
		}
		else {								//Some mode is active...
		
			if (mode == 4) {				//Satellite mode? correct shot
			
				satProgress[player] += 1;
				playSFX(0, 'B', 'A' + satProgress[player], '1' + random(3), 255);	//Progress dialog

				if (satProgress[player] == 3) {				
					addScore(50000);
					winMode();
				}
				else {
					addScore(5000);
				}
				playerLights();	
				
			}
			else {
			
				modePrompt(3);				//Play generic sound for orbits, with MODE PROGRESS prompts
				addScore(1000);
			}
			
		}
		
	}	

	// if (cabinet & startButton) {	
	
		// Serial.print("Audio Mem Current: ");
		// Serial.print(AudioMemoryUsage());	
		// Serial.print(" Audio Mem Max: ");
		// Serial.println(AudioMemoryUsageMax());
		// delay(10);	
	// }	
	
	
}

void flippers() {								//Control flippers, if enabled, as well as ball launcher

	if ((cabinet & leftButton) and LFlipTime == -1) {		//Left button pressed?
		leftDebounce += 1;
		if (leftDebounce > flipperDebounce) {
			leftDebounce = flipperDebounce;
			solenoid[leftFlipper] = 1;							//Set flipper
			LFlipTime = FlipPower;
		}
	}

	if ((cabinet & rightButton) and RFlipTime == -1) {		//Right button pressed?
		rightDebounce += 1;
		if (rightDebounce > flipperDebounce) {
			rightDebounce = flipperDebounce;
			solenoid[rightFlipper] = 1;							//Set flipper
			RFlipTime = FlipPower;
		}
	}

	//Flippers can time out or be released even if not enabled. (which is why Flipper routine should ALWAYS run)

	if (LFlipTime < -1) {	
		LFlipTime += 1;	
	}
	
	if (LFlipTime > 0) {
		LFlipTime -= 1;
		if (LFlipTime == 0) { 					//Did timer run out OR EOS hit?
			solenoid[leftFlipper] = 0;							//Turn off flipper
			LholdTime = holdTop + 5;			 //Set PWM timer
		}
	}

	if (LholdTime) {
	
		if (LholdTime == holdTop) {
			solenoid[leftFlipper] = 1;							//Turn on flipper
		}
		if (LholdTime == holdHalf) {
			solenoid[leftFlipper] = 0;							//Turn off flipper
		}
		if (LholdTime == 1) {									//Almost done?
			LholdTime = holdTop + 1;							//Reset it
		}
		LholdTime -= 1;
	}
	
	if ((cabinet & leftButton) == 0) { //Button released? (normal state)
		leftDebounce = 0;
		LFlipTime = -10;				//Make flipper re-triggerable, with debounce
		LholdTime = 0;				//Disable hold timer.
		solenoid[leftFlipper] = 0;							//Turn off flipper	
	}

	if (RFlipTime < -1) {	
		RFlipTime += 1;	
	}
	
	if (RFlipTime > 0) {
		RFlipTime -= 1;
		if (RFlipTime == 0) { //Did timer run out OR EOS hit?
			solenoid[rightFlipper] = 0;							//Turn off flipper
			RholdTime = holdTop + 5;			 //Set PWM timer
			//digitalWrite(RFlipLow, 1); //Switch on hold current
		}
	}

	if (RholdTime) {
	
		if (RholdTime == holdTop) {
			solenoid[rightFlipper] = 1;							//Turn on flipper
		}
		if (RholdTime == holdHalf) {
			solenoid[rightFlipper] = 0;							//Turn off flipper
		}
		if (RholdTime == 1) {			//Almost done?
			RholdTime = holdTop + 1;		//Reset it
		}
		RholdTime -= 1;
	}
			
	if ((cabinet & rightButton) == 0) { 						//Button released? (normal state)
		rightDebounce = 0;
		RFlipTime = -10;										//Make flipper re-triggerable, with debounce
		RholdTime = 0;											//Disable hold timer
		solenoid[rightFlipper] = 0;							//Turn off flipper	
	}

}

void drainBall() {

	solenoid[leftFlipper] = 0;							//Turn off flipper
	solenoid[rightFlipper] = 0;							//Turn off flipper	

	playSFX(0, 'D', 'R', '1' + random(5), 255);			//Abort abort!

	allLight(0);

	modeMiss = 0;

	resetLED();											//Reset the display
	
	for (int x = 0 ; x < 7 ; x++) {
		scoreString[x] = 1;			//Make all dashes
	}
	
	resetTarget();
	
	player += 1;
	
	if (player > numPlayers) {
		player = 1;
		ball += 1;
	}
	
	if (ball == 4) {
		return;
	}

	displayCycle = 1998;			//To show PLAY # (or BALL # in 1 player) right away
	
	int musicChange = 0;
	
	if (mode) {						//Check now so when we turn lights back on mode is correct
		musicChange = 1;
		mode = 0;
	}	
	
	delay(2000);					//Gives speech time to finish
	playerLights();		
	loadBall();
	
	if (musicChange) {
		playSFX(1, 'M', 'A', '3', 255);				//Normal music	
	}
	
	if (numPlayers > 1) {
		playSFX(0, 'S', 'P', '0' + player, 255);	//If more than 1 player, say which player is up
	}
	

}

void startGame() {

	resetLED();

	allLight(0);

	player = 1;
	mode = 0;								//Shots score points / make noise, no progress
	ball = 1;
	
	for (int x = 1 ; x < 5 ; x++) {
	
		playerScore[x] = 0;
		hubbleProgress[x] = 0;					//Mode 1
		issProgress[x] = 0;						//Mode 2
		asProgress[x] = 0;						//Mode 3
		satProgress[x] = 0;						//Mode 4

		leftOrbitCount[x] = 0;						//What to show in Mode 0
		leftRampCount[x] = 0;
		rightRampCount[x] = 0;
		rightOrbitCount[x] = 0;
		
	}

	makeScore(playerScore[player]);
	
	playSFX(1, 'M', 'A', '3', 255);
	playSFX(0, 'S', 'A', '1' + random(5), 255);			//Welcome to Space Shuttle!

	loadBall();
	//solenoid[flipperGo] = 1;
	resetTarget();
	
	//DO LIGHTS STARTING STATE PER PLAYER

	numPlayers = 1;
	
	playerLights();
	
}

void gameOver() {

	solenoid[leftFlipper] = 0;							//Turn off flipper
	solenoid[rightFlipper] = 0;							//Turn off flipper	

	stopMusic();

	blankLED();
	
	for (int x = 1 ; x < (numPlayers + 1) ; x++) {									//See if any players got a high score!
	
		if (playerScore[x] > highScore) {				//Did this player exceed high score?
			
			//NEW HIGH SCORE PLAYER X VOICE PROMPT!
			
			highScore = playerScore[x];					//Set the new high score...
			eepromLongWrite(1, highScore);				//and store to EEPROM
						
		}
		
	}

}

int loadBall() {

	if ((cabinet & drain) == 0) {				//No ball?
	
		return 0;								//Do not attempt load
	
	}

	solenoid[gateCoil] = 1;
	delay(250);	
	solenoid[loadCoil] = 1;		
	delay(500);
	solenoid[loadCoil] = 0;
	solenoid[gateCoil] = 0;	

	return 1;									//Success! (we hope)
	
}

void machineReset() {

	allLight(0);

	if ((cabinet & drain) == 0) {
		solenoid[gateCoil] = 1;
		delay(500);
		solenoid[gateCoil] = 0;		
	}
		
	for (int x = 0 ; x < 16 ; x++) {
	
		solenoid[x] = 0;
	
	}
	
	while ((cabinet & lowerLimit) == 0) {
	
		delay(1);
		solenoid[motorUp] = 0;
		solenoid[motorDown] = 1;
	
	}

	solenoid[motorDown] = 0;
	
	strobe(26, 2);
	strobe(24, 2);
	strobe(1, 3);
	strobe(4, 3);
	strobe(7, 3);
	strobe(10, 3);
	
	blink(28);
	blink(29);
	blink(30);
	blink(31);

	run = 0;
	
	if ((cabinet & startButton) and player == 0) {	//Hold START on boot to clear high score
		highScore = 1000;
		eepromLongWrite(1, highScore);	
	}
	else {
		highScore = eepromLongRead(1);		//Get the high score!	
	}
	
	displayCycle = 1499;
	
}

void playerLights() {

	allLight(0);
	
	if (mode == 0) {	
		blink(15);	
		
		light(26, 1);
		light(27, 1);
		light(24, 1);
		light(25, 1);
		
		strobe(28, 4);			

		switch(leftOrbitCount[player]) {
		
			case 0:		
				light(1, 0);
				light(2, 0);
				light(3, 0);

			break;
			case 1:
				light(1, 0);
				light(2, 0);	
				light(3, 1);		
			break;
			case 2:
				light(1, 0);
				light(2, 1);
				light(3, 1);		
			break;		
			case 3:		
				light(1, 1);
				light(2, 1);
				light(3, 1);			
			break;				
		}
		switch(leftRampCount[player]) {
		
			case 0:		
				light(4, 0);
				light(5, 0);
				light(6, 0);			
			break;
			case 1:
				light(4, 0);
				light(5, 0);
				light(6, 1);		
			break;
			case 2:
				light(4, 0);
				light(5, 1);
				light(6, 1);		
			break;		
			case 3:
				light(4, 1);
				light(5, 1);
				light(6, 1);		
			break;	
			
		}	
		switch(rightRampCount[player]) {
		
			case 0:		
				light(7, 0);
				light(8, 0);
				light(9, 0);			
			break;
			case 1:
				light(7, 0);
				light(8, 0);
				light(9, 1);		
			break;
			case 2:
				light(7, 0);
				light(8, 1);
				light(9, 1);		
			break;		
			case 3:		
				light(7, 1);
				light(8, 1);
				light(9, 1);		
			break;				
		}
		switch(rightOrbitCount[player]) {
		
			case 0:		
				light(10, 0);
				light(11, 0);
				light(12, 0);	
			break;
			case 1:
				light(10, 0);
				light(11, 0);
				light(12, 1);		
			break;
			case 2:
				light(10, 0);
				light(11, 1);
				light(12, 1);		
			break;		
			case 3:		
				light(10, 1);
				light(11, 1);
				light(12, 1);		
			break;				
		}	

	}
	else {
		light(15, 0);
		
		strobe(26, 2);
		strobe(24, 2);
		
		blink(28);
		blink(29);
		blink(30);
		blink(31);

		switch(hubbleProgress[player]) {
		
			case 0:		
				light(1, 0);
				light(2, 0);
				if (mode == 1) {
					blink(3);
				}
				else {
					light(3, 0);
				}	
			break;
			case 1:
				light(1, 0);
				if (mode == 1) {
					blink(2);
				}
				else {
					light(2, 0);
				}	
				light(3, 1);		
			break;
			case 2:
				if (mode == 1) {
					blink(1);
				}
				else {
					light(1, 0);
				}	
				light(2, 1);
				light(3, 1);		
			break;		
			case 100:		
				light(1, 1);
				light(2, 1);
				light(3, 1);
			
			break;				
		}
		switch(issProgress[player]) {
		
			case 0:		
				light(4, 0);
				light(5, 0);
				if (mode == 2) {
					blink(6);
				}
				else {
					light(6, 0);
				}	
			
			break;
			case 1:
				light(4, 0);
				if (mode == 2) {
					blink(5);
				}
				else {
					light(5, 0);
				}	
				light(6, 1);		
			break;
			case 2:
				if (mode == 2) {
					blink(4);
				}
				else {
					light(4, 0);
				}	
				light(5, 1);
				light(6, 1);		
			break;		
			case 100:
				light(4, 1);
				light(5, 1);
				light(6, 1);		
			break;				
		}	
		switch(asProgress[player]) {
		
			case 0:		
				light(7, 0);
				light(8, 0);
				if (mode == 3) {
					blink(9);
				}
				else {
					light(9, 0);
				}			
			break;
			case 1:
				light(7, 0);
				if (mode == 3) {
					blink(8);
				}
				else {
					light(8, 0);
				}	
				light(9, 1);		
			break;
			case 2:
				if (mode == 3) {
					blink(7);
				}
				else {
					light(7, 0);
				}	
				light(8, 1);
				light(9, 1);		
			break;		
			case 100:		
				light(7, 1);
				light(8, 1);
				light(9, 1);		
			break;				
		}
		switch(satProgress[player]) {
		
			case 0:		
				light(10, 0);
				light(11, 0);
				if (mode == 4) {
					blink(12);
				}
				else {
					light(12, 0);
				}			
			break;
			case 1:
				light(10, 0);
				if (mode == 4) {
					blink(11);
				}
				else {
					light(11, 0);
				}	
				light(12, 1);		
			break;
			case 2:
				if (mode == 4) {
					blink(10);
				}
				else {
					light(10, 0);
				}	
				light(11, 1);
				light(12, 1);		
			break;		
			case 100:		
				light(10, 1);
				light(11, 1);
				light(12, 1);		
			break;				
		}	
		
	}
	

}


void modePrompt(unsigned char whichSound) {

	modeMiss += 1;
	
	if (modeMiss == 3) {		//Every third "missed" shot do a prompt
	
		modeMiss = 0;
		
		if (mode == 0) {
			playSFX(0, 'P', 'E', '1' + random(5), 255);						//Shoot target to start modes!
		}
		else {
			playSFX(0, 'P', '@' + mode, '1' + random(5), 255);	//Shoot WHATEVER to advance mode
		}		
	}
	else {
		playSFX(0, 'S', 'X', '0' + whichSound, 250);		//Generic sounds (lower priority than prompts)
	}

}

void startMode() {

	mode = 0;		//Starting state

	if (hubbleProgress[player] == 100 and issProgress[player] == 100 and asProgress[player] == 100 and satProgress[player] == 100) {
	
		//Did them all? Start over.
		hubbleProgress[player] = 0;
		issProgress[player] = 0;
		asProgress[player] = 0;
		satProgress[player] = 0;
	
	}
	
	while (mode == 0) {		//Don't leave until we pick something
	
		mode = random(4) + 1;		//Pick a mode from 1-4
		
		switch(mode) {				//If randomly selected mode has been completed, pick another
		
			case 1:
				if (hubbleProgress[player] == 100) {
					mode = 0;
				}
				else {
					playSFX(0, 'A', 'A', '1' + random(3), 255);	//Fix hubble!
				}
			break;
			case 2:
				if (issProgress[player] == 100) {
					mode = 0;
				}
				else {
					playSFX(0, 'B', 'A', '1' + random(3), 255);	//ISS!
				}				
			break;		
			case 3:
				if (asProgress[player] == 100) {
					mode = 0;
				}
				else {
					playSFX(0, 'C', 'A', '1' + random(3), 255);	//Asteroid
				}					
			break;
			case 4:
				if (satProgress[player] == 100) {
					mode = 0;
				}
				else {
					playSFX(0, 'D', 'A', '1' + random(3), 255);	//Satellite
				}					
			break;			
		}
	
	}
	
	playSFX(1, 'M', 'B', '3', 255);				//Mission music


	playerLights();
	
}

void winMode() {

	switch(mode) {				//If randomly selected mode has been completed, pick another
	
		case 1:
			hubbleProgress[player] = 100;
		break;
		case 2:
			issProgress[player] = 100;
		break;		
		case 3:
			asProgress[player] = 100;
		break;
		case 4:
			satProgress[player] = 100;
		break;	
		
	}
	
	mode = 0;
	
	resetTarget();
	
	//CHECK IF ALL MODES WON

}

void doMotor() {

	motorState += 1;							//Increment state

	if (motorState < 150) {						//Motor going UP state?
	
		solenoid[motorUp] = 1;
		solenoid[motorDown] = 0;
	
		if (cabinet & upperLimit) {				//Hit the limti switch?
			motorState = 150;					//Jump to next event
		}
	}
	if (motorState == 150) {					//Second event?
		solenoid[motorUp] = 0;					//Reverse motor
		solenoid[motorDown] = 0;	
	}		
	if (motorState == 500) {					//Second event?
		targetState = 1;						//Reset state
		solenoid[motorUp] = 0;					//Reverse motor
		solenoid[motorDown] = 1;	
	}
	if (motorState > 500) {
		if (cabinet & lowerLimit) {				//Hit the limti switch?
			motorState = 1000;					//Jump to next event
		}
	}
	if (motorState > 990 ) {
		motorState = 0;	
		solenoid[motorUp] = 0;					//Stop the motor
		solenoid[motorDown] = 0;		
	}

}

void resetTarget() {

	if ((cabinet & targetDown) == 0) {					//Target NOT down?
		targetState = 1;								//It must be UP, return that state
		return;											//Do nothing Jon Snow!
	}

	//Else put it up
	
	motorState = 1;
	
	if (mode == 0) {
		blink(15);
	}
	
}

void resetLED() {

	sendLEDcommand(0x0C, 1);								//Enable display  
	sendLEDcommand(LEDbright, 10);							//Set LED brightness (0-15)		
	sendLEDcommand(0x0B, 6);								//Set to scan characters 0-6
	sendLEDcommand(0x09, 0);								//Set segments manually	
	
}

void houseKeeping() {

	boolean strobeFlag = 0;					//Whether or not we should move the strobing lights on this cycle

	blinkTimer += lightSpeed;
	
	if (blinkTimer > blinkSpeed1) {	
		blinkTimer = 0;	
	}

	strobeTimer += lightSpeed;
	
	if (strobeTimer > strobeSpeed) {	
		strobeTimer = 0;
		strobeFlag = 1;								//Set strobe flag.
	}

	for (int x = 0 ; x < 32 ; x++) {
	
	if (lampState[x] == 1)	{						//Light set to blink?

		if (blinkTimer < blinkSpeed0) {			
			lamp[x] = 1;			//Light on
		}
		if (blinkTimer > blinkSpeed0) {			
			lamp[x] = 0;			//Light on			
		}

	}
	
	if (lampState[x] == 3)	{						//Light set to strobe?

		if (strobeFlag) {		//Did we roll over timer, and ready to strobe?
		
			strobePos[x] += 1;
			
			if (strobePos[x] == strobeAmount[x]) { //(lampState[x] >> 2) - 1) {
				strobePos[x] = 0;				
			}
		
		}
	
		lamp[x + strobePos[x]] = 1;					//Set current strobe light to ON
		//lampState[x + strobePos[x]] = 10;			//Set current light's state to ON
		
		if (strobePos[x] == 0) {
			lamp[x + strobeAmount[x] - 1] = 0;		//Erase last strobe
			//lampState[x + strobeAmount[x] - 1] = 10;		//Set current light's state to OFF
		}
		else {
			lamp[x + (strobePos[x] - 1)] = 0;		//Erase last strobe
			//lampState[x + (strobePos[x] - 1)] = 10;	//Set current light's state to OFF
		}

	}	

	}

	//Wait for the MS boundary to continue
	
	while (currentMillis == millis()) {
	}
	
	currentMillis = millis();
	
	if (musicPlaying and playWav[1].isPlaying() == 0) {							//Music file ended?	
		playSFX(1, currentMusic[0], currentMusic[1], currentMusic[2], 255); 	//Restart the current file		
	}

	
}

void playSFX(unsigned char whichChannel, unsigned char folder, unsigned char clip0, unsigned char clip1, unsigned char newPriority) {
  
  if (playWav[whichChannel].isPlaying() == 1) {			//Something playing?

	if (priority[whichChannel] > newPriority) {			//New sound NOT same priority or higher?
		Serial.println("Priority Reject!");
		return; 										//Don't play new sound
	}
	else {
		playWav[whichChannel].stop();					//Stop the channel
		while(playWav[whichChannel].isPlaying() == 1) { //Wait for it to end
			delayMicroseconds(1);  
		}
	}
  }
  
  priority[whichChannel] = newPriority;
  
  sfxFilename[0] = folder;
  sfxFilename[1] = clip0;  
  sfxFilename[2] = clip1;
   
  //Serial.print("Playing: ");
  //Serial.print(sfxFilename);  

  if (playWav[whichChannel].play(sfxFilename) == 0) {
    Serial.println(" FILE NOT FOUND");
    return;
  }
  else {
    Serial.println(" OK");
  }  
    
  
  while(playWav[whichChannel].isPlaying() == 0) {
    delayMicroseconds(1);  
  }
  
  if (whichChannel == 1) {								//Music channel?
	musicPlaying = 1;									//Set flag and store filename
	currentMusic[0] = folder;
	currentMusic[1] = clip0;
	currentMusic[2] = clip1;	
  }

}

void stopMusic() {

	playWav[1].stop();					//Stop the channel
	while(playWav[1].isPlaying() == 1) { //Wait for it to end
		delayMicroseconds(1);  
	}
	
	musicPlaying = 0;					//Clear the flag

}


void addScore(long scoreAmount) {

	playerScore[player] += (scoreAmount * scoreMultiplier);	
	makeScore(playerScore[player]);

	displayCycle = 0;
	
}

void makeScore(unsigned long tempScore) {
	
	unsigned long divider = 10000000;						//Divider starts at 1 millions place
	unsigned char size = 0;
	boolean zPad = false;

	blankLED();
	
	//Create the number...
	
	for (int xx = 0 ; xx < 8 ; xx++) {									//Seven places will get us the last 2 digits of a 10 digit score		
		if (tempScore >= divider) {
			tempString[size] = digitMaker[tempScore / divider];		//Create single digit
			tempScore %= divider;										//Divide down the score
			zPad = true;
			size += 1;
		}			
		else if (zPad or divider == 1)  {		
			tempString[size] = digitMaker[0];							//Create ASCII version of digit	0
			size += 1;
		}
		divider /= 10;						
	}	

	//Then right-justify it onscreen
	
	int justifyRight = 7;
	
	for (int x = size ; x >= 0 ; x--) {
	
		scoreString[justifyRight--] = tempString[x];
	
	}
		
}

void blankLED() {
	
	for (int x = 0; x < 8 ; x++) {							//Clear the string
		
		scoreString[x] = 0;								//15 BCD means BLANK digit
		
	}	
		
}

void blink(unsigned char whichLamp) {

	if (lampState[whichLamp] == 3) { 						 //Was this light strobing?
		for (int x = whichLamp ; x < whichLamp + strobeAmount[whichLamp] ; x++) {
				lamp[x] = 0;								 //Clear all strobing lights.					
		}		
	}

	lampState[whichLamp] = 1;						 						

}

void light(unsigned char whichLamp, unsigned char howBright) {

	if (lampState[whichLamp] == 3) { 						 //Was this light strobing?
		for (int x = whichLamp ; x < whichLamp + strobeAmount[whichLamp] ; x++) {
				lamp[x] = 0;								 //Clear all strobing lights above it				
				lampState[x] = 0;
		}		
	}	

	lampState[whichLamp] = 0;								//Default state

	lamp[whichLamp] = howBright;							//Either ON or OFF

}

void strobe(unsigned char whichLamp, unsigned char howMany) {

	lampState[whichLamp] = 3;						 						
	
	strobeAmount[whichLamp] = howMany;	 			 						//Set total number of lights to strobe (includes starting light)			
		
	for (int x = whichLamp + 1 ; x < whichLamp + strobeAmount[whichLamp] ; x++) {
			lampState[x] = 33;								 						//Clear lamp states of strobing lights. Example, if one was set to blink, strobe overwrites it.			
	}		


}

void allLight(unsigned char whichBright) {

	for (int x = 0 ; x < 32 ; x++) {
	
		light(x, whichBright);
	
	}

}

void sendLEDcommand(int theAddress, int theData) {

	LEDcommand = (theAddress << 8) | theData;

	while(LEDcommand) {
		delayMicroseconds(1);
	}

}

void LEDpulse() {

	sendLEDcommand(LEDbright, LEDglow);

	if (LEDglowDir) {
		LEDglow -= 1;
		if (LEDglow == 2) {
			LEDglowDir = 0;
			addScore(1024);	
		}
	}
	else {
		LEDglow += 1;
		if (LEDglow == 15) {
			LEDglowDir = 1;					
		}		
	}
	
}


int cabSwitch(unsigned char switchGet) {				//Read a dedicated switch. If switch is on and debounce is off, returns a 1, else 0

	return (cabinet >> switchGet) & 1;

}

void eepromLongWrite(int location, unsigned long theValue) {	//Store long score little endian format in 4 EEPROM bytes

	EEPROM.write(location + 0, theValue & 0xFF);
	EEPROM.write(location + 1, (theValue >> 8) & 0xFF);
	EEPROM.write(location + 2, (theValue >> 16) & 0xFF);
	EEPROM.write(location + 3, (theValue >> 24) & 0xFF);
	
}

long eepromLongRead(int location) {	//Get long score little endian format in 4 EEPROM bytes

	return EEPROM.read(location) | (EEPROM.read(location + 1) << 8) | (EEPROM.read(location + 2) << 16)  | (EEPROM.read(location + 3) << 24);

}


void pollIO(void) {

	if (LEDcommand) {
		digitData = LEDcommand;
		LEDcommand = 0;
	}
	else {
		digitData = (whichDigit + 1) << 8 | scoreString[whichDigit];
	}

	cabTemp = 0;													//Clear this so we can rebuild it
	
	GPIOB_PCOR = (1 << 17);											//Latch LOW (pin 1)

	for (int x = 0 ; x < 16 ; x++) {
	
		cabTemp <<= 1;												//Shift this one to the left to make room for new bit
	
		GPIOB_PCOR = (1 << 16);										//Clock LOW (pin 0)

		GPIOA_PSOR = (lamp[x + 16] << 12) | (solenoid[x] << 13);	//Set lamps 16-31 and solenoids 0-15 on PORTA
		GPIOA_PCOR = ~((lamp[x + 16] << 12) | (solenoid[x] << 13));		

		GPIOD_PSOR = lamp[x] | (((digitData >> (15 - x)) & 1) << 7);	//Set pin 2 to state of lamps 0-15, and the data going to the LED on PORTD
		GPIOD_PCOR = ~(lamp[x] | (((digitData >> (15 - x)) & 1) << 7));

		GPIOB_PSOR = (1 << 16);										//Clock HIGH (pin 0)

		cabTemp |= !(GPIOE_PDIR & 1);								//Read pin and invert (so 0=off and 1=on)
	
	}

	GPIOB_PSOR = (1 << 17);											//Latch HIGH (pin 1)
		
	whichDigit += 1;
	
	if (whichDigit == 8) {
		whichDigit = 0;
	}
	
	cabinet = cabTemp;												//Copy the cabTemp value into the actual variable (so we don't look at it while it's being built)

}


