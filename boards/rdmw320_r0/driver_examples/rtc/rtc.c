/*
 * Copyright 2020-2021 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "fsl_rtc.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define APP_RTC_DIV_SEL         (kRTC_ClockDiv32)

/* RTC HZ is power of 2. RTC tick and seconds can be converted by shifting. */
#define APP_RTC_HZ_SHIFT        ((uint8_t)kRTC_ClockDiv32768 - (uint8_t)APP_RTC_DIV_SEL)

#define SECONDS_IN_A_DAY    (86400U)
#define SECONDS_IN_A_HOUR   (3600U)
#define SECONDS_IN_A_MINUTE (60U)
#define DAYS_IN_A_YEAR      (365U)
#define YEAR_RANGE_START    (1970U)
#define YEAR_RANGE_END      (2099U)

typedef struct _rtc_datetime
{
    uint16_t year;  /*!< Range from 1970 to 2099.*/
    uint8_t month;  /*!< Range from 1 to 12.*/
    uint8_t day;    /*!< Range from 1 to 31 (depending on month).*/
    uint8_t hour;   /*!< Range from 0 to 23.*/
    uint8_t minute; /*!< Range from 0 to 59.*/
    uint8_t second; /*!< Range from 0 to 59.*/
} rtc_datetime_t;

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
volatile bool busyWait;
volatile uint32_t gSeconds;

/*******************************************************************************
 * Code
 ******************************************************************************/

static bool APP_CheckDatetimeFormat(const rtc_datetime_t *datetime)
{
    assert(NULL != datetime);

    /* Table of days in a month for a non leap year. First entry in the table is not used,
     * valid months start from 1
     */
    uint8_t daysPerMonth[] = {0U, 31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};

    /* Check year, month, hour, minute, seconds */
    if ((datetime->year < YEAR_RANGE_START) || (datetime->year > YEAR_RANGE_END) || (datetime->month > 12U) ||
        (datetime->month < 1U) || (datetime->hour >= 24U) || (datetime->minute >= 60U) || (datetime->second >= 60U))
    {
        /* If not correct then error*/
        return false;
    }

    /* Adjust the days in February for a leap year */
    if ((((datetime->year & 3U) == 0U) && (datetime->year % 100U != 0U)) || (datetime->year % 400U == 0U))
    {
        daysPerMonth[2] = 29U;
    }

    /* Check the validity of the day */
    if ((datetime->day > daysPerMonth[datetime->month]) || (datetime->day < 1U))
    {
        return false;
    }

    return true;
}

static uint32_t APP_ConvertDatetimeToSeconds(const rtc_datetime_t *datetime)
{
    assert(NULL != datetime);

    /* Number of days from begin of the non Leap-year*/
    /* Number of days from begin of the non Leap-year*/
    uint16_t monthDays[] = {0U, 0U, 31U, 59U, 90U, 120U, 151U, 181U, 212U, 243U, 273U, 304U, 334U};
    uint32_t seconds;

    /* Compute number of days from 1970 till given year*/
    seconds = ((uint32_t)datetime->year - 1970U) * DAYS_IN_A_YEAR;
    /* Add leap year days */
    seconds += (((uint32_t)datetime->year / 4U) - (1970U / 4U));
    /* Add number of days till given month*/
    seconds += monthDays[datetime->month];
    /* Add days in given month. We subtract the current day as it is
     * represented in the hours, minutes and seconds field*/
    seconds += ((uint32_t)datetime->day - 1U);
    /* For leap year if month less than or equal to Febraury, decrement day counter*/
    if ((0U == (datetime->year & 3U)) && (datetime->month <= 2U))
    {
        seconds--;
    }

    seconds = (seconds * SECONDS_IN_A_DAY) + ((uint32_t)datetime->hour * SECONDS_IN_A_HOUR) +
              ((uint32_t)datetime->minute * SECONDS_IN_A_MINUTE) + datetime->second;

    return seconds;
}

static void APP_ConvertSecondsToDatetime(uint32_t seconds, rtc_datetime_t *datetime)
{
    assert(NULL != datetime);

    uint32_t x;
    uint32_t secondsRemaining, days;
    uint16_t daysInYear;
    /* Table of days in a month for a non leap year. First entry in the table is not used,
     * valid months start from 1
     */
    uint8_t daysPerMonth[] = {0U, 31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};

    /* Start with the seconds value that is passed in to be converted to date time format */
    secondsRemaining = seconds;

    /* Calcuate the number of days, we add 1 for the current day which is represented in the
     * hours and seconds field
     */
    days = secondsRemaining / SECONDS_IN_A_DAY + 1U;

    /* Update seconds left*/
    secondsRemaining = secondsRemaining % SECONDS_IN_A_DAY;

    /* Calculate the datetime hour, minute and second fields */
    datetime->hour   = (uint8_t)(secondsRemaining / SECONDS_IN_A_HOUR);
    secondsRemaining = secondsRemaining % SECONDS_IN_A_HOUR;
    datetime->minute = (uint8_t)(secondsRemaining / 60U);
    datetime->second = (uint8_t)(secondsRemaining % SECONDS_IN_A_MINUTE);

    /* Calculate year */
    daysInYear     = DAYS_IN_A_YEAR;
    datetime->year = YEAR_RANGE_START;
    while (days > daysInYear)
    {
        /* Decrease day count by a year and increment year by 1 */
        days -= daysInYear;
        datetime->year++;

        /* Adjust the number of days for a leap year */
        if (0U != (datetime->year & 3U))
        {
            daysInYear = DAYS_IN_A_YEAR;
        }
        else
        {
            daysInYear = DAYS_IN_A_YEAR + 1U;
        }
    }

    /* Adjust the days in February for a leap year */
    if (0U == (datetime->year & 3U))
    {
        daysPerMonth[2] = 29U;
    }

    for (x = 1U; x <= 12U; x++)
    {
        if (days <= daysPerMonth[x])
        {
            datetime->month = (uint8_t)x;
            break;
        }
        else
        {
            days -= daysPerMonth[x];
        }
    }

    datetime->day = (uint8_t)days;
}

