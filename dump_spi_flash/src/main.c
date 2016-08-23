//
// This file is part of the GNU ARM Eclipse distribution.
// Copyright (c) 2014 Liviu Ionescu.
//

// ----------------------------------------------------------------------------

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "diag/Trace.h"

#include "Timer.h"
#include "SpiFlash.h"

// ----------------------------------------------------------------------------
//
// Semihosting STM32F1 led blink sample (trace via DEBUG).
//
// In debug configurations, demonstrate how to print a greeting message
// on the trace device. In release configurations the message is
// simply discarded.
//
// To demonstrate semihosting, display a message on the standard output
// and another message on the standard error.
//
// Then demonstrates how to blink a led with 1 Hz, using a
// continuous loop and SysTick delays.
//
// On DEBUG, the uptime in seconds is also displayed on the trace device.
//
// Trace support is enabled by adding the TRACE macro definition.
// By default the trace messages are forwarded to the DEBUG output,
// but can be rerouted to any device or completely suppressed, by
// changing the definitions required in system/src/diag/trace_impl.c
// (currently OS_USE_TRACE_SEMIHOSTING_DEBUG/_STDOUT).
//
// The external clock frequency is specified as a preprocessor definition
// passed to the compiler via a command line option (see the 'C/C++ General' ->
// 'Paths and Symbols' -> the 'Symbols' tab, if you want to change it).
// The value selected during project creation was HSE_VALUE=8000000.
//
// Note: The default clock settings take the user defined HSE_VALUE and try
// to reach the maximum possible system clock. For the default 8 MHz input
// the result is guaranteed, but for other values it might not be possible,
// so please adjust the PLL settings in system/src/cmsis/system_stm32f10x.c
//

// ----- Timing definitions -------------------------------------------------

// Keep the LED on for 2/3 of a second.
#define BLINK_ON_TICKS  (TIMER_FREQUENCY_HZ * 1 / 4)
#define BLINK_OFF_TICKS (TIMER_FREQUENCY_HZ - BLINK_ON_TICKS)

// ----- main() ---------------------------------------------------------------

// Sample pragmas to cope with warnings. Please note the related line at
// the end of this function, used to pop the compiler diagnostics status.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#pragma GCC diagnostic ignored "-Wreturn-type"

int
main(int argc, char* argv[])
{
	// Show the program parameters (passed via semihosting).
	// Output is via the semihosting output channel.
	trace_dump_args(argc, argv);

	// Send a greeting to the trace device (skipped on Release).
	trace_puts("Hello ARM World!");

	// At this stage the system clock should have already been configured
	// at high speed.
	trace_printf("System clock: %u Hz\n", SystemCoreClock);

	timer_start();

	spi_flash_init();
	spi_flash_read_id();

	puts("(d)ump, (p)rogram + verify or (v)erify?\r\n");
	char opt = getchar();
	while (getchar() != '\n');

	switch (opt) {
	case 'd':
		spi_flash_dump();
		break;
	case 'p':
		spi_flash_upload();
		spi_flash_verify();
		break;
	case 'v':
		spi_flash_verify();
		break;
	}

	puts("Ok bye\r\n");


	// Blink
	for (;;)
	{
		spi_flash_led(true);
		timer_sleep(BLINK_ON_TICKS);

		spi_flash_led(false);
		timer_sleep(BLINK_OFF_TICKS);
	}
	return 0;
}

#pragma GCC diagnostic pop

// ----------------------------------------------------------------------------
