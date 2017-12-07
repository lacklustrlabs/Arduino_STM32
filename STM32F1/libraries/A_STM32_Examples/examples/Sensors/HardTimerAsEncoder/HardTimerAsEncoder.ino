/*
 * Hardware Timer as an Encoder interface. 
 * 
 * The STM32 Timers have the possibility of being used as an encoder interface. 
 * This can be both a quadrature encoder (mode 3) or pulse encoder with a signal to give direction (modes 1 and 2). 
 * The default behavior is for quadrature encoder. 
 * 
 * To avoid overflowing the encoder (which may or may not happen (although with 16 bits, it's likely), the following code 
 * will interrupt every time the number of pulses that each revolution gives to increment/decrement a variable (revolutions). 
 * 
 * This means that the total number of pulses given by the encoder will be (revolutions * PPR) + timer.getCount()
 * 
 * Attached is also a bit of code to simulate a quadrature encoder. 
 * To test this library, make the connections as below: 
 * 
 * TIMER4 inputs -> current limiting resistor -> Digital Pins used to simulate.
 * PB6/D16 -> 1K Ohm -> PA7/D4
 * PB7/D15 -> 1K Ohm -> PA6/D5
 * 
 * COUNTING DIRECTION: 
 * 0 means that it is upcounting, meaning that Channel A is leading Channel B
 * 
 * EDGE COUNTING: 
 * 
 * mode 1 - only counts pulses on channel B
 * mode 2 - only counts pulses on Channel A
 * mode 3 - counts on both channels. 
 * 
 */

#include "HardwareTimer.h"

//Encoder simulation stuff
//NOT NEEDED WHEN CONNECTING A REAL ENCODER

const uint8_t output_a = PA7; // D4
const uint8_t output_b = PA6; // D5

//Encoder stuff
const uint8_t input_a = PB6; // D16 this should be the pin of timer 4 channel 1
const uint8_t input_b = PB7; // D15 this should be the pin of timer 4 channel 2

//Pulses per revolution
#define PPR   1024

HardwareTimer timer(4);

volatile uint32_t revolutions = 0;

void overflowInterrupt(){
  if (timer.getDirection()){
    revolutions--;
  } else{
    revolutions++;
  }
}

void simulateStep(uint8_t step){
  static const unsigned char states[] = {0,1,3,2}; //{0b00,0b01,0b11,0b10}; // 0bBA
  digitalWrite(output_a, (states[step&3] & 0x01));
  digitalWrite(output_b, (states[step&3] >> 1));
}

/*
ENCODER SIMULATION PART. 


*/  
void simulate() {
  static unsigned long updatePeriod = 100; //update period.
  static const uint16_t periodDelta = 50;  //inc/dec updatePeriod with this 
  static unsigned long lastSimulationAt = 0;  //time variable for millis()
  static unsigned char mode = 0;  //to issue steps...
  static unsigned char dir = 'F'; // direction of movement of the encoder.

/*
 * 
 * Protocol... 
 * 
 * if received F - Move forward. 
 * if received B - Move BackWard
 * if received 1 - Mode 1 (Channel B counts)
 * if received 2 - Mode 2 (Channel A counts)
 * if received 3 - Mode 3 (Counts on both channels)
 * if received 4 - Change prescaler to 4
 * if received 0 - change prescaler to 1
 * if received - - Decrease Speed
 * if received + - Increase Speed
*/

//take care of comms...
  if (Serial.available() > 0) {
    char received = Serial.read();
    if (received == 'F' || received == 'R') dir = received; //direction. Forward or Reverse.
    if (received == '1') timer.setEdgeCounting(TIMER_SMCR_SMS_ENCODER1); //count only the pulses from input 1
    if (received == '2') timer.setEdgeCounting(TIMER_SMCR_SMS_ENCODER2); //count only the pulses from input 2
    if (received == '3') timer.setEdgeCounting(TIMER_SMCR_SMS_ENCODER3); //count on both channels (default of the lib).
    if (received == '4') timer.setPrescaleFactor(4); //only updates on overflow, so you need to wait for an overflow. Not really used...  
    if (received == '0') timer.setPrescaleFactor(1); //only updates on overflow, so you need to wait for an overflow. 
    if (received == '-') updatePeriod+=periodDelta;  //decrease speed.
    if (received == '+') {
      if (updatePeriod <= periodDelta) updatePeriod = 50; //smallest is 50 ms. 
      else updatePeriod-=periodDelta;
    }
  }
  
//simulate encoder pulses.   
  
  if ( millis() - lastSimulationAt >= updatePeriod) { 
    lastSimulationAt = millis(); //prepare next
    
    if (dir == 'F')  mode++;
    if (dir == 'R')  mode--;

    simulateStep(mode);
  }
}

void setup() {
  Serial.begin(115200);
  while(!Serial)
    ;
  //define the Timer channels as inputs. 
  pinMode(input_a, INPUT_PULLUP);  //channel A
  pinMode(input_b, INPUT_PULLUP);  //channel B

//configure timer as encoder
  //set mode, the channel is not used when in this mode.
  // You must, however, use a valid channel number.
  timer.setMode(TIMER_CH1, TIMER_ENCODER); 
  timer.pause(); //stop... 
  timer.setPrescaleFactor(1); //normal for encoder to have the lowest or no prescaler. 
  timer.setOverflow(PPR);     //use this to match the number of pulse per revolution of the encoder. Most industrial use 1024 single channel steps. 
  timer.setCount(0);          //reset the counter. 
  timer.setEdgeCounting(TIMER_SMCR_SMS_ENCODER3); //or TIMER_SMCR_SMS_ENCODER1 or TIMER_SMCR_SMS_ENCODER2. This uses both channels to count and ascertain direction. 
  timer.refresh();            // set the EGR::UG bit
  timer.attachInterrupt(0, overflowInterrupt); //channel must be 0 here.  
  
  timer.resume();                 //start the encoder... 

//Setup encoder simulator  
  pinMode(output_a, OUTPUT);
  pinMode(output_b, OUTPUT);
  simulateStep(0);
}

void loop() {
  //encoder code
  static uint32_t lastMessageAt=0; //variable for status updates... 
  if (millis() - lastMessageAt >= 1000) { 
    Serial.print("Direction:");
    Serial.print(timer.getDirection()?'R':'F');
    Serial.print(", Full Revs: ");
    Serial.print(revolutions);
    Serial.print(", ");
    Serial.print(timer.getCount()); 
    Serial.println(" counts");
    lastMessageAt = millis();
  }
  
  // simulator code 
  simulate();
}
