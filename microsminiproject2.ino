//include relevant libraries
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <TimerOne.h>

//define number of rows and columns of keypad
const byte ROWS = 4;
const byte COLS = 4;

//define keypad key allocation
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

//define pins of rows and columns
byte rowPins[ROWS] = {A0, A1, A2, A3};
byte colPins[COLS] = {10, 11, 12, 13};

//instance of the keypad class
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

//leds connected to pwm pins
const int LED1 = 9;  //red led
const int LED2 = 6;  //yellow led
const int LED3 = 5;  //green led
const int LED4 = 3;  //blue led

//define lcd size and address according to i2c protocol
LiquidCrystal_I2C lcd(0x27, 16, 2); 

//game variables and array for storing the guess and answer
const byte numBits = 4;
byte answer;
char guess[numBits + 1];
byte guessIndex = 0;

//score and accuracy variables
int score = 0;
int totalGuesses = 0;
int totalCorrectGuesses = 0;
int totalIncorrectGuesses = 0;

//time variables for game start, 60 s duration, and current timestamp
unsigned long gameStartTime = 0;
const unsigned long gameDuration = 60000; 
unsigned long currentNumberStartTime = 0;

//result display duration variables
const unsigned long resultDisplayDuration = 1000; 
unsigned long resultDisplayStartTime = 0;

//button pin definition, interrupt variable and debounce constant
const byte buttonPin = 2;
volatile bool buttonPressed = false;
const unsigned long debounceDelay = 50;

//function call when the button pin triggers on the falling edge interrupt
void buttonInterrupt() {
  //variables for storing debounce and current time
  static unsigned long lastDebounceTime = 0;
  unsigned long currentTime = millis();

  //time difference > debounce = button pressed
  if (currentTime - lastDebounceTime >= debounceDelay) {
    buttonPressed = true; 
    lastDebounceTime = currentTime;
  }
}

//enum representing timer states
//finite state machine logic
enum TimerMode {
  TIMER_DISABLED,
  TIMER_RUNNING
};

//instance of enum set to disabled state and brightness variable
TimerMode timerMode = TIMER_DISABLED;
int brightness1 = 255;

//enum representing game state
//finite state machine logic 
enum GameState {
  WAIT_FOR_START,
  WAIT_FOR_INPUT,
  DISPLAY_NUMBER,
  DISPLAY_RESULT,
  GAME_OVER
};

//instance of enum set the wait for start
GameState gameState = WAIT_FOR_START;

//set up function
void setup() {
  //serial communication started at baud rate of 9600
  Serial.begin(9600);

  //lcd initialised
  lcd.begin(); 

  //button pin initialised with an internal pull up resistor
  pinMode(buttonPin, INPUT_PULLUP);

  //interrupt attached to button pin
  //buttonInterrupt function specified as isr to be called when button is triggered on falling edge
  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonInterrupt, FALLING);

  //set up leds as outputs
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
  pinMode(LED4, OUTPUT);

  //welcome message displayed on lcd
  lcd.setCursor(0, 0);
  lcd.print("Welcome to...");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Binary Guessing");
  lcd.setCursor(0, 1);
  lcd.print("Game! :)");
  delay(3000);
  lcd.clear();
  lcd.print("Press the button");
  lcd.setCursor(0, 1);
  lcd.print("to start!");

  //disable interrupts
  noInterrupts(); 

  //configure timer 1
  TCCR1A = 0;                             //clear timer1 control registers
  TCCR1B = 0;
  TCNT1 = 0;                              //set timer1 counter value to 0
  OCR1A = 15999;                          //set the compare match value (16 MHz / 1000 Hz - 1)
  TCCR1B |= (1 << WGM12);                 //configure timer1 for ctc mode
  TCCR1B |= (1 << CS11) | (1 << CS10);    //set prescaler to 64
  TIMSK1 |= (1 << OCIE1A);                //enable timer1 compare match interrupt

  interrupts(); //enable interrupts
}

