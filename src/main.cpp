

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <timerISR.h>
#include <serialATmega.h>
#include <string.h>

void TimerISR()
{
    TimerFlag = 1;
}

inline unsigned char SetBit(unsigned char x, unsigned char k, unsigned char b)
{
    return (b ? (x | (0x01 << k)) : (x & ~(0x01 << k)));
}

inline unsigned char GetBit(unsigned char x, unsigned char k)
{
    return ((x & (0x01 << k)) != 0);
}

void ADC_init()
{
    ADMUX = (1 << REFS0);
    ADCSRA |= (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    // ADEN: setting this bit enables analog-to-digital conversion.
    // ADSC: setting this bit starts the first conversion.
    // ADATE: setting this bit enables auto-triggering. Since we are
    // in Free Running Mode, a new conversion will trigger whenever
    // the previous conversion completes.
}

unsigned int ADC_read(unsigned char chnl)
{
    uint8_t low, high;
    ADMUX = (ADMUX & 0xF8) | (chnl & 7);
    ADCSRA |= 1 << ADSC;
    while ((ADCSRA >> ADSC) & 0x01)
    {
    }
    low = ADCL;
    high = ADCH;
    return ((high << 8) | low);
}

int joyStick_y;                                                                   // Channel 2 is A2 (vertical)
int joyStick_x;                                                                   // Channel 3 is A3 (horizontal)
int phases[8] = {0b0001, 0b0011, 0b0010, 0b0110, 0b0100, 0b1100, 0b1000, 0b1001}; // 8 phases of the stepper motor step

int moveNum = 0;
int combo[4] = {3, 4, 1, 2}; // up, down, left, right
int myCombo[4] = {0, 0, 0, 0};

int counter;
int phaseNum = 0;
int stepCount = 0;
bool TurnRight = 0;
bool isReset = 0;

// directions[] and outDir() replaces nums[] and outNum() from previous exercise.
// they behave the same, the only difference is outDir() outputs 4 direction and a neutral int directions[5] = {};
// TODO: complete the array containg the values needed forthe 7 segments for each of the 4 directions
// a b c d e f g
// TODO: display the direction to the 7-seg display. HINT: will be very similar to outNum()

void setLEDs(int counter)
{
    PORTC = (PORTC & 0xFC) | (counter & 0x03);
}

void motoRight() // unlocks
{
    //& first to reset pins 2-5 but not 0-1 then | with phase shifted left 2 to assign the right value to pins 2-5
    // increment to next phase
    PORTB = (PORTB & 0x03) | (phases[phaseNum] << 2);
    ++phaseNum;
    if (phaseNum > 7)
    {
        phaseNum = 0;
    }
}

void motoLeft() // locks
{
    PORTB = (PORTB & 0x03) | (phases[phaseNum] << 2);
    --phaseNum;
    if (phaseNum < 0)
    {
        phaseNum = 7;
    }
}

bool isCorrect()
{
    for (int i = 0; i < 4; i++)
    {
        if (myCombo[i] != combo[i])
        {
            return false;
        }
    }
    return true;
}

int dirs[6] = {
    0b11111110, // zero
    0b00001110, //  L (left)   -> D4, D3, D2            1
    0b00000101, // r (right)  -> D3, B0                 2
    0b00111110, // U (up)     -> D2, D3, D4, D5, D6     3
    0b00111101, // d (down)   -> D3, D4, D5, D6, B0     4
    0b00000001, // centered    ->  B0                   5
};

void outDir(int dir)
{
    PORTD = dirs[dir] << 1;                     // shift to left by one to align with D2-7 ports
    PORTB = SetBit(PORTB, 0, dirs[dir] & 0x01); // Set B0 based on the LSB of dirs[dir] only for centered, d, and r
}

int getDir()
{
    if (((572 - 15 <= joyStick_y) && (joyStick_y <= 572 + 15)) && ((546 - 15 <= joyStick_x) && (joyStick_x <= 546 + 15)))
    {
        return 0; // centered
    }
    else if (joyStick_x < 546 - 15)
    {
        return 1; // left
    }
    else if (joyStick_x > 546 + 15)
    {
        return 2; // right
    }
    else if (joyStick_y > 572 + 15)
    {
        return 3; // up
    }
    else if (joyStick_y < 572 - 15)
    {
        return 4; // down
    }
}

enum states
{
    INIT,
    Centered,
    JoyMove,
    checkCombo,
    MotoMove,
    resetPass,

} state; // TODO: finish the enum for the SM

void Tick()
{
    // State Transistions
    // TODO: complete transitions
    switch (state)
    {
    case INIT:
        PORTB = SetBit(PORTB, 0, 1);
        PORTC = SetBit(PORTC, 0, 0);
        PORTC = SetBit(PORTC, 1, 0);
        moveNum = 0;
        joyStick_y = 546;
        joyStick_x = 572;
        counter = 0;
        stepCount = 0;
        phaseNum = 0;

        state = Centered;

        break;

    case Centered:
        joyStick_y = ADC_read(2); // Channel 2 is A2 (vertical)
        joyStick_x = ADC_read(3); // Channel 3 is A3 (horizontal)

        if (TurnRight & (!GetBit(PINC, 4))) // Check if system is unlocked and joystick is pressed
        {
            isReset = !isReset;
            for (int i = 0; i < 4; i++)
            {
                combo[i] = 0;
            }
            moveNum = 0; // Reset moveNum for new passcode recording
            PORTB = SetBit(PORTB, 1, 1);
            state = resetPass; // Transition to new passcode recording
        }
        else
        {

            if (!getDir() & (!isReset))
            {
                if (moveNum == 4) // Array is fully populated
                {
                    state = checkCombo; // Transition to checkCombo state
                }
                else
                {
                    state = Centered; // Go back to Centered state to continue filling
                }
            }
            else
            {
                myCombo[moveNum] = getDir();
                state = JoyMove;
            }
        }

        break;

    case JoyMove:
        joyStick_y = ADC_read(2); // Channel 2 is A2 (vertical)
        joyStick_x = ADC_read(3); // Channel 3 is A3 (horizontal)

        if (!getDir()) // checks if the the joystick is centered
        {
            moveNum++; // increment on transition back to centered
            if (isReset)
            {
                state = resetPass;
            }
            else
            {
                state = Centered;
            }
        }
        else
        {
            state = JoyMove;
        }

        break;

    case checkCombo:
        if (isCorrect())
        {
            counter = 0;
            state = MotoMove;
        }
        else
        {
            if (counter < 41)
            {
                state = checkCombo;
            }
            else
            {
                state = INIT;
            }
            counter++;
        }
        break;

    case MotoMove:
        setLEDs(3);
        TimerSet(1);
        if (stepCount < 1025)
        {
            state = MotoMove; // Continue in MotoMove
        }
        else
        {
            TurnRight = !TurnRight;
            TimerSet(100);
            setLEDs(0);
            stepCount = 0; // Reset step counter after 128 steps
            phaseNum = 0;  // Reset phase counter
            state = INIT;  // Transition back to INIT state
        }
        break;

    case resetPass:
        joyStick_y = ADC_read(2); // Channel 2 is A2 (vertical)
        joyStick_x = ADC_read(3); // Channel 3 is A3 (horizontal)
        if (getDir() & (moveNum < 4))
        {
            combo[moveNum] = getDir();
            state = JoyMove;
        }
        else if (moveNum == 4)
        {
            moveNum = 0;
            PORTB = SetBit(PORTB, 1, 0);
            isReset = false;
            state = Centered;
        }
        break;

    default:
        state = INIT;
        break;
    }
    // State Actions============================================================
    // TODO: complete transitions
    switch (state)
    {
    case INIT:
        break;

    case Centered:

        outDir(5);

        break;

    case JoyMove:
        if (moveNum < 4)
        {
            setLEDs(moveNum + 1);
        }
        else
        {
            setLEDs(3);
        }

        if (getDir() == 1) // left
        {
            outDir(1);
        }
        else if (getDir() == 2) // right
        {
            outDir(2);
        }
        else if (getDir() == 3) // up
        {
            outDir(3);
        }
        else if (getDir() == 4) // down
        {
            outDir(4);
        }

        break;

    case checkCombo:
        if (counter % 5 == 0) // flashes the LEDs
        {
            setLEDs(3);
        }
        else
        {
            setLEDs(0);
        }
        break;

    case MotoMove:
        if (!TurnRight)
        {
            motoRight();
        }
        else
        { // Moving left
            motoLeft();
        }
        stepCount++; // Increment the step count
        break;

    case resetPass:

        break;

    default:
        break;
    }
}
int main(void)
{
    DDRC = 0x03;  // Input except for A0 and A1 which are outputs
    PORTC = 0xFC; // Enable pull-ups

    DDRD = 0xFF;
    PORTD = 0x00; // Initialize PORTD

    DDRB = 0xFF;  // Output
    PORTB = 0x00; // Initialize PORTB

    ADC_init(); // initializes the analog to digital converter
    state = INIT;

    TimerSet(100);
    TimerOn();
    // serial_init(9600);
    while (1)
    {
        Tick(); // Execute one synchSM tick

        while (!TimerFlag)
        {
            // signed joyStick_y = ADC_read(2); // Channel 2 is A2 (vertical)
            // signed joyStick_x = ADC_read(3); // Channel 3 is A3 (horizontal)
            // signed button = GetBit(PINC, 4);

            // Output the values to the serial monitor
            // serial_println(joyStick_y, 10); // Print vertical axis value (Y-axis)
            // serial_println(joyStick_x, 10); // Print horizontal axis value (X-axis)
            // serial_println(button, 10);

        } // Wait for SM period
        TimerFlag = 0; // Lower flag
    }

    return 0;
}