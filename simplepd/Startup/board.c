/*
 * Copyright (c) 2016-2017, Texas Instruments Incorporated
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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <ti/devices/cc26x0r2/inc/hw_ints.h>
#include <ti/devices/cc26x0r2/inc/hw_memmap.h>

/*
 *  =============================== ADC ===============================
 *  Keep one internal VDDS channel for future voltage measurement.
 */
#include <ti/drivers/ADC.h>
#include <ti/drivers/adc/ADCCC26XX.h>

ADCCC26XX_Object adcCC26xxObjects[3];

const ADCCC26XX_HWAttrs adcCC26xxHWAttrs[3] = {
    {
        .adcDIO              = PIN_UNASSIGNED,
        .adcCompBInput       = ADC_COMPB_IN_DCOUPL,
        .refSource           = ADCCC26XX_FIXED_REFERENCE,
        .samplingDuration    = ADCCC26XX_SAMPLING_DURATION_2P7_US,
        .inputScalingEnabled = true,
        .triggerSource       = ADCCC26XX_TRIGGER_MANUAL,
        .returnAdjustedVal   = false
    },
    {
        .adcDIO              = PIN_UNASSIGNED,
        .adcCompBInput       = ADC_COMPB_IN_VSS,
        .refSource           = ADCCC26XX_FIXED_REFERENCE,
        .samplingDuration    = ADCCC26XX_SAMPLING_DURATION_2P7_US,
        .inputScalingEnabled = true,
        .triggerSource       = ADCCC26XX_TRIGGER_MANUAL,
        .returnAdjustedVal   = false
    },
    {
        .adcDIO              = PIN_UNASSIGNED,
        .adcCompBInput       = ADC_COMPB_IN_VDDS,
        .refSource           = ADCCC26XX_FIXED_REFERENCE,
        .samplingDuration    = ADCCC26XX_SAMPLING_DURATION_2P7_US,
        .inputScalingEnabled = true,
        .triggerSource       = ADCCC26XX_TRIGGER_MANUAL,
        .returnAdjustedVal   = false
    }
};

const ADC_Config ADC_config[3] = {
    {&ADCCC26XX_fxnTable, &adcCC26xxObjects[0], &adcCC26xxHWAttrs[0]},
    {&ADCCC26XX_fxnTable, &adcCC26xxObjects[1], &adcCC26xxHWAttrs[1]},
    {&ADCCC26XX_fxnTable, &adcCC26xxObjects[2], &adcCC26xxHWAttrs[2]},
};

const uint_least8_t ADC_count = 3;

/*
 *  =============================== Crypto ===============================
 *  BLE stack security and random address support may pull in these drivers.
 */
#include <ti/drivers/crypto/CryptoCC26XX.h>

CryptoCC26XX_Object cryptoCC26XXObjects[1];

const CryptoCC26XX_HWAttrs cryptoCC26XXHWAttrs[1] = {
    {
        .baseAddr    = CRYPTO_BASE,
        .powerMngrId = PowerCC26XX_PERIPH_CRYPTO,
        .intNum      = INT_CRYPTO_RESULT_AVAIL_IRQ,
        .intPriority = ~0,
    }
};

const CryptoCC26XX_Config CryptoCC26XX_config[1] = {
    {
        .object  = &cryptoCC26XXObjects[0],
        .hwAttrs = &cryptoCC26XXHWAttrs[0]
    },
};

/*
 *  =============================== AESCCM ===============================
 */
#include <ti/drivers/AESCCM.h>
#include <ti/drivers/aesccm/AESCCMCC26XX.h>

AESCCMCC26XX_Object aesccmCC26XXObjects[1];

const AESCCMCC26XX_HWAttrs aesccmCC26XXHWAttrs[1] = {
    {
        .intPriority = ~0,
    }
};

const AESCCM_Config AESCCM_config[1] = {
    {
        .object  = &aesccmCC26XXObjects[0],
        .hwAttrs = &aesccmCC26XXHWAttrs[0]
    },
};

const uint_least8_t AESCCM_count = 1;

/*
 *  =============================== AESCTR ===============================
 */
#include <ti/drivers/AESCTR.h>
#include <ti/drivers/aesctr/AESCTRCC26XX.h>

AESCTRCC26XX_Object aesctrCC26XXObjects[1];

const AESCTRCC26XX_HWAttrs aesctrCC26XXHWAttrs[1] = {
    {
        .intPriority = ~0,
    }
};

const AESCTR_Config AESCTR_config[1] = {
    {
        .object  = &aesctrCC26XXObjects[0],
        .hwAttrs = &aesctrCC26XXHWAttrs[0]
    },
};

const uint_least8_t AESCTR_count = 1;

/*
 *  =============================== AESCTRDRBG ===============================
 */
#include <ti/drivers/AESCTRDRBG.h>
#include <ti/drivers/aesctrdrbg/AESCTRDRBGXX.h>

AESCTRDRBGXX_Object aesctrdrbgXXObjects[1];

const AESCTRDRBGXX_HWAttrs aesctrdrbgXXHWAttrs[1] = {
    {
        .aesctrIndex = 0,
    }
};

const AESCTRDRBG_Config AESCTRDRBG_config[1] = {
    {
        .object  = &aesctrdrbgXXObjects[0],
        .hwAttrs = &aesctrdrbgXXHWAttrs[0]
    },
};

const uint_least8_t AESCTRDRBG_count = 1;

/*
 *  =============================== NVS ===============================
 */
