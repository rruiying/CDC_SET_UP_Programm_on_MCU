/* ========================================
 * Simple Fun Temperature Programs
 * 简单有趣的温度程序 - 稳定版
 * ========================================
*/

#include "project.h"
#include <stdio.h>

/* Global variables */
float last_temp = 25.0;
float max_temp = 0;
float min_temp = 50;

/* Simple temperature read function */
float GetTemperature(void) {
    uint8 temp_lsb, temp_msb;
    int16 temp_raw;
    float temperature;
    
    /* Reset pulse */
    OneWire_Pin_Write(0);
    CyDelayUs(480);
    OneWire_Pin_Write(1);
    CyDelayUs(480);
    
    /* Skip ROM - 0xCC */
    uint8 cmd = 0xCC;
    for (int i = 0; i < 8; i++) {
        if (cmd & 0x01) {
            OneWire_Pin_Write(0);
            CyDelayUs(6);
            OneWire_Pin_Write(1);
            CyDelayUs(64);
        } else {
            OneWire_Pin_Write(0);
            CyDelayUs(60);
            OneWire_Pin_Write(1);
            CyDelayUs(10);
        }
        cmd >>= 1;
    }
    
    /* Convert T - 0x44 */
    cmd = 0x44;
    for (int i = 0; i < 8; i++) {
        if (cmd & 0x01) {
            OneWire_Pin_Write(0);
            CyDelayUs(6);
            OneWire_Pin_Write(1);
            CyDelayUs(64);
        } else {
            OneWire_Pin_Write(0);
            CyDelayUs(60);
            OneWire_Pin_Write(1);
            CyDelayUs(10);
        }
        cmd >>= 1;
    }
    
    /* Wait for conversion */
    CyDelay(750);
    
    /* Reset again */
    OneWire_Pin_Write(0);
    CyDelayUs(480);
    OneWire_Pin_Write(1);
    CyDelayUs(480);
    
    /* Skip ROM - 0xCC */
    cmd = 0xCC;
    for (int i = 0; i < 8; i++) {
        if (cmd & 0x01) {
            OneWire_Pin_Write(0);
            CyDelayUs(6);
            OneWire_Pin_Write(1);
            CyDelayUs(64);
        } else {
            OneWire_Pin_Write(0);
            CyDelayUs(60);
            OneWire_Pin_Write(1);
            CyDelayUs(10);
        }
        cmd >>= 1;
    }
    
    /* Read Scratchpad - 0xBE */
    cmd = 0xBE;
    for (int i = 0; i < 8; i++) {
        if (cmd & 0x01) {
            OneWire_Pin_Write(0);
            CyDelayUs(6);
            OneWire_Pin_Write(1);
            CyDelayUs(64);
        } else {
            OneWire_Pin_Write(0);
            CyDelayUs(60);
            OneWire_Pin_Write(1);
            CyDelayUs(10);
        }
        cmd >>= 1;
    }
    
    /* Read LSB */
    temp_lsb = 0;
    for (int i = 0; i < 8; i++) {
        temp_lsb >>= 1;
        OneWire_Pin_Write(0);
        CyDelayUs(3);
        OneWire_Pin_Write(1);
        CyDelayUs(12);
        if (OneWire_Pin_Read()) {
            temp_lsb |= 0x80;
        }
        CyDelayUs(50);
    }
    
    /* Read MSB */
    temp_msb = 0;
    for (int i = 0; i < 8; i++) {
        temp_msb >>= 1;
        OneWire_Pin_Write(0);
        CyDelayUs(3);
        OneWire_Pin_Write(1);
        CyDelayUs(12);
        if (OneWire_Pin_Read()) {
            temp_msb |= 0x80;
        }
        CyDelayUs(50);
    }
    
    /* Calculate temperature */
    temp_raw = (temp_msb << 8) | temp_lsb;
    temperature = temp_raw * 0.0625;
    
    return temperature;
}

/* Program 1: Temperature Bar Graph */
void TemperatureBarGraph(void) {
    float temp;
    char buffer[80];
    
    UART_UartPutString("\r\n==== Temperature Bar Graph ====\r\n");
    UART_UartPutString("Visual temperature display\r\n");
    UART_UartPutString("Press any key to stop...\r\n\r\n");
    
    UART_UartPutString("15C         25C         35C\r\n");
    UART_UartPutString(" |           |           |\r\n");
    
    while(UART_SpiUartGetRxBufferSize() == 0) {
        temp = GetTemperature();
        
        /* Display temperature */
        int temp_int = (int)temp;
        int temp_dec = (int)((temp - temp_int) * 10);
        sprintf(buffer, "\r%d.%d C ", temp_int, temp_dec);
        UART_UartPutString(buffer);
        
        /* Draw bar */
        int bar_length = (int)((temp - 15.0) * 2);
        if (bar_length < 0) bar_length = 0;
        if (bar_length > 40) bar_length = 40;
        
        UART_UartPutString("[");
        for (int i = 0; i < 40; i++) {
            if (i < bar_length) {
                if (temp < 20) UART_UartPutString("-");
                else if (temp < 25) UART_UartPutString("=");
                else if (temp < 30) UART_UartPutString("#");
                else UART_UartPutString("!");
            } else {
                UART_UartPutString(" ");
            }
        }
        UART_UartPutString("]");
        
        CyDelay(200);
    }
    
    while(UART_SpiUartGetRxBufferSize() > 0) UART_UartGetChar();
}

