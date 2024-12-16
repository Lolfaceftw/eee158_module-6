#include "../platform.h"

#include <xc.h>

/*
    Initialization function based off code generated through Microchip Code Configurator. (c) Microchip Inc.
    Read/Write functions written by Allen.
 */

void SERCOM2_I2C_Clock_Init() {
    // All APB C bus peripheral clocks are enabled by default, so this isn't necessary
    // MCLK_REGS->MCLK_APBCMASK |= (1 << 3);

    // Set clock to GEN2 (4MHz)
    GCLK_REGS->GCLK_PCHCTRL[19] = 0x00000042;
    while ((GCLK_REGS->GCLK_PCHCTRL[19] & 0x00000040) == 0)
        asm("nop");
}

void SERCOM2_I2C_Abort();

void SERCOM2_I2C_Initialize(void) {
    SERCOM2_I2C_Clock_Init();

    /* Reset the module */
    SERCOM2_REGS->I2CM.SERCOM_CTRLA = SERCOM_I2CM_CTRLA_SWRST_Msk;

    /* Wait for synchronization */
    while ((SERCOM2_REGS->I2CM.SERCOM_SYNCBUSY) != 0U) {
        /* Do nothing */
    }

    /* Enable smart mode, set smart mode response to ACK */
    SERCOM2_REGS->I2CM.SERCOM_CTRLB = SERCOM_I2CM_CTRLB_SMEN_Msk;

    /* Wait for synchronization */
    while ((SERCOM2_REGS->I2CM.SERCOM_SYNCBUSY) != 0U) {
        /* Do nothing */
    }

    /* Baud rate - Master Baud Rate*/
    SERCOM2_REGS->I2CM.SERCOM_BAUD = 0xE8;

    // enable interrupts for ERROR, SB, MB (but not their NVIC interrupt lines)
    SERCOM2_REGS->I2CM.SERCOM_INTENSET |= SERCOM_I2CM_INTENSET_MB(1) | SERCOM_I2CM_INTENSET_SB(1) | SERCOM_I2CM_INTENSET_ERROR(1);

    // configure pins here
    // PA12 (SDA) and PA13 (SCL)
    PORT_SEC_REGS->GROUP[0].PORT_PMUX[6] = 0x22;
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[13] = 0x03;
    PORT_SEC_REGS->GROUP[0].PORT_PINCFG[12] = 0x03;

    /* Set Operation Mode (Master), SDA Hold time, run in stand by and i2c master enable */
    SERCOM2_REGS->I2CM.SERCOM_CTRLA = SERCOM_I2CM_CTRLA_INACTOUT(0x2) | SERCOM_I2CM_CTRLA_MODE_I2C_MASTER | SERCOM_I2CM_CTRLA_SDAHOLD_75NS | SERCOM_I2CM_CTRLA_SPEED_SM | SERCOM_I2CM_CTRLA_SCLSM(0UL) | SERCOM_I2CM_CTRLA_ENABLE_Msk;

    /* Wait for synchronization */
    while ((SERCOM2_REGS->I2CM.SERCOM_SYNCBUSY) != 0U) {
        /* Do nothing */
    }

    /* Initial Bus State: IDLE */
    SERCOM2_REGS->I2CM.SERCOM_STATUS = (uint16_t) SERCOM_I2CM_STATUS_BUSSTATE(0x01UL);

    /* Wait for synchronization */
    while ((SERCOM2_REGS->I2CM.SERCOM_SYNCBUSY) != 0U) {
        /* Do nothing */
    }
}

