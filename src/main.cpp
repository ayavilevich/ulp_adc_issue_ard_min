#include <Arduino.h> // comment out this line for an esp-idf compatible code

#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "driver/adc.h"
#include "esp32/ulp.h"
#include "esp_chip_info.h"
#include "esp_idf_version.h"
#if ESP_IDF_VERSION_MAJOR == 5
#include "soc\rtc_cntl_reg.h"	 // IDF 5 needs this for RTC_CNTL_LOW_POWER_ST_REG, etc
#endif

void setup()
{
#ifdef ESP_ARDUINO_VERSION_MAJOR
	Serial.begin(115200);
	printf("ESP-IDF: %d.%d.%d, ESP Arduino: %d.%d.%d\n", ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH, ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
#else
	printf("ESP-IDF: %d.%d.%d\n", ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH);
#endif
	printf("Start\n");

	// this was tested on ESP32 rev 3
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);
#if ESP_IDF_VERSION_MAJOR == 5
	printf("Chip revision: %d\n", chip_info.revision);
#else
	printf("Chip revision: %d, full: %d\n", chip_info.revision, chip_info.full_revision);
#endif

	esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
	if (cause == ESP_SLEEP_WAKEUP_ULP)
	{
		printf("ULP wakeup\n");
	}
	if (cause == ESP_SLEEP_WAKEUP_TIMER)
	{
		printf("Timer wakeup\n");
	}

	if (cause != ESP_SLEEP_WAKEUP_ULP && cause != ESP_SLEEP_WAKEUP_TIMER)
	{
		printf("Not ULP/timer wakeup\n");

		// The ULP program with its consts

#define U_CONST_ADC_CHANNEL 6

#define U_MEM_START_COUNTER 0 // use as index to RTC_SLOW_MEM
#define U_MEM_WAKEUP_COUNTER 1
#define U_MEM_WAKEUP_COUNTER_GOAL 2
#define U_MEM_DO_ADC 3
#define U_MEM_ENTRY 4 // program start address

#define U_LABEL_WAKE_UP 0 // use with M_ macros
#define U_LABEL_EXIT 1

		const ulp_insn_t program[] = {
			// Increment start_counter
			I_MOVI(R3, U_MEM_START_COUNTER),
			I_LD(R2, R3, 0),
			I_ADDI(R2, R2, 1),
			I_ST(R2, R3, 0),
			// Load wakeup_counter and wakeup_counter_goal
			I_MOVI(R3, U_MEM_WAKEUP_COUNTER),	   // R3 = wakeup counter address
			I_LD(R2, R3, 0),					   // R2 = wakeup counter value
			I_MOVI(R1, U_MEM_WAKEUP_COUNTER_GOAL), // R1 = wakeup counter goal address
			I_LD(R0, R1, 0),					   // R0 = wakeup counter goal value
			I_SUBR(R1, R2, R0),					   // R1 = R2 - R0 = wakeup counter value - wakeup counter goal value
			M_BXZ(U_LABEL_WAKE_UP),				   // If equal (last ALU = 0) (counter reached goal), jump to wake_up
			// inc wakeup counter
			I_ADDI(R2, R2, 1),
			I_ST(R2, R3, 0), // Store incremented wakeup_counter
			// If do_adc == 0, exit
			I_MOVI(R3, U_MEM_DO_ADC), // R3 = do_adc address
			I_LD(R0, R3, 0),		  // R0 = do_adc value
			M_BL(U_LABEL_EXIT, 1),	  // exit if do_adc<1
			// ADC read (channel index is ADC_CHANNEL + 1)
			I_ADC(R1, 0, U_CONST_ADC_CHANNEL), // R1 = adc, do I need to add +1 ??? see https://github.com/espressif/arduino-esp32/issues/10904
			// exit:
			M_LABEL(U_LABEL_EXIT),
			I_HALT(),
			// wake_up:
			M_LABEL(U_LABEL_WAKE_UP),
			// Read RTC_CNTL_RDY_FOR_WAKEUP flag
			I_RD_REG(RTC_CNTL_LOW_POWER_ST_REG, RTC_CNTL_RDY_FOR_WAKEUP_S, RTC_CNTL_RDY_FOR_WAKEUP_S), // READ_RTC_FIELD(RTC_CNTL_LOW_POWER_ST_REG, RTC_CNTL_RDY_FOR_WAKEUP)
			I_ANDI(R0, R0, 1),																		   // R0 mask first bit
			M_BXZ(U_LABEL_EXIT),																	   // If last ALU = 0, jump to exit
			// Wake up main CPU
			I_WAKE(),
			// Disable ULP wakeup timer
			I_END(),			// WRITE_RTC_FIELD(RTC_CNTL_STATE0_REG, RTC_CNTL_ULP_CP_SLP_TIMER_EN, 0)
			M_BX(U_LABEL_EXIT), // goto exit
		};

		// load program
		size_t size = sizeof(program) / sizeof(ulp_insn_t);
		esp_err_t err = ulp_process_macros_and_load(U_MEM_ENTRY, program, &size);
		ESP_ERROR_CHECK(err);

		// configure ADC for ULP
		adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);
		adc1_config_width(ADC_WIDTH_BIT_12);
		adc1_ulp_enable();

		// init vars
		RTC_SLOW_MEM[U_MEM_START_COUNTER] = 0;

		// Set ULP wake up period to 100ms
		ulp_set_wakeup_period(0, 100 * 1000);
	}
	else
	{
		printf("Deep sleep wakeup\n");
		printf("ULP do adc %d\n", RTC_SLOW_MEM[U_MEM_DO_ADC] & UINT16_MAX);
		printf("ULP start counter: %d\n", RTC_SLOW_MEM[U_MEM_START_COUNTER] & UINT16_MAX);
		printf("ULP wake counter: %d\n", RTC_SLOW_MEM[U_MEM_WAKEUP_COUNTER] & UINT16_MAX);
	}

	RTC_SLOW_MEM[U_MEM_WAKEUP_COUNTER] = 0;
	RTC_SLOW_MEM[U_MEM_WAKEUP_COUNTER_GOAL] = 50;
	// RTC_SLOW_MEM[U_MEM_DO_ADC] = 0; // don't perform ADC
	RTC_SLOW_MEM[U_MEM_DO_ADC] = 1; // perform ADC, will get stuck on new SDK

	// Start the program
	esp_err_t err = ulp_run(U_MEM_ENTRY);
	ESP_ERROR_CHECK(err);

	// optional "timeout" timer. doesn't seem to affect the outcode but prevents the sleep from being forever and allows to see counters when ULP doesn't wake the device.
	printf("TEST: sleeping for 20 sec\n");
	esp_sleep_enable_timer_wakeup((uint64_t)20 * 1000000L); // wake after specified time

	printf("Entering deep sleep\n");
	ESP_ERROR_CHECK(esp_sleep_enable_ulp_wakeup());

	esp_deep_sleep_start();
}

void loop()
{
}

#ifndef ESP_ARDUINO_VERSION_MAJOR
void app_main(void)
{
	setup();
}
#endif
