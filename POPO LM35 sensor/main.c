//Header Files needed to access the TIVA APIs
#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/debug.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/adc.h"  //contains definitions for the ADC driver
//Headers for UART
#include "driverlib/pin_map.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include "utils/uartstdio.c"

//! The following UART signals are configured only for displaying console
//! messages for this example.
//! - UART0 peripheral
//! - GPIO Port A peripheral (for UART0 pins)
//! - UART0RX - PA0
//! - UART0TX - PA1

// This function sets up UART0 to be used for a console to display information
// as the example is running.
//
//*****************************************************************************
void
InitConsole(void)
{
    //
    // Enable GPIO port A which is used for UART0 pins.
    // TODO: change this to whichever GPIO port you are using.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);

    //
    // Configure the pin muxing for UART0 functions on port A0 and A1.
    // This step is not necessary if your part does not support pin muxing.
    // TODO: change this to select the port/pin you are using.
    //
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);

    //
    // Enable UART0 so that we can configure the clock.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);

    //
    // Use the internal 16MHz oscillator as the UART clock source.
    //
    UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);

    //
    // Select the alternate (UART) function for these pins.
    // TODO: change this to select the port/pin you are using.
    //
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    //
    // Initialize the UART for console I/O.
    //
    UARTStdioConfig(0, 115200, 16000000);
}


int main(void)
{
    //Using Sequencer 1 of FIFO size = 4, hence creating an array to store its data
    uint32_t ui32ADC0Value[4];
    //Variable to store final (averaged temperature)
    uint32_t ui32TempValueC;

    // Set up the serial console to use for displaying messages.
    InitConsole();
    // Display the setup on the console.
    UARTprintf("TEMPERATURE: ->\n");

    //Set up the system clock to run at 40MHz
    /* # PLL runs on 400 MHz
     * -- this frequency is divided by 2 by default
     * -- additional dividing factors between 5 & 25 can be used
     * -- Here we use 5 (a total of 2+5=10)
     * -Use PLL
     * -Use main OSC as the system clock
     * -Run it at 16 MHz
     */
    SysCtlClockSet(SYSCTL_SYSDIV_5|SYSCTL_USE_PLL|SYSCTL_OSC_MAIN|SYSCTL_XTAL_16MHZ);

    //Enable the ADC0
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    // Wait for the ADC module to be ready.
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0))
    {}

    //Enable the PE3 to be an input pin
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);   //Enables the clock to PORT E
    GPIOPinTypeADC(GPIO_PORTE_BASE, GPIO_PIN_3);    //Defines the multiplexing role of the GPIO used

    /* Configuring the ADC sequencer (AND NOT TRIGGER!):-
     * ->Use ADC0 (STEP 1)
     * ->Use Sample Sequencer 1 (STEP 2)
     * ->Ask the processor to trigger the sequence (STEP 3)
     * ->Use the highest priority (STEP 4)
     * # allow the ADC12 to run at its default rate of 1Msps
     */
    ADCSequenceConfigure(ADC0_BASE, 1, ADC_TRIGGER_PROCESSOR, 0);

    //Next we need to configure all four steps in the ADC sequencer
    ADCSequenceStepConfigure(ADC0_BASE, 1, 0, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC0_BASE, 1, 1, ADC_CTL_CH0);
    ADCSequenceStepConfigure(ADC0_BASE, 1, 2, ADC_CTL_CH0);
    // Perform last step and set the interrupt flag (ADC_CTL_IE) as the last sample is done.
    // Also, tell the ADC logic that this is the last conversion on SS1 (ADC_CTL_END)
    ADCSequenceStepConfigure(ADC0_BASE, 1, 3, ADC_CTL_CH0 | ADC_CTL_IE | ADC_CTL_END);
    //Enabling the SS1 (AND NOT TRIGGER!)
    ADCSequenceEnable(ADC0_BASE, 1);

    //Clearing the ADC interrupt status flag
    ADCIntClear(ADC0_BASE, 1);

    //An infinite loop
    while(1)
    {
        //Trigger the ADC conversion
        ADCProcessorTrigger(ADC0_BASE, 1);
        //Get the current Interrupt status | false indicates that "raw" interrupt status is reqd and not "masked"
        //And wait until the status comes out to be 1 . i.e, the conversion is complete
        while(!ADCIntStatus(ADC0_BASE, 1, false))
        { }

        // Clear the ADC interrupt flag.
        ADCIntClear(ADC0_BASE, 1);
        //Copy the ADC values from the SS1 into a buffer. SS1 -> (array)ui32ADC0Value
        ADCSequenceDataGet(ADC0_BASE, 1, ui32ADC0Value);

        //As per the datasheet of LM35D, the IC provides the output, already calibrated in Celsius
        //Temp = ADC value / 10
        ui32TempValueC = (ui32ADC0Value[0] + ui32ADC0Value[1] + ui32ADC0Value[2] + ui32ADC0Value[3])/40;  //10 * 4 (for averaging)

        UARTprintf("Temperature = %3d*C \r", ui32TempValueC);
    }
}