#include <ti/drivers/NVS.h>
#include <ti/drivers/nvs/NVSCC26XX.h>

#define NVS_REGIONS_BASE 0x1A000
#define SECTORSIZE       0x1000
#define REGIONSIZE       (SECTORSIZE * 4)

#if defined(__TI_COMPILER_VERSION__)
#pragma LOCATION(flashBuf, NVS_REGIONS_BASE);
#pragma NOINIT(flashBuf);
static char flashBuf[REGIONSIZE];
#elif defined(__IAR_SYSTEMS_ICC__)
static __no_init char flashBuf[REGIONSIZE] @ NVS_REGIONS_BASE;
#elif defined(__GNUC__)
__attribute__ ((section (".nvs")))
static char flashBuf[REGIONSIZE];
#endif

NVSCC26XX_Object nvsCC26xxObjects[1];

const NVSCC26XX_HWAttrs nvsCC26xxHWAttrs[1] = {
    {
        .regionBase = (void *)flashBuf,
        .regionSize = REGIONSIZE,
    },
};

const NVS_Config NVS_config[1] = {
    {
        .fxnTablePtr = &NVSCC26XX_fxnTable,
        .object = &nvsCC26xxObjects[0],
        .hwAttrs = &nvsCC26xxHWAttrs[0],
    },
};

const uint_least8_t NVS_count = 1;

/*
 *  =============================== PIN ===============================
 */
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>

const PIN_Config BoardGpioInitTable[] = {
    PIN_TERMINATE
};

const PINCC26XX_HWAttrs PINCC26XX_hwAttrs = {
    .intPriority = ~0,
    .swiPriority = 0
};

/*
 *  =============================== Power ===============================
 */
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>

const PowerCC26XX_Config PowerCC26XX_config = {
    .policyInitFxn      = NULL,
    .policyFxn          = &PowerCC26XX_standbyPolicy,
    .calibrateFxn       = &PowerCC26XX_calibrate,
    .enablePolicy       = true,
    .calibrateRCOSC_LF  = false,
    .calibrateRCOSC_HF  = true,
};

/*
 *  =============================== RF Driver ===============================
 */
#include <ti/drivers/rf/RF.h>

const RFCC26XX_HWAttrsV2 RFCC26XX_hwAttrs = {
    .hwiPriority        = ~0,
    .swiPriority        = 0,
    .xoscHfAlwaysNeeded = true,
    .globalCallback     = NULL,
    .globalEventMask    = 0
};

/*
 *  =============================== UART ===============================
 */
#include <ti/drivers/UART.h>
#include <ti/drivers/uart/UARTCC26XX.h>

UARTCC26XX_Object uartCC26XXObjects[1];

uint8_t uartCC26XXRingBuffer[1][32];

const UARTCC26XX_HWAttrsV2 uartCC26XXHWAttrs[1] = {
    {
        .baseAddr       = UART0_BASE,
        .powerMngrId    = PowerCC26XX_PERIPH_UART0,
        .intNum         = INT_UART0_COMB,
        .intPriority    = ~0,
        .swiPriority    = 0,
        .txPin          = IOID_0,
        .rxPin          = IOID_1,
        .ctsPin         = PIN_UNASSIGNED,
        .rtsPin         = PIN_UNASSIGNED,
        .ringBufPtr     = uartCC26XXRingBuffer[0],
        .ringBufSize    = sizeof(uartCC26XXRingBuffer[0]),
        .txIntFifoThr   = UARTCC26XX_FIFO_THRESHOLD_1_8,
        .rxIntFifoThr   = UARTCC26XX_FIFO_THRESHOLD_4_8,
        .errorFxn       = NULL
    }
};

const UART_Config UART_config[1] = {
    {
        .fxnTablePtr = &UARTCC26XX_fxnTable,
        .object      = &uartCC26XXObjects[0],
        .hwAttrs     = &uartCC26XXHWAttrs[0]
    },
};

const uint_least8_t UART_count = 1;

/*
 *  =============================== UDMA ===============================
 */
#include <ti/drivers/dma/UDMACC26XX.h>

UDMACC26XX_Object udmaObjects[1];

const UDMACC26XX_HWAttrs udmaHWAttrs[1] = {
    {
        .baseAddr    = UDMA0_BASE,
        .powerMngrId = PowerCC26XX_PERIPH_UDMA,
        .intNum      = INT_DMA_ERR,
        .intPriority = ~0
    }
};

const UDMACC26XX_Config UDMACC26XX_config[1] = {
    {
        .object  = &udmaObjects[0],
        .hwAttrs = &udmaHWAttrs[0]
    },
};

/*
 *  =============================== TRNG ===============================
 */
#include <TRNGCC26XX.h>

TRNGCC26XX_Object trngCC26XXObjects[1];

const TRNGCC26XX_HWAttrs TRNGCC26XXHWAttrs[1] = {
    {
        .powerMngrId = PowerCC26XX_PERIPH_TRNG,
    }
};

const TRNGCC26XX_Config TRNGCC26XX_config[] = {
    {
        .object  = &trngCC26XXObjects[0],
        .hwAttrs = &TRNGCC26XXHWAttrs[0]
    },
    {NULL, NULL}
};


/*
 *  ======== Board_init ========
 */
void Board_init(void)
{
    Power_init();
	ADC_init();

    if (PIN_init(BoardGpioInitTable) != PIN_SUCCESS) {
        while (1);
    }
}