//loop function
void loop() {
  //switch statement to handle different game states
  switch (gameState) {

    //game state 1
    case WAIT_FOR_START:

      //wait for button to be pressed
      if (buttonPressed) {
        buttonPressed = false;

        //if button is pressed, set up lcd to begin the game
        if (gameState == WAIT_FOR_START || gameState == GAME_OVER) {
          resetGame();
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Starting game...");
          delay(1000);

          //generate random numbers
          generateRandomNumber(); 

          //start timer
          gameStartTime = millis(); 

          //enable timer1
          timerMode = TIMER_RUNNING;

          //transition to next state 
          gameState = WAIT_FOR_INPUT; 
        }
      }
      break;

    //game state 2
    case WAIT_FOR_INPUT:

      //if timer stops, the game is over
      if (timerMode == TIMER_DISABLED) {

        //display times up message
        lcd.clear(); 
        lcd.setCursor(0, 0); 
        lcd.print("Time's up!");
        delay(1000);
        lcd.clear();

        //display the score
        lcd.setCursor(0, 0);
        lcd.print("Score: ");
        lcd.print(score); 

        //display the accuracy
        lcd.setCursor(0, 1);
        lcd.print("Accuracy: ");
        lcd.print(calculateAccuracy()); 
        delay(6000); 

        //display end message
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Press the button");
        lcd.setCursor(0, 1);
        lcd.print("to play again!");

        //transition to game over state
        gameState = GAME_OVER; 

        //if the game if not yet over
      } else if (guessIndex >= numBits) {
        //player has entered a guess, display the result
        resultDisplayStartTime = millis();

        //transition to next state
        gameState = DISPLAY_RESULT; 
      } else {

        //check if the button is pressed
        if (buttonPressed) {
          buttonPressed = false;

          //turn on all leds
          analogWrite(LED1, 255);
          analogWrite(LED2, 255);
          analogWrite(LED3, 255);
          analogWrite(LED4, 255);
        }

        //elasped game time calculation
        unsigned long elapsedTime = millis() - gameStartTime;

        //elapsed time scaled to brightness level of leds
        int brightness = map(elapsedTime, 0, gameDuration, 255, 0);

        //decrease the brightness of the leds if the game is still on
        //compare brightness variable to the threshold value
        //if the brightness is more than the threshold value
        //pass to analogWrite(), else set to 0
        if (brightness > 0) {
          analogWrite(LED1, brightness1);
        } else {
          analogWrite(LED1, 0);
        }

        if (brightness > 63) {
          analogWrite(LED2, brightness - 63);
        } else {
          analogWrite(LED2, 0);
        }

        if (brightness > 127) {
          analogWrite(LED3, brightness - 127);
        } else {
          analogWrite(LED3, 0);
        }

        if (brightness > 191) {
          analogWrite(LED4, brightness - 191);
        } else {
          analogWrite(LED4, 0);
        }
      }
      break;

    //game state 3
    case DISPLAY_NUMBER:
      
      //random number generated as long as the game is ongoing
      if (millis() - currentNumberStartTime >= resultDisplayDuration) {
        generateRandomNumber();

        //transition to next state 
        gameState = WAIT_FOR_INPUT; 
      }
      break;

    case DISPLAY_RESULT:

      //random number is generated as long as the game is ongoing
      if (millis() - resultDisplayStartTime >= resultDisplayDuration) {
        generateRandomNumber();

        //transition to next state 
        gameState = DISPLAY_NUMBER; 
      }
      break;

    //game state 5
    case GAME_OVER:

      //if the button has been pressed, return to starting state
      if (buttonPressed) {
        buttonPressed = false;

        //transition to next state
        gameState = WAIT_FOR_START; 
      }
      break;
  }

  //keypad logic controls
  //function call from keypad library to check which key was pressed
  char key = keypad.getKey();

  //if the '#' key is pressed, check the guess
  if (key != NO_KEY && gameState == WAIT_FOR_INPUT) {
    if (key == '#') {
      checkGuess(); 
    } else {
      if (guessIndex < numBits) {

        //print guess to lcd and store guess
        lcd.setCursor(guessIndex + 7, 1); 
        lcd.print(key); 
        guess[guessIndex] = key; 
        guessIndex++;
      }
    }
  }
}

//interrupt service routine
//executed when timer1 compare match interrupt occurs
ISR(TIMER1_COMPA_vect) {

  //if the timer is in the running state
  if (timerMode == TIMER_RUNNING) {

    //current time calculated using millis()
    unsigned long currentTime = millis();

    //brightness variable 
    int brightness1 = 255;

    //time difference between current time and start time = 60 s
    if (currentTime - gameStartTime >= gameDuration) {

      //disable timer1
      timerMode = TIMER_DISABLED; 
    }
  }
}

//function to generate a random 4 bit binary number
void generateRandomNumber() {

  //prepare lcd
  lcd.clear(); 
  lcd.setCursor(0, 0); 
  lcd.print("Binary: ");

  //generate and display number
  answer = random(0, 16);
  printBinary(answer); 

  //print next statement
  lcd.setCursor(0, 1); 
  lcd.print("Guess: ");

  //update guess array and current time
  guessIndex = 0;
  currentNumberStartTime = millis();
}

//function to convert decimal to binary to display on lcd
void printBinary(byte decimalNumber) {
  for (byte i = 0; i < numBits; i++) {
    byte bit = (decimalNumber >> (numBits - 1 - i)) & 1; 
    lcd.print(bit); 
  }
}

//function to verify player's guesses
void checkGuess() {

  //clear space on lcd
  lcd.setCursor(0, 1); 
  lcd.print("            "); 
  guess[guessIndex] = '\0'; 

  //convert guess string to decimal
  byte decimalGuess = atoi(guess); 

  //if the guess is a valid answer, check if it is right or wrong
  if (decimalGuess == answer) {

    //correct
    Serial.println("Correct!"); 
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Correct!");
    delay(resultDisplayDuration);

    //increment score and accuracy variables 
    score += 2; 
    totalGuesses++;
    totalCorrectGuesses++; 
  } else {

    //wrong
    Serial.println("Wrong!"); 
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Wrong!");
    delay(resultDisplayDuration); 

    //increment score and accuracy variables
    if (score > 0) {
      score -= 1; 
    }
    totalGuesses++; 
    totalIncorrectGuesses++; 
  }

  //transition to display number state
  gameState = DISPLAY_NUMBER; 
}

//function to calculate player accuracy
//takes correct guesses divided by total guesses and converts to %
float calculateAccuracy() {
  if (totalGuesses > 0) {
    return ((float) totalCorrectGuesses / totalGuesses) * 100; 
  } else {
    return 0.0; 
  }
}

//function to reset game
//resets score, game and timer variables and leds
void resetGame() {
  score = 0;
  totalGuesses = 0;
  totalCorrectGuesses = 0;
  totalIncorrectGuesses = 0;
  guessIndex = 0;
  buttonPressed = false;
  gameState = WAIT_FOR_START;
  timerMode = TIMER_DISABLED;
  digitalWrite(LED1, LOW);
  digitalWrite(LED2, LOW);
  digitalWrite(LED3, LOW);
  digitalWrite(LED4, LOW);
}