/* Program 2: Temperature Meter */
void TemperatureMeter(void) {
    float temp, temp_change;
    char buffer[80];
    
    UART_UartPutString("\r\n==== Temperature Meter ====\r\n");
    UART_UartPutString("Shows temperature with trend\r\n");
    UART_UartPutString("Press any key to stop...\r\n\r\n");
    
    last_temp = GetTemperature();
    
    while(UART_SpiUartGetRxBufferSize() == 0) {
        temp = GetTemperature();
        temp_change = temp - last_temp;
        
        /* Display temperature */
        int temp_int = (int)temp;
        int temp_dec = (int)((temp - temp_int) * 10);
        sprintf(buffer, "\rTemp: %d.%d C ", temp_int, temp_dec);
        UART_UartPutString(buffer);
        
        /* Show trend */
        if (temp_change > 0.1) {
            UART_UartPutString("Rising  ^^^ ");
        } else if (temp_change < -0.1) {
            UART_UartPutString("Falling vvv ");
        } else {
            UART_UartPutString("Stable  --- ");
        }
        
        /* Show emoji-like status */
        if (temp < 20) {
            UART_UartPutString("[COLD]    ");
        } else if (temp < 25) {
            UART_UartPutString("[COOL]    ");
        } else if (temp < 30) {
            UART_UartPutString("[WARM]    ");
        } else {
            UART_UartPutString("[HOT!]    ");
        }
        
        /* Update min/max */
        if (temp > max_temp) {
            max_temp = temp;
            UART_UartPutString("*MAX*");
        }
        if (temp < min_temp) {
            min_temp = temp;
            UART_UartPutString("*MIN*");
        }
        
        last_temp = temp;
        CyDelay(500);
    }
    
    while(UART_SpiUartGetRxBufferSize() > 0) UART_UartGetChar();
}

/* Program 3: Temperature Game */
void TemperatureGame(void) {
    float target, current;
    char buffer[80];
    int score = 0;
    int round;
    
    UART_UartPutString("\r\n==== Temperature Target Game ====\r\n");
    UART_UartPutString("Heat the sensor to the target!\r\n\r\n");
    
    current = GetTemperature();
    
    for (round = 1; round <= 3; round++) {
        sprintf(buffer, "Round %d of 3\r\n", round);
        UART_UartPutString(buffer);
        
        /* Set target 2-4 degrees higher */
        target = current + 2.0 + (round - 1);
        
        int target_int = (int)target;
        sprintf(buffer, "Target: %d C\r\n", target_int);
        UART_UartPutString(buffer);
        UART_UartPutString("GO!\r\n\r\n");
        
        /* Game loop */
        while(1) {
            current = GetTemperature();
            
            int current_int = (int)current;
            sprintf(buffer, "\rCurrent: %d C ", current_int);
            UART_UartPutString(buffer);
            
            /* Distance indicator */
            int diff = (int)(target - current);
            if (diff > 3) {
                UART_UartPutString("[.......]");
            } else if (diff > 2) {
                UART_UartPutString("[==.....]");
            } else if (diff > 1) {
                UART_UartPutString("[====...]");
            } else if (diff > 0) {
                UART_UartPutString("[======.]");
            } else {
                UART_UartPutString("[=======]");
                UART_UartPutString(" SUCCESS!\r\n");
                score += 10;
                break;
            }
            
            CyDelay(300);
        }
        
        CyDelay(1000);
    }
    
    sprintf(buffer, "\r\nGame Over! Final Score: %d/30\r\n", score);
    UART_UartPutString(buffer);
}

/* Main program */
int main(void) {
    uint8 choice;
    float temp;
    char buffer[80];
    
    /* Enable global interrupts */
    CyGlobalIntEnable;
    
    /* Start UART */
    UART_Start();
    
    /* Welcome */
    UART_UartPutString("\033[2J\033[H");
    UART_UartPutString("============================\r\n");
    UART_UartPutString("  Simple Temperature Fun!   \r\n");
    UART_UartPutString("============================\r\n");
    
    /* Test sensor */
    UART_UartPutString("\r\nTesting sensor... ");
    
    OneWire_Pin_Write(0);
    CyDelayUs(480);
    OneWire_Pin_Write(1);
    CyDelayUs(70);
    
    if (!OneWire_Pin_Read()) {
        UART_UartPutString("OK!\r\n");
        CyDelayUs(410);
        
        /* Get initial temperature */
        temp = GetTemperature();
        int temp_int = (int)temp;
        sprintf(buffer, "Current temperature: %d C\r\n", temp_int);
        UART_UartPutString(buffer);
        min_temp = max_temp = temp;
    } else {
        UART_UartPutString("FAILED!\r\n");
        UART_UartPutString("Check connections!\r\n");
        while(1);
    }
    
    /* Main menu */
    for(;;) {
        UART_UartPutString("\r\n=== Menu ===\r\n");
        UART_UartPutString("1. Temperature Bar Graph\r\n");
        UART_UartPutString("2. Temperature Meter\r\n");
        UART_UartPutString("3. Temperature Game\r\n");
        UART_UartPutString("\r\nSelect (1-3): ");
        
        while(UART_SpiUartGetRxBufferSize() == 0);
        choice = UART_UartGetChar();
        UART_UartPutChar(choice);
        UART_UartPutString("\r\n");
        
        switch(choice) {
            case '1':
                TemperatureBarGraph();
                break;
            case '2':
                TemperatureMeter();
                break;
            case '3':
                TemperatureGame();
                break;
            default:
                UART_UartPutString("Invalid!\r\n");
        }
        
        CyDelay(1000);
    }
}

/* [] END OF FILE */