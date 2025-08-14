/*
 * main.c - VL6180X距离传感器完整版本
 * 包含初始化和距离测量功能
 */

#include "project.h"
#include <stdio.h>

// VL6180X寄存器地址
#define VL6180X_I2C_ADDR                    0x29

// 系统寄存器
#define VL6180X_IDENTIFICATION_MODEL_ID     0x000
#define VL6180X_IDENTIFICATION_MODEL_REV    0x001
#define VL6180X_IDENTIFICATION_MODULE_REV   0x002
#define VL6180X_IDENTIFICATION_DATE_HI      0x006
#define VL6180X_IDENTIFICATION_DATE_LO      0x007
#define VL6180X_IDENTIFICATION_TIME         0x008

#define VL6180X_SYSTEM_MODE_GPIO0           0x010
#define VL6180X_SYSTEM_MODE_GPIO1           0x011
#define VL6180X_SYSTEM_HISTORY_CTRL         0x012
#define VL6180X_SYSTEM_INTERRUPT_CONFIG_GPIO 0x014
#define VL6180X_SYSTEM_INTERRUPT_CLEAR      0x015
#define VL6180X_SYSTEM_FRESH_OUT_OF_RESET   0x016
#define VL6180X_SYSTEM_GROUPED_PARAMETER_HOLD 0x017

// 测距寄存器
#define VL6180X_SYSRANGE_START              0x018
#define VL6180X_SYSRANGE_THRESH_HIGH        0x019
#define VL6180X_SYSRANGE_THRESH_LOW         0x01A
#define VL6180X_SYSRANGE_INTERMEASUREMENT_PERIOD 0x01B
#define VL6180X_SYSRANGE_MAX_CONVERGENCE_TIME 0x01C
#define VL6180X_SYSRANGE_RANGE_CHECK_ENABLES 0x02D
#define VL6180X_SYSRANGE_VHV_RECALIBRATE    0x02E
#define VL6180X_SYSRANGE_VHV_REPEAT_RATE    0x031

// 结果寄存器
#define VL6180X_RESULT_RANGE_STATUS         0x04D
#define VL6180X_RESULT_INTERRUPT_STATUS_GPIO 0x04F
#define VL6180X_RESULT_RANGE_VAL            0x062
#define VL6180X_RESULT_HISTORY_BUFFER_0     0x052
#define VL6180X_RESULT_HISTORY_BUFFER_1     0x053

// 函数声明
void uart_print(char* str);
void uart_print_number(uint16 num);
void uart_print_hex(uint8 value);
uint8 vl6180x_read_byte(uint16 reg_addr);
void vl6180x_write_byte(uint16 reg_addr, uint8 data);
uint8 vl6180x_init(void);
uint8 vl6180x_read_distance(void);
void vl6180x_configure_default(void);
void display_status_code(uint8 status);

// UART输出函数
void uart_print(char* str)
{
    UART_UartPutString(str);
}

void uart_print_number(uint16 num)
{
    char buffer[10];
    sprintf(buffer, "%d", num);
    uart_print(buffer);
}

void uart_print_hex(uint8 value)
{
    char buffer[5];
    sprintf(buffer, "0x%02X", value);
    uart_print(buffer);
}

// VL6180X写字节
void vl6180x_write_byte(uint16 reg_addr, uint8 data)
{
    uint32 status;
    
    status = I2C_Distance_I2CMasterSendStart(VL6180X_I2C_ADDR, I2C_Distance_I2C_WRITE_XFER_MODE, 1000);
    if(status != I2C_Distance_I2C_MSTR_NO_ERROR) {
        I2C_Distance_I2CMasterSendStop(1000);
        return;
    }
    
    I2C_Distance_I2CMasterWriteByte((reg_addr >> 8) & 0xFF, 1000);
    I2C_Distance_I2CMasterWriteByte(reg_addr & 0xFF, 1000);
    I2C_Distance_I2CMasterWriteByte(data, 1000);
    I2C_Distance_I2CMasterSendStop(1000);
    CyDelay(1);
}

// VL6180X读字节
uint8 vl6180x_read_byte(uint16 reg_addr)
{
    uint32 status;
    uint8 read_data = 0;
    
    // 写地址
    status = I2C_Distance_I2CMasterSendStart(VL6180X_I2C_ADDR, I2C_Distance_I2C_WRITE_XFER_MODE, 1000);
    if(status != I2C_Distance_I2C_MSTR_NO_ERROR) {
        I2C_Distance_I2CMasterSendStop(1000);
        return 0;
    }
    
    I2C_Distance_I2CMasterWriteByte((reg_addr >> 8) & 0xFF, 1000);
    I2C_Distance_I2CMasterWriteByte(reg_addr & 0xFF, 1000);
    
    // 读数据
    I2C_Distance_I2CMasterSendRestart(VL6180X_I2C_ADDR, I2C_Distance_I2C_READ_XFER_MODE, 1000);
    I2C_Distance_I2CMasterReadByte(I2C_Distance_I2C_NAK_DATA, &read_data, 1000);
    I2C_Distance_I2CMasterSendStop(1000);
    
    return read_data;
}

