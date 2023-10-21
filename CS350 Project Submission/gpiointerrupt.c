/*
 * Copyright (c) 2015-2020, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== gpiointerrupt.c ========
 */
#include <stdint.h>
#include <stddef.h>
#include "I2C.h"
#include "UART2.h"
#include <ti/drivers/UART2.h>

#include "Timer.h"
#include <ti/drivers/GPIO.h>
#include "ti_drivers_config.h"

// I2C Global Variables
static const struct {
    uint8_t address;
    uint8_t resultReg;
    char *id;
} sensors[3] = {
    {0x48, 0x0000, "11X"},
    {0x49, 0x0000, "116"},
    {0x41, 0x0001, "006"}
};

uint8_t txBuffer[1];
uint8_t rxBuffer[2];
I2C_Transaction i2cTransaction;
I2C_Handle i2c;

// UART Global Variables
char output[64];
UART2_Handle uart;

// Timer Global Variables
Timer_Handle timer0;
volatile unsigned char TimerFlag = 0;

// Variables for temperature, set-point, and heat status
int16_t roomTemperature = 25; // Initial room temperature
int16_t setPoint = 22;        // Initial temperature set-point
int heatStatus = 0;           // Heat status (0 for off, 1 for on)

// Variable to count seconds since board reset
uint32_t seconds = 0;

// Flag to indicate button press
int buttonPressed = 0;

/* Function prototypes */
void initI2C(void);
void initUART(void);
void initTimer(void);
int16_t readTemp(void);
void timerCallback(Timer_Handle myHandle, int_fast16_t status);
void gpioButtonFxn(uint_least8_t index);

/* Initialize functions */
void initI2C(void)
{
    int8_t i, found;
    I2C_Params i2cParams;

    // Initialize I2C driver
    I2C_init();

    // Configure I2C driver parameters
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;

    // Open the I2C driver
    i2c = I2C_open(CONFIG_I2C_0, &i2cParams);
    if (i2c == NULL)
    {
        // Handle I2C open failure
        while (1);
    }

    // Scan for the connected temperature sensor
    i2cTransaction.writeBuf = txBuffer;
    i2cTransaction.writeCount = 1;
    i2cTransaction.readBuf = rxBuffer;
    i2cTransaction.readCount = 0;
    found = false;

    for (i = 0; i < 3; ++i)
    {
        i2cTransaction.slaveAddress = sensors[i].address;
        txBuffer[0] = sensors[i].resultReg;

        if (I2C_transfer(i2c, &i2cTransaction))
        {
            found = true;
            break;
        }
    }

    if (found)
    {
        // Temperature sensor detected
    }
    else
    {
        // Temperature sensor not found, handle accordingly
    }
}

void initUART(void)
{
    UART2_Params uartParams;

    // Configure UART2 driver parameters
    UART2_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_BINARY;
    uartParams.readDataMode = UART_DATA_BINARY;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.baudRate = 115200;

    // Open the UART2 driver
    uart = UART2_open(CONFIG_UART2_0, &uartParams);
    if (uart == NULL) {
        // Handle UART open failure
        while (1);
    }
}

void initTimer(void)
{
    Timer_Params params;

    // Initialize Timer driver
    Timer_init();

    // Configure Timer driver parameters
    Timer_Params_init(&params);
    params.period = 1000000; // 1 second period
    params.periodUnits = Timer_PERIOD_US;
    params.timerMode = Timer_CONTINUOUS_CALLBACK;
    params.timerCallback = timerCallback;

    // Open the Timer driver
    timer0 = Timer_open(CONFIG_TIMER_0, &params);
    if (timer0 == NULL) {
        // Handle Timer open failure
        while (1);
    }

    // Start the Timer
    if (Timer_start(timer0) == Timer_STATUS_ERROR) {
        // Handle Timer start failure
        while (1);
    }
}

int16_t readTemp(void)
{
    int16_t temperature = 0;
    i2cTransaction.readCount = 2;

    if (I2C_transfer(i2c, &i2cTransaction))
    {
        temperature = (rxBuffer[0] << 8) | (rxBuffer[1]);
        temperature *= 0.0078125;

        if (rxBuffer[0] & 0x80)
        {
            temperature |= 0xF000;
        }
    }
    else
    {
        // Handle temperature sensor read error
    }

    return temperature;
}

void timerCallback(Timer_Handle myHandle, int_fast16_t status)
{
    seconds++;

    // Check buttons every 200ms
    if (seconds % 2 == 0) {
        if (buttonPressed == 1) {
            setPoint++;
        } else if (buttonPressed == 2) {
            setPoint--;
        }
    }

    // Check temperature every 500ms
    if (seconds % 5 == 0) {
        int16_t currentTemperature = readTemp();
        if (currentTemperature > setPoint) {
            GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF);
            heatStatus = 0;
        } else {
            GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);
            heatStatus = 1;
        }
    }

    // Update UART output every second
    if (seconds % 10 == 0) {char uartOutput[32];
    snprintf(uartOutput, sizeof(uartOutput), "<%02d,%02d,%d,%04d>\n\r", roomTemperature, setPoint, heatStatus, seconds);
    UART2_write(uart, uartOutput, strlen(uartOutput), NULL); // Add NULL as the last argument
    }
}

void gpioButtonFxn(uint_least8_t index)
{
    if (buttonPressed == 0) {
        buttonPressed = 1;
    } else if (buttonPressed == 1) {
        buttonPressed = 2;
    } else {
        buttonPressed = 0;
    }
}

void *mainThread(void *arg0)
{
    // Call driver init functions
    GPIO_init();
    initUART();
    initI2C();
    initTimer();

    // Configure the LED and button
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_setConfig(CONFIG_GPIO_BUTTON_0, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);

    // Install Button callback
    GPIO_setCallback(CONFIG_GPIO_BUTTON_0, gpioButtonFxn);
    GPIO_enableInt(CONFIG_GPIO_BUTTON_0);

    // Main loop
    while (1) {
        // Continue with other tasks
    }
    return (NULL);
}


