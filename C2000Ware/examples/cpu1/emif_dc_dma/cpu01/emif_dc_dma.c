//###########################################################################
//
// FILE:   emif_dc_dma.c
//
// TITLE:  EMIF Daughtercard DMA Transfer
//
//! \addtogroup cpu01_example_list
//! <h1>EMIF Daughtercard DMA Transfer (emif_dc_dma)</h1>
//!
//!  This example runs on an EMIF Daughtercard that connects through
//!  the high density connector on F2837X evaluation boards:
//!     - TMDSCNCD28379D
//!     - LAUNCHXL-F28379D
//!     - LAUNCHXL-F28377S
//!
//!  Block data is transferred from CS0 SDRAM to CS2 ASRAM using the DMA
//!  and verified after transfer.
//!
//!  The source and destination locations can be changed using the
//!  DATA_SECTION pragmas. Variables in far memory (CS0 SDRAM) require
//!  special declaration attributes.
//!
//!  The following values must match the target evaluation board:
//!    - EMIF_NUM (emif_dc_dma.c)
//!    - EMIF_DC_F2837X_LAUNCHPAD_V1 (emif_dc.h)
//!    - _LAUNCHXL_F28377S or _LAUNCHXL_F28379D (Predefined Symbols)
//!
//
//###########################################################################
// $TI Release: F2837xD Support Library v3.04.00.00 $
// $Release Date: Sun Mar 25 13:26:04 CDT 2018 $
// $Copyright:
// Copyright (C) 2013-2018 Texas Instruments Incorporated - http://www.ti.com/
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions 
// are met:
// 
//   Redistributions of source code must retain the above copyright 
//   notice, this list of conditions and the following disclaimer.
// 
//   Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the 
//   documentation and/or other materials provided with the   
//   distribution.
// 
//   Neither the name of Texas Instruments Incorporated nor the names of
//   its contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// $
//###########################################################################

//
// Included Files
//
#include "F28x_Project.h"
#include "emif_dc.h"

//
// Constants
//
#define EMIF_NUM      EMIF_DC_F2837X_LAUNCHPAD_EMIF_NUM
#define BUFFER_WORDS  256

//
// Global Variabls
//
#if( EMIF_NUM == EMIF_DC_F2837X_LAUNCHPAD_EMIF_NUM   )
#pragma DATA_SECTION(srcBuffer, ".em1_cs0");
#pragma DATA_SECTION(dstBuffer, ".em1_cs2");
#endif
#if( EMIF_NUM == EMIF_DC_F2837X_CONTROLCARD_EMIF_NUM )
#pragma DATA_SECTION(srcBuffer, ".em2_cs0");
#pragma DATA_SECTION(dstBuffer, ".em2_cs2");
#endif
__attribute__((far)) volatile Uint16 srcBuffer[BUFFER_WORDS];
Uint16 dstBuffer[BUFFER_WORDS];

//
// Main
//
void main(void)
{
    Uint16 word;
    Uint16 errors;

    //
    // Initialize System Control:
    // PLL, WatchDog, enable Peripheral Clocks
    //
    InitSysCtrl();

    //
    // Initialize EMIF module for use with daughtercard
    //
    EMIF_DC_setupPinmux(EMIF_NUM, GPIO_MUX_CPU1);
    EMIF_DC_initModule(EMIF_NUM);
    EMIF_DC_initCS0(EMIF_NUM);
    EMIF_DC_initCS2(EMIF_NUM, EMIF_DC_ASRAM);

    //
    // Initialize DMA module for transfer
    //
    DMAInitialize();
    DMACH1AddrConfig32bit((volatile Uint32*)dstBuffer, (volatile Uint32*)srcBuffer);
    DMACH1BurstConfig(31, 2, 2); // 32 words per burst
                                 // Increment src and dst addresses by 2 words
    DMACH1TransferConfig((BUFFER_WORDS/32)-1, 2, 2);
    DMACH1WrapConfig(0xFFFF,0,0xFFFF,0);
    DMACH1ModeConfig(0x0,PERINT_ENABLE,ONESHOT_ENABLE,
                     CONT_DISABLE,SYNC_DISABLE,SYNC_SRC,
                     OVRFLOW_DISABLE,THIRTYTWO_BIT,CHINT_END,CHINT_DISABLE);
    StartDMACH1();

    //
    // Initialize data buffers
    //
    for(word=0; word<BUFFER_WORDS; word++)
    {
        srcBuffer[word] = word;
        dstBuffer[word] = 0;
    }

    //
    // Verify that data buffers have correct starting values
    // If buffers are not initialized correctly, check the following:
    //      EMIF_NUM (emif_dc_cla.c)
    //      EMIF_DC_F2837X_LAUNCHPAD_V1 (emif_dc.h)
    //      _LAUNCHXL_F28377S or _LAUNCHXL_F28379D (Predefined Symbols)
    //
    errors = 0;

    for(word=0; word<BUFFER_WORDS; word++)
    {
        if(srcBuffer[word] != word)
        {
            errors++;
            ESTOP0;
            break;
        }
        if(dstBuffer[word] != 0)
        {
            errors++;
            ESTOP0;
            break;
        }
    }

    //
    // Execute block data transfer by forcing DMA trigger
    //
    EALLOW;
    DmaRegs.CH1.CONTROL.bit.PERINTFRC = 1;
    EDIS;

    //
    // Wait for DMA transfer to begin and complete
    //
    while(DmaRegs.CH1.CONTROL.bit.TRANSFERSTS != 1)
    {
    }
    while(DmaRegs.CH1.CONTROL.bit.TRANSFERSTS != 0)
    {
    }

    //
    // Verify that block data has been transferred
    //
    for(word=0; word<BUFFER_WORDS; word++)
    {
        if(srcBuffer[word] != word)
        {
            errors++;
            ESTOP0;
            break;
        }

        if(dstBuffer[word] != word)
        {
            errors++;
            ESTOP0;
            break;
        }
    }

    if(errors == 0)
    {
        ESTOP0; // PASS
    }
    else
    {
        ESTOP0; // FAIL
    }
}

//
// End of file
//