// VL6180X默认配置
void vl6180x_configure_default(void)
{
    // 必需的初始化序列（来自ST应用笔记）
    vl6180x_write_byte(0x0207, 0x01);
    vl6180x_write_byte(0x0208, 0x01);
    vl6180x_write_byte(0x0096, 0x00);
    vl6180x_write_byte(0x0097, 0xfd);
    vl6180x_write_byte(0x00e3, 0x00);
    vl6180x_write_byte(0x00e4, 0x04);
    vl6180x_write_byte(0x00e5, 0x02);
    vl6180x_write_byte(0x00e6, 0x01);
    vl6180x_write_byte(0x00e7, 0x03);
    vl6180x_write_byte(0x00f5, 0x02);
    vl6180x_write_byte(0x00d9, 0x05);
    vl6180x_write_byte(0x00db, 0xce);
    vl6180x_write_byte(0x00dc, 0x03);
    vl6180x_write_byte(0x00dd, 0xf8);
    vl6180x_write_byte(0x009f, 0x00);
    vl6180x_write_byte(0x00a3, 0x3c);
    vl6180x_write_byte(0x00b7, 0x00);
    vl6180x_write_byte(0x00bb, 0x3c);
    vl6180x_write_byte(0x00b2, 0x09);
    vl6180x_write_byte(0x00ca, 0x09);
    vl6180x_write_byte(0x0198, 0x01);
    vl6180x_write_byte(0x01b0, 0x17);
    vl6180x_write_byte(0x01ad, 0x00);
    vl6180x_write_byte(0x00ff, 0x05);
    vl6180x_write_byte(0x0100, 0x05);
    vl6180x_write_byte(0x0199, 0x05);
    vl6180x_write_byte(0x01a6, 0x1b);
    vl6180x_write_byte(0x01ac, 0x3e);
    vl6180x_write_byte(0x01a7, 0x1f);
    vl6180x_write_byte(0x0030, 0x00);
    
    // 配置测距参数
    vl6180x_write_byte(VL6180X_SYSRANGE_MAX_CONVERGENCE_TIME, 0x32); // 最大收敛时间50ms
    vl6180x_write_byte(VL6180X_SYSRANGE_RANGE_CHECK_ENABLES, 0x10 | 0x01); // 使能范围检查
    vl6180x_write_byte(0x002e, 0x01); // 提前收敛估计
    
    // 配置中断（可选）
    vl6180x_write_byte(VL6180X_SYSTEM_MODE_GPIO1, 0x10); // GPIO1中断使能
    vl6180x_write_byte(VL6180X_SYSTEM_INTERRUPT_CONFIG_GPIO, 0x24); // 新样本准备中断
}

// VL6180X初始化
uint8 vl6180x_init(void)
{
    uint8 model_id;
    uint8 fresh_out_of_reset;
    
    uart_print("\r\n=== VL6180X Initialization ===\r\n");
    
    // 读取并验证设备ID
    model_id = vl6180x_read_byte(VL6180X_IDENTIFICATION_MODEL_ID);
    uart_print("Model ID: ");
    uart_print_hex(model_id);
    
    if(model_id != 0xB4) {
        uart_print(" - ERROR: Wrong ID!\r\n");
        return 0;
    }
    uart_print(" - OK\r\n");
    
    // 读取版本信息
    uint8 model_rev = vl6180x_read_byte(VL6180X_IDENTIFICATION_MODEL_REV);
    uint8 module_rev = vl6180x_read_byte(VL6180X_IDENTIFICATION_MODULE_REV);
    uart_print("Model Rev: ");
    uart_print_number(model_rev >> 4);
    uart_print(".");
    uart_print_number(model_rev & 0x0F);
    uart_print(", Module Rev: ");
    uart_print_number(module_rev >> 4);
    uart_print(".");
    uart_print_number(module_rev & 0x0F);
    uart_print("\r\n");
    
    // 检查是否需要初始化
    fresh_out_of_reset = vl6180x_read_byte(VL6180X_SYSTEM_FRESH_OUT_OF_RESET);
    if(fresh_out_of_reset == 1) {
        uart_print("Fresh boot detected, applying configuration...\r\n");
        
        // 应用默认配置
        vl6180x_configure_default();
        
        // 清除复位标志
        vl6180x_write_byte(VL6180X_SYSTEM_FRESH_OUT_OF_RESET, 0x00);
        
        uart_print("Configuration complete\r\n");
    }
    else {
        uart_print("Already configured\r\n");
    }
    
    // 设置默认测量模式（单次测量）
    vl6180x_write_byte(VL6180X_SYSRANGE_START, 0x00);
    
    uart_print("✓ Initialization successful\r\n");
    return 1;
}

