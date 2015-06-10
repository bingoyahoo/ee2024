/*****************************************************************************
 *   A demo example using several of the peripherals on the base board
 *
 *   Copyright(C) 2011, EE2024
 *   All rights reserved.
 *
 ******************************************************************************/

#include "lpc17xx_pinsel.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx_timer.h"
#include "lpc17xx_uart.h"

#include <stdio.h>
#include "joystick.h"
#include "pca9532.h"
#include "acc.h"
#include "oled.h"
#include "rgb.h"
#include "led7seg.h"
#include "light.h"
#include "temp.h"
#include "string.h"

#define RGB_RED   0x01
#define RGB_BLUE  0x02
#define TIME_UNIT 250;
static char* msg = NULL;

const uint32_t lightLoLimit = 0;
const uint32_t lightHiLimit = 2000;
static const int FLARE_INTENSITY = 2000;

static uint32_t TICK_RATE_ONE_SEC = 1000;

volatile uint32_t msTicks; // counter for 1ms SysTicks
volatile uint32_t oneSecondTicks, unitTicks, threeSecondsTicks,
		readingResSensorsTicks, sw4PressedTicks;

int firstRun = 1;

int8_t x = 0, y = 0, z = 0;
volatile int isSwitchingToRestricted = 0;
volatile int isTriggerPressed = 0;

uint32_t temperature = 0;
uint32_t light = 0;
unsigned char result[100] = "";
volatile uint16_t countSafe = 0; // also uses as 16LED LEDMask

//Strings for OLED
int8_t OLED_EXT_MODE[15];
int8_t OLED_X[15];
int8_t OLED_Y[15];
int8_t OLED_Z[15];
int8_t OLED_LIGHT[15];
int8_t OLED_TEMPERATURE[15];

typedef enum {
	MODE_BASIC, MODE_RESTRICTED, MODE_EXTENDED
} system_mode_t;
volatile system_mode_t mode;

static void init_ssp(void) {
	SSP_CFG_Type SSP_ConfigStruct;
	PINSEL_CFG_Type PinCfg;

	/*
	 * Initialize SPI pin connect
	 * P0.7 - SCK;
	 * P0.8 - MISO
	 * P0.9 - MOSI
	 * P2.2 - SSEL - used as GPIO
	 */
	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 0;
	PinCfg.Pinnum = 7;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 8;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 9;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Funcnum = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 2;
	PINSEL_ConfigPin(&PinCfg);

	SSP_ConfigStructInit(&SSP_ConfigStruct);

	// Initialize SSP peripheral with parameter given in structure above
	SSP_Init(LPC_SSP1, &SSP_ConfigStruct);

	// Enable SSP peripheral
	SSP_Cmd(LPC_SSP1, ENABLE);

}

static void init_i2c(void) {
	PINSEL_CFG_Type PinCfg;

	/* Initialize I2C2 pin connect */
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 10;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 11;
	PINSEL_ConfigPin(&PinCfg);

	// Initialize I2C peripheral
	I2C_Init(LPC_I2C2, 100000);

	/* Enable I2C1 operation */
	I2C_Cmd(LPC_I2C2, ENABLE);
}

static void init_GPIO(void) {

	//Initialize button sw4
	PINSEL_CFG_Type PinCfg;
	PinCfg.Funcnum = 0;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 1;
	PinCfg.Pinnum = 31;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(1, 1 << 31, 0);

	//Initialize button sw3
	PinCfg.Funcnum = 0;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 10;
	PINSEL_ConfigPin(&PinCfg);
	GPIO_SetDir(2, 1 << 10, 0);

}

//SysTick_Handler - just increment SysTick counter
void SysTick_Handler(void) {
	msTicks++;
}

uint32_t getTicks() {
	return msTicks;
}