uint16_t SERCOM2_I2C_Write_Polled(uint8_t addr, uint8_t *buf, uint16_t len) {
    uint8_t flags = 0;
    uint16_t pos = 0;
    uint32_t temp = 0;
    
    // Step 0: Wait for the bus to become idle
    while ((SERCOM2_REGS->I2CM.SERCOM_STATUS & SERCOM_I2CM_STATUS_BUSSTATE_Msk) != SERCOM_I2CM_STATUS_BUSSTATE(0x01UL)) {
        if ((SERCOM2_REGS->I2CM.SERCOM_STATUS & SERCOM_I2CM_STATUS_BUSSTATE_Msk) == SERCOM_I2CM_STATUS_BUSSTATE(0x00UL)) {
            SERCOM2_I2C_Abort();
            return 0;
        }
    }

    // Step 1: Initialize transfer by writing to ADDR (ADDR[0] indicates direction)
    SERCOM2_REGS->I2CM.SERCOM_ADDR = (addr << 1) & ~0x01;

    // Step 2: Wait for and process interrupt flags
    while (!(SERCOM2_REGS->I2CM.SERCOM_INTFLAG & (SERCOM_I2CM_INTFLAG_MB_Msk | SERCOM_I2CM_INTFLAG_ERROR_Msk)));
    
    if (SERCOM2_REGS->I2CM.SERCOM_INTFLAG & SERCOM_I2CM_INTFLAG_ERROR_Msk) {
        SERCOM2_I2C_Abort();
        return 0;
    }
       
    // Step 3: Write data, wait for ACKs
    for (pos = 0; pos < len; pos++) {
        // write data
        SERCOM2_REGS->I2CM.SERCOM_DATA = buf[pos];

        // check for ack
        while (!(SERCOM2_REGS->I2CM.SERCOM_INTFLAG & (SERCOM_I2CM_INTFLAG_MB_Msk | SERCOM_I2CM_INTFLAG_ERROR_Msk)));

        // if error, abort and exit
        if (SERCOM2_REGS->I2CM.SERCOM_INTFLAG & SERCOM_I2CM_INTFLAG_ERROR_Msk) {
            SERCOM2_I2C_Abort();
            return pos;
        }
    }
    
    // Step 4: Issue stop condition and return state back to idle
    SERCOM2_REGS->I2CM.SERCOM_CTRLB |= SERCOM_I2CM_CTRLB_CMD(0x3);

    return pos;
}

uint16_t SERCOM2_I2C_Read_Polled(uint8_t addr, uint8_t *buf, uint16_t len) {
    uint16_t pos = 0;

    // Step 0: Wait for the bus to become idle
    while ((SERCOM2_REGS->I2CM.SERCOM_STATUS & SERCOM_I2CM_STATUS_BUSSTATE_Msk) != SERCOM_I2CM_STATUS_BUSSTATE(0x01UL)) {
        if ((SERCOM2_REGS->I2CM.SERCOM_STATUS & SERCOM_I2CM_STATUS_BUSSTATE_Msk) == SERCOM_I2CM_STATUS_BUSSTATE(0x00UL)) {
            SERCOM2_I2C_Abort();
            return 0;
        }
    }

    // Step 1: Initialize transfer by writing to ADDR (ADDR[0] indicates direction)
    SERCOM2_REGS->I2CM.SERCOM_ADDR = (addr << 1) & ~0x01; // Set read bit

    // Step 2: Wait for SB flag and check for errors
    while ((SERCOM2_REGS->I2CM.SERCOM_INTFLAG & (1 << 1)) != 1) {
        asm("nop"); // If Master Bit is not set do nothing
    }

    // Step 3: Read data, respond with ACKs (I2C smart mode automatically sends an ACK when DATA is read)
    for (pos = 0; pos < len; pos++) {
        // Read data
        buf[pos] = SERCOM2_REGS->I2CM.SERCOM_DATA;

        // If this is not the last byte, wait for the next byte
        if (pos < len - 1) {
            // Wait for SB flag or error
            while ((SERCOM2_REGS->I2CM.SERCOM_INTFLAG & (SERCOM_I2CM_INTFLAG_SB_Msk | SERCOM_I2CM_INTFLAG_ERROR_Msk)) == 0) {
                asm("nop");
            }

            if (SERCOM2_REGS->I2CM.SERCOM_INTFLAG & SERCOM_I2CM_INTFLAG_ERROR_Msk) {
                SERCOM2_I2C_Abort();
                return pos;
            }
        }
    }

    // Step 4: Issue stop condition and return state back to idle
    SERCOM2_REGS->I2CM.SERCOM_CTRLB |= SERCOM_I2CM_CTRLB_CMD(0x3);

    return pos; // Return number of bytes successfully read
}

void SERCOM2_I2C_Abort() {
    // turn it off and on again
    SERCOM2_REGS->I2CM.SERCOM_CTRLA &= ~SERCOM_I2CM_CTRLA_ENABLE_Msk;
    while ((SERCOM2_REGS->I2CM.SERCOM_SYNCBUSY) != 0);

    SERCOM2_REGS->I2CM.SERCOM_CTRLA |= SERCOM_I2CM_CTRLA_ENABLE_Msk;
    while ((SERCOM2_REGS->I2CM.SERCOM_SYNCBUSY) != 0);

    // reinitialize state to idle
    SERCOM2_REGS->I2CM.SERCOM_STATUS = (uint16_t) SERCOM_I2CM_STATUS_BUSSTATE(0x01UL);
    while ((SERCOM2_REGS->I2CM.SERCOM_SYNCBUSY) != 0);
}