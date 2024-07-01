Blue Pill LCD and Servo Control Project
This project demonstrates how to use an STM32 Blue Pill microcontroller to read a potentiometer, display its value on an LCD, and control a 180-degree servo motor using PWM.

Components
STM32 Blue Pill
ST7735 LCD Display
10kΩ Potentiometer
180-degree Servo Motor
Breadboard and connecting wires
Circuit Diagram
Potentiometer Connections:

VCC to 3.3V
GND to GND
Wiper (middle pin) to ADC input pin (e.g., PA0)
LCD Display Connections:

Connect the LCD to the SPI pins of the Blue Pill
Servo Motor Connections:

VCC to 5V
GND to GND
Signal pin to PWM output pin (e.g., PB9 for TIM1 CH4)
Functionality
ADC Read:

Reads the potentiometer value using ADC1
PWM Control:

Maps the ADC value to a suitable pulse width for the servo motor
LCD Display:

Displays the potentiometer's numerical value
Draws a bar representing the potentiometer's value for visual representation
Usage
Connect all components as per the circuit diagram.
Load the project code into STM32CubeIDE.
Compile and flash the code to the Blue Pill.
Adjust the potentiometer and observe the changes on the LCD and the servo motor's movement.
Future Enhancements
Add more interactive elements on the LCD.
Integrate additional sensors or input devices.
Implement a more sophisticated control algorithm for the servo motor.