static void APP_SetDatetime(const rtc_datetime_t *datetime)
{
    bool ret;

    assert(NULL != datetime);

    /* Return error if the time provided is not valid */
    ret = APP_CheckDatetimeFormat(datetime);
    if (!ret)
    {
        assert(false);
    }

    /* Set time in seconds */
    gSeconds = APP_ConvertDatetimeToSeconds(datetime);
    RTC_ResetTimer(RTC);
}

uint32_t APP_GetDatetime(rtc_datetime_t *datetime)
{
    uint32_t sec;
    uint32_t cnt0, cnt;

    assert(NULL != datetime);

    cnt0 = RTC_GetCounter(RTC);
    sec = gSeconds;
    cnt = RTC_GetCounter(RTC);

    if (cnt0 > cnt) /* Overflow */
    {
        sec = gSeconds; /* Get gSeconds again */
    }
    sec += cnt >> APP_RTC_HZ_SHIFT;

    APP_ConvertSecondsToDatetime(sec, datetime);

    return sec;
}

/*!
 * @brief ISR for Alarm interrupt
 *
 * This function changes the state of busyWait.
 */
void RTC_IRQHandler(void)
{
    uint32_t status = RTC_GetStatusFlags(RTC);

    if ((status & (uint32_t)kRTC_TimeOverflowFlag) != 0U)
    {
        /* Safe in case APP_RTC_HZ_SHIFT = 0 because in such case overflow will never occur. */
        gSeconds += (1UL << (32U - APP_RTC_HZ_SHIFT));
    }

    if ((status & (uint32_t)kRTC_AlarmFlag) != 0U)
    {
        busyWait = false;
    }

    /* Clear all flags */
    RTC_ClearStatusFlags(RTC, kRTC_AllClearableFlags);

    SDK_ISR_EXIT_BARRIER;
}

/*!
 * @brief Main function
 */
int main(void)
{
    uint32_t sec;
    uint32_t currSeconds;
    uint32_t cnt;
    uint8_t index;
    rtc_datetime_t date;
    rtc_config_t rtcConfig;

    /* Board pin, clock, debug console init */
    BOARD_InitPins();
    BOARD_BootClockRUN();
    BOARD_InitDebugConsole();

    CLOCK_EnableXtal32K(kCLOCK_Osc32k_External);
    CLOCK_AttachClk(kXTAL32K_to_RTC);
    /* Init RTC */
    /*
     *    config->ignoreInRunning = false;
     *    config->autoUpdateCntVal = true;
     *    config->stopCntInDebug = true;
     *    config->clkDiv = kRTC_ClockDiv32;
     *    config->cntUppVal = 0xFFFFFFFFU;
     */
    RTC_GetDefaultConfig(&rtcConfig);
    rtcConfig.clkDiv = APP_RTC_DIV_SEL;
    RTC_Init(RTC, &rtcConfig);

    PRINTF("RTC example: set up time to wake up an alarm\r\n");

    /* Set a start date time and start RTC */
    date.year   = 2021U;
    date.month  = 3U;
    date.day    = 10U;
    date.hour   = 19U;
    date.minute = 5U;
    date.second = 0U;

    /* Set RTC time to default */
    APP_SetDatetime(&date);

    /* Enable RTC overflow and alarm interrupt */
    RTC_EnableInterrupts(RTC, (uint32_t)kRTC_AllInterruptsEnable);

    /* Enable at the NVIC */
    EnableIRQ(RTC_IRQn);

    /* Start the RTC time counter */
    RTC_StartTimer(RTC);

    /* This loop will set the RTC alarm */
    while (true)
    {
        busyWait = true;
        index    = 0;
        sec      = 0;

        /* Get alarm time from user */
        PRINTF("Please input the number of seconds to wait for alarm \r\n");
        /* Alarm cannot exceed overflow time. Otherwise the alarm will be triggered after (alarm % overflow) seconds.
         * Here we set the limitation to < 1/2 overflow time to ease the validation. */
        PRINTF("The second must be positive integer value and less than %u\r\n", (1UL << (31U - APP_RTC_HZ_SHIFT)));

        while (index != 0x0D)
        {
            index = GETCHAR();
            if ((index >= '0') && (index <= '9'))
            {
                PUTCHAR(index);
                sec = sec * 10U + (index - 0x30U);
            }
        }
        PRINTF("\r\n");
        assert(sec < (1UL << (31U - APP_RTC_HZ_SHIFT)));

        /* Get date time */
        currSeconds = APP_GetDatetime(&date);
        /* alarm counter overflow takes low 32-bit. */
        cnt = RTC_GetCounter(RTC) + (sec << APP_RTC_HZ_SHIFT);

        /* Set alarm counter */
        RTC_SetAlarm(RTC, cnt);

        /* print default time */
        PRINTF("Current datetime: %04hd-%02hd-%02hd %02hd:%02hd:%02hd\r\n", date.year, date.month, date.day,
               date.hour, date.minute, date.second);

        /* Get alarm time */
        APP_ConvertSecondsToDatetime(currSeconds + sec, &date);

        /* Print alarm time */
        PRINTF("Alarm will occur at: %04hd-%02hd-%02hd %02hd:%02hd:%02hd\r\n", date.year, date.month, date.day,
               date.hour, date.minute, date.second);

        /* Wait until alarm occurs */
        while (busyWait)
        {
        }

        PRINTF("\r\n Alarm occurs !!!!\r\n");
    }
}