// noted there is conflict with OLED helper function with rgb helper function
// max char per line: 15
// Dimension of one char on OLED: 5*7
static void OLED_printStr(unsigned char* result) {
	oled_clearScreen(OLED_COLOR_BLACK);
	oled_putString(0, 0, result, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
}

void readSensors(uint32_t* temperature, uint32_t* light, int8_t* x, int8_t* y,
		int8_t* z) {
	*temperature = temp_read();
	*light = light_read();
	acc_read(&*x, &*y, &*z);
}

void setRGB(uint8_t ledMask) {
	if (ledMask == RGB_RED) {
		GPIO_SetValue(2, (1 << 0));
	} else {
		GPIO_ClearValue(2, (1 << 0));
	}
	if (ledMask == RGB_BLUE) {
		GPIO_SetValue(0, (1 << 26));
	} else {
		GPIO_ClearValue(0, (1 << 26));
	}
}

void incrementLED( num) {
	while (num) {
		if (countSafe != 65535) {
			countSafe = (countSafe << 1) + 1;
		}
		num--;
	}
	pca9532_setLeds(countSafe, 0xffff);
}

//EINT3 Interrupt Handler, GPIO0 and GPIO2 interrupts share the same position in NVIC with EINT3 - pg 24 GPIO notes
void EINT3_IRQHandler(void) {
	//light sensor
	if ((LPC_GPIOINT->IO2IntStatF >> 5) & 0x1) {
		countSafe = 0;
		if (mode == MODE_BASIC) {
			isSwitchingToRestricted = 1;
		}
		unitTicks = msTicks;
		mode = MODE_RESTRICTED;
		LPC_GPIOINT->IO2IntClr = (1 << 5);
	}
	if ((LPC_GPIOINT->IO2IntStatF >> 10) & 0x1) {
		isTriggerPressed = 1;
		LPC_GPIOINT->IO2IntClr = (1 << 10);
	}
}

void pinsel_uart3(void) {
	PINSEL_CFG_Type PinCfg;
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 0;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 1;
	PINSEL_ConfigPin(&PinCfg);
}

void init_uart(void) {
	UART_CFG_Type uartCfg;
	uartCfg.Baud_rate = 115200;
	uartCfg.Databits = UART_DATABIT_8;
	uartCfg.Parity = UART_PARITY_NONE;
	uartCfg.Stopbits = UART_STOPBIT_1;
	//pin select for uart3
	pinsel_uart3();
	//supply power and setup working parts for uart3
	UART_Init(LPC_UART3, &uartCfg);
	//enable transmit for uart3
	UART_TxCmd(LPC_UART3, ENABLE);
}

int main(void) {
	/* Enable and setup SysTick Timer at a periodic rate 1 msec*/
	if (SysTick_Config(SystemCoreClock / 1000)) {
		while (1)
			;  // Capture error
	}
	oneSecondTicks = msTicks;	// read current tick counter
	threeSecondsTicks = msTicks;	// read current tick counter
	unitTicks = msTicks;	// read current tick counter
	unitTicks = msTicks;	// read current tick counter
	sw4PressedTicks = msTicks;	// read current tick counter

	int8_t xoff = 0;
	int8_t yoff = 0;
	int8_t zoff = 0;
	int numToIncrement = 0;

	int counter7Seg = 1;
	char counter7SegDisplay = '0';
	Bool invert7Seg = FALSE;

	uint8_t btnSW4 = 1;

	init_i2c();
	init_ssp();
	init_GPIO();
	init_uart();

	pca9532_init();
	joystick_init();
	acc_init();
	oled_init();
	led7seg_init();
	temp_init(getTicks);
	light_enable();
	rgb_init();

	// Assume base board in zero-g position when reading first value.
	acc_read(&x, &y, &z);
	xoff = 0 - x;
	yoff = 0 - y;
	zoff = 0 - z;

	/* ---- Speaker ------> */
	GPIO_SetDir(2, 1 << 0, 1);
	GPIO_SetDir(2, 1 << 1, 1);

	GPIO_SetDir(0, 1 << 27, 1);
	GPIO_SetDir(0, 1 << 28, 1);
	GPIO_SetDir(2, 1 << 13, 1);
	GPIO_SetDir(0, 1 << 26, 1);
	//GPIO_SetDir(2, 1 << 5, 0);

	GPIO_ClearValue(0, 1 << 27); //LM4811-clk
	GPIO_ClearValue(0, 1 << 28); //LM4811-up/dn
	GPIO_ClearValue(2, 1 << 13); //LM4811-shutdn

	// Setup light limit for triggering interrupt
	light_setRange(LIGHT_RANGE_4000);
	light_setLoThreshold(lightLoLimit);
	light_setHiThreshold(lightHiLimit);
	light_setIrqInCycles(LIGHT_CYCLE_1);
	light_clearIrqStatus();

	LPC_GPIOINT->IO2IntClr = 1 << 5;
	LPC_GPIOINT->IO2IntEnF |= 1 << 5; //light sensor

	LPC_GPIOINT->IO2IntClr = 1 << 10; //SW3
	LPC_GPIOINT->IO2IntEnF |= 1 << 10; //switch
	NVIC_EnableIRQ(EINT3_IRQn);

	oled_clearScreen(OLED_COLOR_BLACK);
	led7seg_setChar('0', FALSE);

	if (light_read() < FLARE_INTENSITY) {
		mode = MODE_BASIC;
	} else {
		mode = MODE_RESTRICTED;
	}
	while (1) {
		//increment 7 segment
		if ((msTicks - oneSecondTicks) >= TICK_RATE_ONE_SEC) {
			oneSecondTicks = msTicks;
			counter7SegDisplay = '0' + counter7Seg % 10;
			led7seg_setChar(counter7SegDisplay, invert7Seg);
			counter7Seg = (counter7Seg + 1) % 10;
		}
		if (isTriggerPressed) {
			isTriggerPressed = 0;
			readSensors(&temperature, &light, &x, &y, &z);
			x = x + xoff;
			y = y + yoff;
			z = z + zoff;
			sprintf(result, "L%d_T%.1f_AX%d_AY%d_AZ%d\r", light,
					temperature / 10.0, x, y, z);
			OLED_printStr(result);
			UART_Send(LPC_UART3, (uint8_t *) result, strlen(result), BLOCKING);
		}
		btnSW4 = (GPIO_ReadValue(1) >> 31) & 0x01;
		if ((btnSW4 == 0) && (msTicks - sw4PressedTicks >= 500)) {
			sw4PressedTicks = msTicks;
			if (mode == MODE_BASIC || mode == MODE_RESTRICTED) {
				mode = MODE_EXTENDED;
				sprintf(result, "Entering Extended Mode");
			} else {
				mode = MODE_BASIC;
				sprintf(result, "Leaving Extended Mode");
			}
			invert7Seg = !invert7Seg;
			OLED_printStr(result);
		}
		switch (mode) {
		case MODE_BASIC:
			setRGB(RGB_BLUE); //set blue for BASIC
			pca9532_setLeds(0xffff, 0x00);
			if (((msTicks - threeSecondsTicks) >= 3000) || (firstRun)) {
				threeSecondsTicks = msTicks;
				firstRun = 0;
				readSensors(&temperature, &light, &x, &y, &z);
				x = x + xoff;
				y = y + yoff;
				z = z + zoff;
				sprintf(result, "L%d_T%.1f_AX%d_AY%d_AZ%d\r", light,
						temperature / 10.0, x, y, z);
				OLED_printStr(result);
				UART_Send(LPC_UART3, (uint8_t *) result, strlen(result),
						BLOCKING);
			}
			break;
		case MODE_RESTRICTED:
			light_clearIrqStatus();
			setRGB(RGB_RED);
			if (isSwitchingToRestricted) {
				msg =
						"Solar Flare Detected. Scheduled Telemetry is Temporarily Suspended.\r";
				UART_Send(LPC_UART3, (uint8_t *) msg, strlen(msg), BLOCKING);
				isSwitchingToRestricted = 0;
				pca9532_setLeds(0x0000, 0xFFFF);
			}
			if ((msTicks - readingResSensorsTicks) >= 3000) {
				readingResSensorsTicks = msTicks;
				sprintf(result, "L%c_T%c_AX%c_AY%c_AZ%c\r", 'R', 'R', 'R', 'R',
						'R');
				OLED_printStr(result);
			}
			if ((numToIncrement) = ((msTicks - unitTicks) / 250)) {
				if (light_read() < FLARE_INTENSITY) {
					incrementLED(numToIncrement);
				}
				unitTicks = msTicks;
			} else if (countSafe == 0) {
				pca9532_setLeds(0x0000, 0xFFFF);
			}
			if (countSafe >= 65535) {
				mode = MODE_BASIC;
				msg =
						"Space Weather is Back to Normal. Scheduled Telemetry Will Now Resume.\r";
				UART_Send(LPC_UART3, (uint8_t *) msg, strlen(msg), BLOCKING);
				countSafe = 0;
			}
			break;
		case MODE_EXTENDED:
			if (((msTicks - threeSecondsTicks) >= 3000) || (firstRun)) {
						threeSecondsTicks = msTicks;
			oled_clearScreen(OLED_COLOR_BLACK);
			sprintf(OLED_EXT_MODE, "<In Extension>");
			sprintf(OLED_LIGHT, "L=%d", light);
			sprintf(OLED_TEMPERATURE, "TEMP=%.1f", temperature);
			sprintf(OLED_X, "X=%d", x);
			sprintf(OLED_Y, "Y=%d", y);
			sprintf(OLED_Z, "Z=%d", z);
			oled_putString(0, 0, (uint8_t *) OLED_EXT_MODE, OLED_COLOR_WHITE,
					OLED_COLOR_BLACK);
			//oled_putStar(0,8, 0x7f, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
			oled_putString(0, 10, (uint8_t *) OLED_LIGHT, OLED_COLOR_WHITE,
					OLED_COLOR_BLACK);
			oled_putString(0, 20, (uint8_t *) OLED_TEMPERATURE,
					OLED_COLOR_WHITE, OLED_COLOR_BLACK);
			oled_putString(0, 30, (uint8_t *) OLED_X, OLED_COLOR_WHITE,
					OLED_COLOR_BLACK);
			oled_putString(0, 40, (uint8_t *) OLED_Y, OLED_COLOR_WHITE,
					OLED_COLOR_BLACK);
			oled_putString(0, 50, (uint8_t *) OLED_Z, OLED_COLOR_WHITE,
					OLED_COLOR_BLACK);
			}
			setRGB(RGB_RED);
			break;
		}

	}

}

void check_failed(uint8_t *file, uint32_t line) {
	/* User can add his own implementation to report the file name and line number,
	 ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

	/* Infinite loop */
	while (1)
		;
}
