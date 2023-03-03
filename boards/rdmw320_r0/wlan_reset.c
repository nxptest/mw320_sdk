/*
 * Copyright 2021 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <fsl_power.h>
#include <wm_os.h>
/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*******************************************************************************
 * Variables
 ******************************************************************************/
/*******************************************************************************
 * Code
 ******************************************************************************/

static void Setup_RC32M()
{
    POWER_PowerOnWlan();
    CLOCK_EnableRefClk(kCLOCK_RefClk_SYS);

    /* Configure the SFLL */
    clock_sfll_config_t sfllCfgSet;

    /* 38.4M Main Crystal -> 192M Fvco */
    sfllCfgSet.sfllSrc = kCLOCK_SFllSrcMainXtal;
    sfllCfgSet.refDiv  = 0x60;
    sfllCfgSet.fbDiv   = 0xF0;
    sfllCfgSet.kvco    = 1;
    sfllCfgSet.postDiv = 0x00;

    CLOCK_InitSFll(&sfllCfgSet);

    while (((PMU->CLK_RDY & PMU_CLK_RDY_PLL_CLK_RDY_MASK) >> PMU_CLK_RDY_PLL_CLK_RDY_SHIFT) == 0)
        ;

    /* Set clock divider for IPs */
    CLOCK_SetClkDiv(kCLOCK_DivApb0, 2U);
    CLOCK_SetClkDiv(kCLOCK_DivApb1, 2U);
    CLOCK_SetClkDiv(kCLOCK_DivPmu, 4U);

    /* Switch system clock source to SFLL
     * before RC32M calibration */
    CLOCK_SetSysClkSource(kCLOCK_SysClkSrcSFll);

    /* Enable RC32M_GATE functional clock
     * for calibration use
     */
    PMU->PERI3_CTRL &= ~PMU_PERI3_CTRL_RC32M_GATE(1);

    CLOCK_CalibrateRC32M(true, 0U);

    while ((!((RC32->STATUS & RC32_STATUS_CAL_DONE_MASK) >> RC32_STATUS_CAL_DONE_SHIFT)) ||
           (!((RC32->STATUS & RC32_STATUS_CLK_RDY_MASK) >> RC32_STATUS_CLK_RDY_SHIFT)))
        ;

    /* Disable RC32M_GATE functional clock
     * on calibrating RC32M
     */
    PMU->PERI3_CTRL |= PMU_PERI3_CTRL_RC32M_GATE(1);

    /* Reset the PMU clock divider to 1 */
    CLOCK_SetClkDiv(kCLOCK_DivPmu, 1U);
}

/*
 * PLL Configuration Routine
 *
 * Fout=Fvco/P=Refclk/M*2*N /P
 * where Fout is the output frequency of CLKOUT, Fvco is the frequency of the
 * VCO, M is reference divider ratio, N is feedback divider ratio, P is post
 * divider ratio.
 * Given the CLKOUT should be programmed to Fout, it should follow these
 * steps in sequence:
 * A) Select proper M to get Refclk/M = 400K (+/-20%)
 * B) Find proper P to make P*Fout in the range of 150MHz ~ 300MHz
 * C) Find out the N by Round(P*Fout/(Refclk/M*2))
 */
static void CLK_Config_Pll(int ref_clk, clock_sfll_src_t type)
{
    int refDiv;
    int fout = DEFAULT_SYSTEM_CLOCK;
    clock_sfll_config_t sfllConfigSet;

    while (((PMU->CLK_RDY & PMU_CLK_RDY_PLL_CLK_RDY_MASK) >> PMU_CLK_RDY_PLL_CLK_RDY_SHIFT) == 1)
        ;

    sfllConfigSet.sfllSrc = type;
    ;

    refDiv = (int)(ref_clk / 400000);

    /* Configure the SFLL */
    sfllConfigSet.refDiv = refDiv;
    sfllConfigSet.fbDiv  = (int)((double) fout / (((double)ref_clk / (double)refDiv) * 2));

    sfllConfigSet.kvco = 1;

    /* Post divider ratio, 2-bit
     * 2'b00, Fout = Fvco/1
     * 2'b01, Fout = Fvco/2
     * 2'b10, Fout = Fvco/4
     * 2'b11, Fout = Fvco/8
     */
    sfllConfigSet.postDiv = 0x0U;
    CLOCK_InitSFll(&sfllConfigSet);
    while (((PMU->CLK_RDY & PMU_CLK_RDY_PLL_CLK_RDY_MASK) >> PMU_CLK_RDY_PLL_CLK_RDY_SHIFT) == 0)
        ;
}