// 显示状态码含义
void display_status_code(uint8 status)
{
    uint8 error_code = (status >> 4) & 0x0F;
    
    switch(error_code) {
        case 0:  uart_print("[No error]"); break;
        case 1:  uart_print("[VCSEL continuity test]"); break;
        case 2:  uart_print("[VCSEL watchdog test]"); break;
        case 3:  uart_print("[VCSEL watchdog]"); break;
        case 4:  uart_print("[PLL1 lock]"); break;
        case 5:  uart_print("[PLL2 lock]"); break;
        case 6:  uart_print("[Early convergence estimate]"); break;
        case 7:  uart_print("[Max convergence]"); break;
        case 8:  uart_print("[No target ignore]"); break;
        case 11: uart_print("[Max SNR]"); break;
        case 12: uart_print("[Raw ranging algo underflow]"); break;
        case 13: uart_print("[Raw ranging algo overflow]"); break;
        case 14: uart_print("[Ranging algo underflow]"); break;
        case 15: uart_print("[Ranging algo overflow]"); break;
        default: uart_print("[Unknown error]"); break;
    }
}

// 读取距离
uint8 vl6180x_read_distance(void)
{
    uint8 status;
    uint8 distance;
    uint16 timeout = 0;
    
    // 清除中断
    vl6180x_write_byte(VL6180X_SYSTEM_INTERRUPT_CLEAR, 0x07);
    
    // 启动单次测量
    vl6180x_write_byte(VL6180X_SYSRANGE_START, 0x01);
    
    // 等待测量完成
    do {
        CyDelay(1);
        status = vl6180x_read_byte(VL6180X_RESULT_INTERRUPT_STATUS_GPIO);
        timeout++;
        if(timeout > 100) {  // 100ms超时
            uart_print("Timeout! ");
            return 0xFF;
        }
    } while((status & 0x04) == 0);
    
    // 读取距离值
    distance = vl6180x_read_byte(VL6180X_RESULT_RANGE_VAL);
    
    // 读取状态
    status = vl6180x_read_byte(VL6180X_RESULT_RANGE_STATUS);
    uint8 error_code = (status >> 4) & 0x0F;
    
    // 清除中断
    vl6180x_write_byte(VL6180X_SYSTEM_INTERRUPT_CLEAR, 0x07);
    
    // 检查错误
    if(error_code != 0) {
        if(error_code == 11) {
            // 没有目标 - 这是正常的
            return 0xFF;
        }
        // 其他错误
        uart_print("Error ");
        uart_print_number(error_code);
        uart_print(" ");
        return 0xFF;
    }
    
    return distance;
}

// 主函数
int main(void)
{
    uint8 distance;
    uint16 measurement_count = 0;
    uint8 continuous_errors = 0;
    
    CyGlobalIntEnable;
    
    // 启动外设
    UART_Start();
    I2C_Distance_Start();
    CyDelay(100);
    
    // 启动信息
    uart_print("\r\n\r\n");
    uart_print("=====================================\r\n");
    uart_print("  VL6180X Distance Sensor Complete  \r\n");
    uart_print("=====================================\r\n");
    
    // 初始化VL6180X
    if(!vl6180x_init()) {
        uart_print("\r\n✗ Initialization failed!\r\n");
        uart_print("System halted.\r\n");
        while(1) {
            CyDelay(1000);
        }
    }
    
    uart_print("\r\n=== Starting Distance Measurements ===\r\n");
    uart_print("Range: 0-200mm, Updates every 500ms\r\n\r\n");
    
    // 主循环 - 连续测量距离
    for(;;)
    {
        distance = vl6180x_read_distance();
        measurement_count++;
        
        // 显示测量序号
        if((measurement_count % 10) == 1) {
            uart_print("\r\n--- Measurement Block ");
            uart_print_number(measurement_count / 10 + 1);
            uart_print(" ---\r\n");
        }
        
        uart_print("#");
        uart_print_number(measurement_count);
        uart_print(": ");
        
        if(distance == 0xFF) {
            uart_print("Out of range / No target");
            continuous_errors++;
        }
        else {
            uart_print_number(distance);
            uart_print(" mm ");
            
            // 距离条形图
            uint8 bars = distance / 10;  // 每10mm一个条
            if(bars > 20) bars = 20;
            
            uart_print("[");
            for(uint8 i = 0; i < bars; i++) {
                uart_print("=");
            }
            for(uint8 i = bars; i < 20; i++) {
                uart_print(" ");
            }
            uart_print("]");
            
            // 距离判断
            if(distance < 20) {
                uart_print(" WARNING: Too close!");
            }
            else if(distance < 50) {
                uart_print(" Near");
            }
            else if(distance < 100) {
                uart_print(" Medium");
            }
            else if(distance < 150) {
                uart_print(" Far");
            }
            else {
                uart_print(" Very far");
            }
            
            continuous_errors = 0;
        }
        
        uart_print("\r\n");
        
        // 如果连续错误太多，尝试重新初始化
        if(continuous_errors > 10) {
            uart_print("\r\nToo many errors, reinitializing...\r\n");
            vl6180x_init();
            continuous_errors = 0;
        }
        
        CyDelay(500);  // 500ms测量间隔
    }
}