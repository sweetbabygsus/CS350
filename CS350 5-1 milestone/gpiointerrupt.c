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
#include <ti/drivers/Timer.h>
#include <unistd.h>
#include <string.h>

/* Driver Header files */
#include <ti/drivers/GPIO.h>

/* Driver configuration */
#include "ti_drivers_config.h"

#define DOT_DURATION 50000   // Duration of a dot in microseconds
#define DASH_DURATION 3 * DOT_DURATION  // Duration of a dash is 3 times a dot
#define SYMBOL_PAUSE 3 * DOT_DURATION  // Pause between symbols (3 dots)
#define LETTER_PAUSE 7 * DOT_DURATION  // Pause between letters (7 dots)
#define WORD_PAUSE 15 * DOT_DURATION   // Pause between words (15 dots)

static const char *message = "SOS";  // Initial message
static int messageIndex = 0;        // Index to track the message characters
static int dotCount = 0;            // Counter for dots
static int buttonPressed = 0;       // Flag to track button press

/* Function to toggle LEDs based on Morse code */
void toggleMorseLED(char symbol) {
    switch (symbol) {
        case 'S':
            // Blink S (3 short dots)
            GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON); // Green LED ON
            usleep(DOT_DURATION);
            GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF); // Green LED OFF
            usleep(DOT_DURATION);
            GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON); // Green LED ON
            usleep(DOT_DURATION);
            GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_OFF); // Green LED OFF
            usleep(SYMBOL_PAUSE);
            break;
        case 'O':
            // Blink O (3 long dashes)
            GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_ON); // Red LED ON
            usleep(DASH_DURATION);
            GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_OFF); // Red LED OFF
            usleep(DOT_DURATION);
            GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_ON); // Red LED ON
            usleep(DASH_DURATION);
            GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_OFF); // Red LED OFF
            usleep(DOT_DURATION);
            GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_ON); // Red LED ON
            usleep(DASH_DURATION);
            GPIO_write(CONFIG_GPIO_LED_1, CONFIG_GPIO_LED_OFF); // Red LED OFF
            usleep(SYMBOL_PAUSE);
            break;
        case ' ':
            // Pause between words
            usleep(WORD_PAUSE);
            break;
    }
}

/* Timer callback function */
void timerCallback(Timer_Handle myHandle, int_fast16_t status)
{
    // Toggle Morse code LEDs based on the message
    char symbol = message[messageIndex];
    if (symbol == '\0') {
        // End of message, restart
        messageIndex = 0;
        dotCount = 0;
    } else {
        // Toggle LEDs for the current symbol
        toggleMorseLED(symbol);
        if (symbol == ' ' || dotCount >= 3) {
            // Pause for letters and words or after 3 dots
            usleep(LETTER_PAUSE);
        } else {
            // Pause between dots and dashes
            usleep(DOT_DURATION);
        }
        // Move to the next symbol
        messageIndex++;
        dotCount = (symbol == ' ') ? 0 : dotCount + 1;
    }
}

/*
 *  ======== gpioButtonFxn0 ========
 *  Callback function for the GPIO interrupt on CONFIG_GPIO_BUTTON_0.
 *
 *  Note: GPIO interrupts are cleared prior to invoking callbacks.
 */

/* Callback function for the GPIO interrupt on CONFIG_GPIO_BUTTON_0 */
void gpioButtonFxn0(uint_least8_t index)
{

    //Toggle the buttonPRessed flag to switch between message
    buttonPressed = 1;
}

/* Function prototype for initTimer */
void initTimer(void);


/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    /* Call driver init functions */
    GPIO_init();

    /* Configure the LED and button pins */
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_setConfig(CONFIG_GPIO_LED_1, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_setConfig(CONFIG_GPIO_BUTTON_0, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

    /* Turn on user LED */
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);

    /* Install Button callback */
    GPIO_setCallback(CONFIG_GPIO_BUTTON_0, gpioButtonFxn0);

    /* Enable interrupts */
    GPIO_enableInt(CONFIG_GPIO_BUTTON_0);

    /* Initialize and start the timer */
    initTimer();

    /* enter the main loop here */
    while(1) {
        // Check if button was pressed
        if (buttonPressed) {
            //Toggle between "SOS" and "OK"
            if (strcmp(message, "SOS") == 0) {
                message = "OK";
            } else {
                message = "SOS";
            }
            //Reset flags and timers
            buttonPressed = 0;
            messageIndex = 0;
            dotCount = 0;
            usleep(WORD_PAUSE);
        }
    }
}

/* Initialize and start the timer */
void initTimer(void)
{
    Timer_Handle timer0;
    Timer_Params params;

    // Initialize the Timer driver
    Timer_init();

    // Initialize Timer Parameters
    Timer_Params_init(&params);
    params.period = 1000000;
    params.periodUnits = Timer_PERIOD_US;
    params.timerMode = Timer_CONTINUOUS_CALLBACK;
    params.timerCallback = timerCallback;

    // Open the timer instance
    timer0 = Timer_open(CONFIG_TIMER_0, &params);
    if (timer0 == NULL) {
        // Failed to initiate timer, handle the error
        while (1) {}
    }

    // Start the timer
    if (Timer_start(timer0) == Timer_STATUS_ERROR) {
        // Failed to start timer, handle error
        while (1) {}
    }
}

