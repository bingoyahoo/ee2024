Note: I am posting this for the benefits of students taking the NUS EE2024 module in future semesters because I believe in open-source learning. Anyway, this is posted after the semester was over because they did a plagiarism check. Your EE2024 project assignment will be different, but the fundamentals will probably be similar. Don't just copy and paste the code, most of these can be found in the lecture notes anyway. Make sure that you really understand what's going on inside your software. I did not include all the libraries files here.

=====================================================================
Read a writeup here: http://muggingsg.com/general/ee2024-assignment-2-guide-basic-embed-sys/

You can go to this Youtube link to see a video of our working board: https://youtu.be/NvzNnPhkiUU (it's loud so mute it if you want)

=====================================================================
The objective of this EE2024 assignment is to simulate a spacecraft STAR-T monitoring the Sun at close range using the LPC1769 and EA baseboard.

We will use three main sensors namely the accelerometer, light sensor and the temperature sensor on the baseboard to capture data about the physical environment surrounding STAR-T. The data will then be transmitted back at regular intervals via UART to a laptop/PC which is used to represent the Mission Control Centre (MCC) on Earth.

=====================================================================
Firstly, STAR-T consists of three mutually exclusive modes:

1.BASIC which represents normal operation. In this mode,
	a. The RGB LED will be set to blue.
	b. All 16 LEDs from the port expander PCA9532 are turned on.
	c. The three main sensors are sampled at intervals of 3 seconds.
	d. The OLED will display the latest sampled data from the three sensors.
	e. These values will also be sent to the MCC via wireless UART.

2. RESTRICTED which represents critical section when light intensity is above 2000 lux.
	a. The RGB LED will be set to red.
	b. All 16 LEDs from port expander PCA9532 are turned off.
	c. OLED stops displaying the sampled data but shows the character ‘R’ for each of them.
	d. If luminance level is below 2000 lux, one LED from the port expander PCA9532 will turn on every 250ms. 
		i. When all 16 of them have turned on, STAR-T re-enters BASIC mode.
		ii. However, if luminance level is above 2000 lux anytime during this period, then all 16 LEDs will be turned off and the counting sequence will be restarted.

3.EXTENDED which occurs when SW4 is pressed (Optional and up to you. We chose simple features.)
	a. The 16 LEDs on the port expander will be blinking to indicate that the mode is EXTENDED.
	b. The OLED changes the format of the sensor readings to a more readable one.
	c. A moving progress bar indicates that the program is running smoothly.
	d. The 7 Segment Display will be turned upside down for easier reading.
	e. The intervals between readings can be adjusted using the rotary sensor.
		i. Turning right increases the interval between readings.
		ii. Turning left decreases the intervals between readings.
		iii. The blinking rate of the 16 LEDs on the port expander will also change correspondingly to indicate the increase or decrease of reading intervals.

=====================================================================

TIPS:
1) Make sure that you really understand what is going on in your software. They will ask you many questions about it.
2) Ensure that your system doesn't crash no matter what because they will do rigorous testing. Our final system was simple but it would not break.
3) Get the basic requirements right before you start chasing extra features.