void CLK_Init(clock_sys_clk_src_t input_ref_clk_src)
{
    int freq                        = 0;
    clock_sfll_src_t type           = kCLOCK_SFllSrcRC32M;
    clock_sys_clk_src_t sys_clk_src = input_ref_clk_src;

    if (sys_clk_src == kCLOCK_SysClkSrcRC32M_1)
    {
        /* SFLL(driven by RC32M) will be used as
         * the system clock source */
        freq = CLK_RC32M_CLK;
        type = kCLOCK_SFllSrcRC32M;
    }
    else if (sys_clk_src == kCLOCK_SysClkSrcMainXtal)
    {
        /* XTAL/SFLL(driven by XTAL) will be used as
         * the system clock source */
        /* 38.4 MHz XTAL is routed through WLAN */
        freq = CLK_MAINXTAL_CLK;
        type = kCLOCK_SFllSrcMainXtal;
        CLOCK_EnableRefClk(kCLOCK_RefClk_SYS);
    }

    Setup_RC32M();

    /* On RC32M setup, SystemClkSrc = SFLL
     * Change the clock to reference clock before configuring PLL */
    CLOCK_SetSysClkSource(input_ref_clk_src);

    /* If board_cpu_frequency > board_main_xtal, SFLL would be
     * used as system clock source.
     * SFLL should be disabled otherwise.
     * Also, SFLL should be disabled before reconfiguring
     * SFLL to a new frequency value */
    CLOCK_DeinitSFll();

    /*
     * Check if expected cpu frequency is greater than the
     * source clock frequency. In that case we need to enable
     * the PLL.
     */
    if (DEFAULT_SYSTEM_CLOCK > freq)
    {
        CLK_Config_Pll(freq, type);
        freq = DEFAULT_SYSTEM_CLOCK;
        ;
    }

    if (freq > 50000000)
    {
        /* Max APB0 freq 50MHz */
        CLOCK_SetClkDiv(kCLOCK_DivApb0, 2U);
        /* Max APB1 freq 50MHz */
        CLOCK_SetClkDiv(kCLOCK_DivApb1, 2U);
    }

    CLOCK_SetSysClkSource(kCLOCK_SysClkSrcSFll);
}

static void wlan_power_down_card()
{
    /*
     * Disable interrupts to make
     * CLK switching  atomic
     */
    os_disable_all_interrupts();
    __disable_irq();
    /*
     * RC32M is now used as  SFLL ref clock
     * Default ref clock is  38.4 MHZ from WLAN
     * When WLAN enters PDN
     * 38.4 MHz is not available
     * Configure SFLL to 200 MHz using RC32M
     */
    CLK_Init(kCLOCK_SysClkSrcRC32M_1);

    /*
     * Power Down WLAN
     */
    POWER_PowerOffWlan();

    /*
     * Re-enable interrupts
     */
    os_enable_all_interrupts();
    __enable_irq();
}

static void wlan_power_up_card()
{
    /*
     * Disable interrupts to make
     * CLK switching  atomic
     */
    os_disable_all_interrupts();
    __disable_irq();
    /*
     * Once WLAN is powered up, the
     * default ref clock of 38.4 MHz is
     * available.
     * Configure SFLL to 200 MHz using MAINXTAL
     */
    /*
     * Power On WLAN
     */
    POWER_PowerOnWlan();

    CLK_Init(kCLOCK_SysClkSrcMainXtal);

    /*
     * Re-enable interrupts
     */
    os_enable_all_interrupts();
    __enable_irq();
}

void BOARD_WLANPowerReset()
{
    /*
     * Power down WLAN
     */
    wlan_power_down_card();
    os_thread_sleep(os_msec_to_ticks(2000));

    /*
     * Power up WLAN
     */
    wlan_power_up_card();
    os_thread_sleep(os_msec_to_ticks(2000));
}
