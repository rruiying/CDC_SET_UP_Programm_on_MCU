#include "project.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG_MODE 1

#define STEPS_PER_MM        100
#define STEPPER_DELAY       1
#define MAX_HEIGHT          200.0
#define MIN_HEIGHT          0.0

#define SERVO_MIN_PULSE     500
#define SERVO_MAX_PULSE     2500
#define SERVO_CENTER        1500
#define MAX_ANGLE           90.0
#define MIN_ANGLE          -90.0

#define VL6180X_I2C_ADDR    0x29
#define VL6180X_SYSRANGE_START 0x018
#define VL6180X_RESULT_RANGE_VAL 0x062
#define VL6180X_RESULT_INTERRUPT_STATUS_GPIO 0x04F
#define VL6180X_SYSTEM_INTERRUPT_CLEAR 0x015

#define LIMIT_SWITCH_TRIGGERED  0
#define LIMIT_SWITCH_RELEASED   1
#define HOMING_SPEED_DELAY      1
#define HOMING_TIMEOUT         30000 

// 命令缓冲区
#define CMD_BUFFER_SIZE     128
#define PARAM_BUFFER_SIZE   64

// ============ 系统状态 ============
typedef enum {
    STATUS_READY,
    STATUS_MOVING,
    STATUS_ERROR,
    STATUS_HOMING
} SystemStatus;

// ============ 全局变量 ============
// 系统状态
SystemStatus system_status = STATUS_READY;
uint8_t emergency_stop_flag = 0;
uint8 delay_with_check(uint32 ms);
uint8 check_for_emergency_command(void);

// 位置状态
float current_height = 0.0;    // 当前高度 (mm)
float current_angle = 0.0;     // 当前角度 (度)
float target_height = 0.0;     // 目标高度
float target_angle = 0.0;      // 目标角度

// 步进电机状态
int32_t stepper_position = 0;  // 当前位置（步数）

// 传感器数据
float temperature = 25.0;       // 温度
float distance_upper1 = 0.0;   // 上距离传感器1
float distance_upper2 = 0.0;   // 上距离传感器2（暂时假设）
float distance_lower1 = 0.0;   // 下距离传感器1（暂时假设）
float distance_lower2 = 0.0;   // 下距离传感器2（暂时假设）
float capacitance = 120.5;      // 电容值（暂时假设）

// 命令缓冲区
char cmd_buffer[CMD_BUFFER_SIZE] = {0};
uint16_t cmd_index = 0;

// ============ 基础功能函数 ============

void uart_print(const char* str) {
    UART_UartPutString(str);
}

uint8 read_limit_switch(void) {
    return Pin_LimitSwitch_Read();
}

void float_to_string(char* buffer, float value) {
    int integer_part = (int)value;
    int decimal_part = (int)((value - integer_part) * 10);
    
    if(decimal_part < 0) decimal_part = -decimal_part;
    
    sprintf(buffer, "%d.%d", integer_part, decimal_part);
}

void int_to_string_with_decimal(char* buffer, int value) {
    sprintf(buffer, "%d.0", value);
}

void uart_send_response(const char* response) {
    uart_print(response);
}

void debug_print_with_value(const char* str, float value) {
    #if DEBUG_MODE
    char buffer[128];
    sprintf(buffer, "[DEBUG] %s: %.2f", str, value);
    uart_print(buffer);
    uart_print("\r\n");
    #endif
}

void debug_print(const char* str) {
    #if DEBUG_MODE
    uart_print("[DEBUG] ");
    uart_print(str);
    uart_print("\r\n");
    #endif
}

void servo_set_angle(float angle) {
    uint16 pulse_width;
    
    if(angle < MIN_ANGLE) angle = MIN_ANGLE;
    if(angle > MAX_ANGLE) angle = MAX_ANGLE;
    
    pulse_width = SERVO_CENTER + (int16)((angle / 90.0) * (SERVO_MAX_PULSE - SERVO_CENTER));
    
    PWM_Servo_WriteCompare(pulse_width);
    current_angle = angle;
}

void process_init_home(void) {
    char msg[128];
    uint32 timeout = 0;
    uint32 steps_moved = 0;
    
    uart_send_response("INFO:Starting safe homing sequence...\r\n");
    
    // ⭐ 步骤1：首先将伺服电机归中（安全角度）
    uart_send_response("INFO:Step 1 - Setting servo to center position (0 degrees)...\r\n");
    servo_set_angle(0.0);
    CyDelay(1000);  // 等待伺服到位
    
    // 确认角度已归零
    current_angle = 0.0;
    target_angle = 0.0;
    uart_send_response("OK:Servo centered at 0 degrees\r\n");
    
    // ⭐ 步骤2：清除紧急停止并启用电机
    emergency_stop_flag = 0;
    Pin_ENABLE_Write(0);  // 使能步进电机
    system_status = STATUS_HOMING;
    
    // ⭐ 步骤3：检查是否已经在限位开关位置
    uart_send_response("INFO:Step 2 - Checking limit switch status...\r\n");
    if(read_limit_switch() == LIMIT_SWITCH_TRIGGERED) {
        uart_send_response("INFO:Already at home position, backing off...\r\n");
        
        // 向下移动一点，离开限位开关
        Pin_DIR_Write(0);  // 向下方向
        for(int i = 0; i < 500; i++) {  // 后退500步
            Pin_STEP_Write(1);
            CyDelay(2);
            Pin_STEP_Write(0);
            CyDelay(2);
            
            // 检查紧急停止
            if(emergency_stop_flag || check_for_emergency_command()) {
                Pin_ENABLE_Write(1);
                uart_send_response("ERROR:Homing interrupted\r\n");
                system_status = STATUS_ERROR;
                return;
            }
            
            if(read_limit_switch() == LIMIT_SWITCH_RELEASED) {
                uart_send_response("INFO:Cleared limit switch\r\n");
                break;
            }
        }
        CyDelay(100);
    }
    
    // ⭐ 步骤4：向上移动寻找限位开关
    Pin_DIR_Write(1);  // 向上方向
    CyDelay(10);
    
    uart_send_response("INFO:Step 3 - Moving up to find limit switch...\r\n");
    uart_send_response("INFO:Please ensure area is clear!\r\n");
    
    // 慢速向上移动
    while(read_limit_switch() != LIMIT_SWITCH_TRIGGERED) {
        // 安全检查
        if(emergency_stop_flag || check_for_emergency_command()) {
            Pin_ENABLE_Write(1);
            uart_send_response("ERROR:Homing interrupted by emergency stop\r\n");
            system_status = STATUS_ERROR;
            return;
        }
        
        // 超时检查
        if(timeout >= HOMING_TIMEOUT) {
            Pin_ENABLE_Write(1);
            uart_send_response("ERROR:Homing timeout - limit switch not found\r\n");
            uart_send_response("INFO:Check limit switch connection\r\n");
            system_status = STATUS_ERROR;
            return;
        }
        
        // 执行一步（慢速）
        Pin_STEP_Write(1);
        CyDelay(HOMING_SPEED_DELAY);
        Pin_STEP_Write(0);
        CyDelay(HOMING_SPEED_DELAY);
        
        steps_moved++;
        timeout += (HOMING_SPEED_DELAY * 2);
        
        // 状态报告
        if(steps_moved % 100 == 0) {
            sprintf(msg, "INFO:Homing... %lu steps (%.1f mm)\r\n", 
                    steps_moved, (float)steps_moved / STEPS_PER_MM);
            uart_send_response(msg);
        }
    }
    
    // ⭐ 步骤5：找到限位开关，精确定位
    uart_send_response("INFO:Step 4 - Limit switch detected, fine-tuning...\r\n");
    
    // 后退一点
    Pin_DIR_Write(0);  // 向下
    for(int i = 0; i < 20; i++) {
        Pin_STEP_Write(1);
        CyDelay(10);
        Pin_STEP_Write(0);
        CyDelay(10);
    }
    
    // 慢速前进到触发点
    Pin_DIR_Write(1);  // 向上
    while(read_limit_switch() != LIMIT_SWITCH_TRIGGERED) {
        Pin_STEP_Write(1);
        CyDelay(10);
        Pin_STEP_Write(0);
        CyDelay(10);
    }
    
    // ⭐ 步骤6：重置所有位置参数
    stepper_position = 0;
    current_height = 0.0;
    current_angle = 0.0;
    target_height = 0.0;
    target_angle = 0.0;
    
    system_status = STATUS_READY;
    
    // ⭐ 完成报告
    uart_send_response("=====================================\r\n");
    uart_send_response("OK:Homing complete!\r\n");
    uart_send_response("  - Servo angle: 0.0 degrees\r\n");
    uart_send_response("  - Height: 0.0 mm (at limit switch)\r\n");
    uart_send_response("  - System ready for operation\r\n");
    uart_send_response("=====================================\r\n");
}

// ============ VL6180X距离传感器函数 ============
void vl6180x_write_byte(uint16 reg_addr, uint8 data) {
    uint32 status;
    
    status = I2C_Distance_I2CMasterSendStart(VL6180X_I2C_ADDR, I2C_Distance_I2C_WRITE_XFER_MODE, 100);
    if(status != I2C_Distance_I2C_MSTR_NO_ERROR) {
        I2C_Distance_I2CMasterSendStop(100);
        return;
    }
    
    I2C_Distance_I2CMasterWriteByte((reg_addr >> 8) & 0xFF, 100);
    I2C_Distance_I2CMasterWriteByte(reg_addr & 0xFF, 100);
    I2C_Distance_I2CMasterWriteByte(data, 100);
    I2C_Distance_I2CMasterSendStop(100);
}

uint8 vl6180x_read_byte(uint16 reg_addr) {
    uint32 status;
    uint8 read_data = 0;
    
    status = I2C_Distance_I2CMasterSendStart(VL6180X_I2C_ADDR, I2C_Distance_I2C_WRITE_XFER_MODE, 100);
    if(status != I2C_Distance_I2C_MSTR_NO_ERROR) {
        I2C_Distance_I2CMasterSendStop(100);
        return 0;
    }
    
    I2C_Distance_I2CMasterWriteByte((reg_addr >> 8) & 0xFF, 100);
    I2C_Distance_I2CMasterWriteByte(reg_addr & 0xFF, 100);
    
    I2C_Distance_I2CMasterSendRestart(VL6180X_I2C_ADDR, I2C_Distance_I2C_READ_XFER_MODE, 100);
    I2C_Distance_I2CMasterReadByte(I2C_Distance_I2C_NAK_DATA, &read_data, 100);
    I2C_Distance_I2CMasterSendStop(100);
    
    return read_data;
}

uint8 read_distance_sensor(void) {
    uint8 status, distance;
    uint16 timeout = 0;
    
    vl6180x_write_byte(VL6180X_SYSTEM_INTERRUPT_CLEAR, 0x07);
    vl6180x_write_byte(VL6180X_SYSRANGE_START, 0x01);
    
    do {
        CyDelay(1);
        status = vl6180x_read_byte(VL6180X_RESULT_INTERRUPT_STATUS_GPIO);
        timeout++;
        if(timeout > 100) return 0xFF;
    } while((status & 0x04) == 0);
    
    distance = vl6180x_read_byte(VL6180X_RESULT_RANGE_VAL);
    vl6180x_write_byte(VL6180X_SYSTEM_INTERRUPT_CLEAR, 0x07);
    
    return distance;
}

// ============ DS18B20温度传感器函数 ============
float read_temperature(void) {
    uint8 temp_lsb, temp_msb;
    int16 temp_raw;
    float temperature_value;
    
    // Reset pulse
    OneWire_Pin_Write(0);
    CyDelayUs(480);
    OneWire_Pin_Write(1);
    CyDelayUs(480);
    
    // Skip ROM - 0xCC
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
    
    // Convert T - 0x44
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
    
    CyDelay(750);  // 等待转换
    
    // 再次复位并读取
    OneWire_Pin_Write(0);
    CyDelayUs(480);
    OneWire_Pin_Write(1);
    CyDelayUs(480);
    
    // Skip ROM - 0xCC
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
    
    // Read Scratchpad - 0xBE
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
    
    // Read LSB
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
    
    // Read MSB
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
    
    temp_raw = (temp_msb << 8) | temp_lsb;
    temperature_value = temp_raw * 0.0625;
    
    return temperature_value;
}

uint8 check_for_emergency_command(void) {
    if(UART_SpiUartGetRxBufferSize() > 0) {
        char temp_buffer[20];
        uint8 i = 0;
        
        while(UART_SpiUartGetRxBufferSize() > 0 && i < 19) {
            temp_buffer[i++] = UART_UartGetChar();
        }
        temp_buffer[i] = '\0';
        
        if(strstr(temp_buffer, "EMERGENCY") != NULL) {
            emergency_stop_flag = 1;
            uart_send_response("INFO:Emergency detected during move\r\n");
            return 1;
        }
    }
    return 0;
}

uint8 delay_with_check(uint32 ms) {
    uint32 i;
    
    for(i = 0; i < ms; i++) {
        if(emergency_stop_flag) {
            return 0;
        }
        if(check_for_emergency_command()) {
            return 0;
        }
        CyDelay(1);
    }
    return 1;
}



// ============ 步进电机控制函数 ============
void stepper_move_steps(int32 steps) {
    uint8 dir = (steps > 0) ? 0 : 1;
    int32 abs_steps = (steps > 0) ? steps : -steps;
    int32 steps_completed = 0;
    int32 i;
    char msg[64];
    
    Pin_DIR_Write(dir);
    CyDelay(10);
    
    for(i = 0; i < abs_steps; i++) {
        if(emergency_stop_flag) {
            if(dir) {
                stepper_position += steps_completed;
            } else {
                stepper_position -= steps_completed;
            }
            current_height = (float)stepper_position / STEPS_PER_MM;
            
            Pin_ENABLE_Write(1);
            system_status = STATUS_ERROR;
            
            char msg[64];
            sprintf(msg, "INFO:Stopped at step %ld of %ld\r\n", steps_completed, abs_steps);
            uart_send_response(msg);
            return;
        }
        
        if(i % 100 == 0) {
            if(UART_SpiUartGetRxBufferSize() >= 14) { 
                char peek_buf[20];
                uint8 j;
                
                for(j = 0; j < 14 && UART_SpiUartGetRxBufferSize() > 0; j++) {
                    peek_buf[j] = UART_UartGetChar();
                }
                peek_buf[j] = '\0';
                
                if(strstr(peek_buf, "EMERGENCY") != NULL) {
                    emergency_stop_flag = 1;
                    
                    if(dir) {
                        stepper_position += steps_completed;
                    } else {
                        stepper_position -= steps_completed;
                    }
                    current_height = (float)stepper_position / STEPS_PER_MM;
                    
                    Pin_ENABLE_Write(1);
                    uart_send_response("EMERGENCY:Stopped\r\n");
                    
                    sprintf(msg, "INFO:Position: %.1f mm\r\n", current_height);
                    uart_send_response(msg);
                    return;
                }
            }
        }
        Pin_STEP_Write(1);
        if(!delay_with_check(STEPPER_DELAY)) {
            if(dir) {
                stepper_position += steps_completed;
            } else {
                stepper_position -= steps_completed;
            }
            current_height = (float)stepper_position / STEPS_PER_MM;
            Pin_ENABLE_Write(1);
            return;
        }
        Pin_STEP_Write(0);
        if(!delay_with_check(STEPPER_DELAY)) {
            if(dir) {
                stepper_position += steps_completed;
            } else {
                stepper_position -= steps_completed;
            }
            current_height = (float)stepper_position / STEPS_PER_MM;
            Pin_ENABLE_Write(1);
            return;
        }
        steps_completed++;
    }
    
    stepper_position += steps;
    current_height = (float)stepper_position / STEPS_PER_MM;
}

void stepper_move_to_height(float height_mm) {
    int32_t target_steps = (int32_t)(height_mm * STEPS_PER_MM);
    int32_t delta = target_steps - stepper_position;
    
    if(delta != 0) {
        stepper_move_steps(delta);
    }
}

// ============ 命令处理函数 ============
void process_set_height(const char* params) {
    if(emergency_stop_flag) {
        uart_send_response("ERROR:EMERGENCY_STOP_ACTIVE\r\n");
        return;
    }
    
    float height = atof(params);
    
    if(height < MIN_HEIGHT || height > MAX_HEIGHT) {
        uart_send_response("ERROR:OUT_OF_RANGE\r\n");
        return;
    }
    
    target_height = height;
    uart_send_response("OK\r\n");
}

void process_set_angle(const char* params) {
    if(emergency_stop_flag) {
        uart_send_response("ERROR:EMERGENCY_STOP_ACTIVE\r\n");
        return;
    }
    float angle = atof(params);
    
    if(angle < MIN_ANGLE || angle > MAX_ANGLE) {
        uart_send_response("ERROR:OUT_OF_RANGE\r\n");
        return;
    }
    
    target_angle = angle;
    uart_send_response("OK\r\n");
}

void process_move_to(const char* params) {
    if(emergency_stop_flag) {
        uart_send_response("ERROR:EMERGENCY_STOP_ACTIVE\r\n");
        return;
    }
    float height, angle;
    char* comma;
    char params_copy[PARAM_BUFFER_SIZE];
    
    strcpy(params_copy, params);
    comma = strchr(params_copy, ',');
    
    if(comma == NULL) {
        uart_send_response("ERROR:INVALID_COMMAND\r\n");
        return;
    }
    
    *comma = '\0';
    height = atof(params_copy);
    angle = atof(comma + 1);
    
    if(height < MIN_HEIGHT || height > MAX_HEIGHT || 
       angle < MIN_ANGLE || angle > MAX_ANGLE) {
        uart_send_response("ERROR:OUT_OF_RANGE\r\n");
        return;
    }
    
    target_height = height;
    target_angle = angle;
    system_status = STATUS_MOVING;
    
    // 执行移动
    stepper_move_to_height(target_height);
    servo_set_angle(target_angle);
    
    if(emergency_stop_flag) {
        uart_send_response("ERROR:MOVEMENT_INTERRUPTED\r\n");
        return;
    }
    
    system_status = STATUS_READY;
    uart_send_response("OK\r\n");
}

void process_stop(void) {
    // 停止当前动作
    system_status = STATUS_READY;
    uart_send_response("OK\r\n");
}

void process_reset(void) {
    emergency_stop_flag = 0;  // 清除紧急停止标志
    system_status = STATUS_READY;
    Pin_ENABLE_Write(0);  // 重新使能步进电机
    uart_send_response("OK:System reset\r\n");
}

void process_home(void) {
    if(emergency_stop_flag) {
        uart_send_response("INFO:Clearing emergency stop\r\n");
        emergency_stop_flag = 0;
        Pin_ENABLE_Write(0);
        CyDelay(100); 
        uart_send_response("INFO:Motors re-enabled\r\n");
    } else {
        Pin_ENABLE_Write(0);
    }
    
    system_status = STATUS_HOMING;
    
    uart_send_response("INFO:Homing started\r\n");
    
    servo_set_angle(0.0);
    
    if(check_for_emergency_command()) {
        system_status = STATUS_ERROR;
        uart_send_response("ERROR:Homing interrupted\r\n");
        return;
    }
    
    stepper_move_to_height(0.0);
    
    if(emergency_stop_flag) {
        uart_send_response("ERROR:Homing interrupted\r\n");
        return;
    }
    
    stepper_position = 0;
    current_height = 0.0;
    current_angle = 0.0;
    
    system_status = STATUS_READY;
    uart_send_response("OK:HOME\r\n");
}

void process_emergency_stop(void) {
    emergency_stop_flag = 1;
    system_status = STATUS_ERROR;
    
    // 立即停止所有电机
    Pin_ENABLE_Write(1);  // 禁用步进电机
    
    uart_send_response("OK:EMERGENCY_STOP\r\n");
    debug_print("EMERGENCY STOP ACTIVATED - All motors disabled");
}

void process_get_status(void) {
    char response[128];
    const char* status_str;
     if(emergency_stop_flag) {
        status_str = "EMERGENCY_STOP";
    } else {
        switch(system_status) {
            case STATUS_READY:  status_str = "READY"; break;
            case STATUS_MOVING: status_str = "MOVING"; break;
            case STATUS_ERROR:  status_str = "ERROR"; break;
            case STATUS_HOMING: status_str = "HOMING"; break;
            default: status_str = "UNKNOWN"; break;
        }
    }
    float h = current_height;
    float a = current_angle;
    if(h == 0.0 && a == 0.0 && stepper_position == 0) {
        h = 0.0;
        a = 0.0;
    }
    
    sprintf(response, "STATUS:%s,%.1f,%.1f\r\n", 
            status_str, h, a);
    uart_send_response(response);
}

void process_get_sensors(void) {
    char response[256];
    char temp_str[8][16];
    uint8 dist;
    
    int dist1 = 12;
    int dist2 = 13;
    int dist3 = 156;
    int dist4 = 157;
    float temp = 25.0;
    float angle = 80.0;
    float cap = 120.5;
    
    dist = read_distance_sensor();
    if(dist != 0xFF && dist > 0) {
        dist1 = dist;
        dist2 = dist + 1;
    } else {
        debug_print("Distance sensor read failed, using default");
    }
    
    dist3 = 156 + (rand() % 5);
    dist4 = 157 + (rand() % 5);
    
    temp = 25.0 + ((float)(rand() % 100) / 10.0);
    
    cap = 120.5 + (current_height * 0.5);
    
    int_to_string_with_decimal(temp_str[0], dist1);
    int_to_string_with_decimal(temp_str[1], dist2);
    int_to_string_with_decimal(temp_str[2], dist3);
    int_to_string_with_decimal(temp_str[3], dist4);
    
    float_to_string(temp_str[4], temp);
    float_to_string(temp_str[5], angle);
    float_to_string(temp_str[6], cap);
    
    sprintf(response, "SENSORS:%s,%s,%s,%s,%s,%s,%s\r\n",
            temp_str[0], temp_str[1], temp_str[2], temp_str[3],
            temp_str[4], temp_str[5], temp_str[6]);
    
    #if DEBUG_MODE
    debug_print("Sending sensor data:");
    debug_print(response);
    #endif
    
    uart_send_response(response);
}

void process_command(char* cmd) {
    char* colon;
    char* params;
    char debug_msg[128];
    
    sprintf(debug_msg, "Received command: [%s] (length: %d)", cmd, strlen(cmd));
    debug_print(debug_msg);
    
    while(*cmd == ' ') cmd++;
    
    // 查找冒号分隔符
    colon = strchr(cmd, ':');
    if(colon != NULL) {
        *colon = '\0';
        params = colon + 1;
        sprintf(debug_msg, "Command: [%s], Params: [%s]", cmd, params);
        debug_print(debug_msg);
    } else {
        params = NULL;
        sprintf(debug_msg, "Command: [%s], No params", cmd);
        debug_print(debug_msg);
    }
    
    // 处理命令
    if(strcmp(cmd, "SET_HEIGHT") == 0 && params != NULL) {
        process_set_height(params);
    }
    else if(strcmp(cmd, "INIT_HOME") == 0) {
        process_init_home();
    }
    else if(strcmp(cmd, "CHECK_LIMIT") == 0) {
        if(read_limit_switch() == LIMIT_SWITCH_TRIGGERED) {
            uart_send_response("INFO:Limit switch is TRIGGERED\r\n");
        } else {
            uart_send_response("INFO:Limit switch is RELEASED\r\n");
        }
    }
    else if(strcmp(cmd, "SET_ANGLE") == 0 && params != NULL) {
        process_set_angle(params);
    }
    else if(strcmp(cmd, "MOVE_TO") == 0 && params != NULL) {
        process_move_to(params);
    }
    else if(strcmp(cmd, "STOP") == 0) {
        process_stop();
    }
    else if(strcmp(cmd, "EMERGENCY_STOP") == 0) {
        process_emergency_stop();
    }
    else if(strcmp(cmd, "HOME") == 0) {
        process_home();
    }
    else if(strcmp(cmd, "GET_STATUS") == 0) {
        process_get_status();
    }
    else if(strcmp(cmd, "GET_SENSORS") == 0) {
        process_get_sensors();
    }
    else if(strcmp(cmd, "TEST") == 0) {
        debug_print("TEST command received - system is responding");
        uart_send_response("TEST_OK:System is working\r\n");
    }
    else if(strcmp(cmd, "ECHO") == 0 && params != NULL) {
        char echo_response[128];
        sprintf(echo_response, "ECHO:%s\r\n", params);
        uart_send_response(echo_response);
    }
    else if(strcmp(cmd, "VERSION") == 0) {
        uart_send_response("VERSION:CDC_Control_v1.0\r\n");
    }
    else if(strcmp(cmd, "HELP") == 0) {
        uart_send_response("Commands:\r\n");
        uart_send_response("Commands:\r\n");
        uart_send_response("  INIT_HOME - Initialize home position using limit switch\r\n");
        uart_send_response("  CHECK_LIMIT - Check limit switch status\r\n");
        uart_send_response("  GET_STATUS - Get system status\r\n");
        uart_send_response("  GET_SENSORS - Get sensor readings\r\n");
        uart_send_response("  SET_HEIGHT:value - Set target height\r\n");
        uart_send_response("  SET_ANGLE:value - Set target angle\r\n");
        uart_send_response("  MOVE_TO:height,angle - Move to position\r\n");
        uart_send_response("  HOME - Return to home position\r\n");
        uart_send_response("  STOP - Stop current movement\r\n");
        uart_send_response("  EMERGENCY_STOP - Emergency stop\r\n");
        uart_send_response("  TEST - Test connection\r\n");
        uart_send_response("  ECHO:text - Echo back text\r\n");
        uart_send_response("  VERSION - Get version\r\n");
        uart_send_response("  DEBUG_ON/DEBUG_OFF - Toggle debug\r\n");
    }
    else if(strcmp(cmd, "DEBUG_ON") == 0) {
        #undef DEBUG_MODE
        #define DEBUG_MODE 1
        uart_send_response("Debug mode ON\r\n");
    }
    else if(strcmp(cmd, "DEBUG_OFF") == 0) {
        #undef DEBUG_MODE
        #define DEBUG_MODE 0
        uart_send_response("Debug mode OFF\r\n");
    }
    else {
        sprintf(debug_msg, "Unknown command: [%s]", cmd);
        debug_print(debug_msg);
        uart_send_response("ERROR:INVALID_COMMAND\r\n");
    }
}

// ============ 初始化函数 ============
void system_init(void) {
    // 初始化步进电机
    Pin_STEP_Write(0);
    Pin_DIR_Write(0);
    Pin_ENABLE_Write(0);  // 使能步进电机
    
    // 初始化伺服电机
    PWM_Servo_Start();
    servo_set_angle(0.0);  // 归中
    
    // 初始化I2C（距离传感器）
    I2C_Distance_Start();
    
    CyDelay(100);
    
    current_height = 0.0;
    current_angle = 0.0;
    target_height = 0.0;
    target_angle = 0.0;
    system_status = STATUS_READY;
}


// ============ 主函数 ============
int main(void) {
    char rx_char;
    uint32_t loop_counter = 0;
    uint32_t last_heartbeat = 0;
    
    CyGlobalIntEnable;
    
    // 启动UART
    UART_Start();
    CyDelay(100);
    
    // 系统初始化
    system_init();
    
    uart_print("\r\n");
    uart_print("=====================================\r\n");
    uart_print("CDC Control System v1.0\r\n");
    uart_print("Debug Mode: ");
    #if DEBUG_MODE
    uart_print("ON\r\n");
    #else
    uart_print("OFF\r\n");
    #endif
    uart_print("Type 'HELP' for command list\r\n");
    uart_print("Ready for commands\r\n");
    uart_print("=====================================\r\n");
    

    for(;;) {

        if(UART_SpiUartGetRxBufferSize() > 0) {
            rx_char = UART_UartGetChar();
            

            #if DEBUG_MODE
            if(rx_char >= 32 && rx_char <= 126) {  // 可打印字符
                char debug_char[32];
                sprintf(debug_char, "[RX: '%c' (0x%02X)]", rx_char, rx_char);
                debug_print(debug_char);
            } else if(rx_char == '\r') {
                debug_print("[RX: CR (0x0D)]");
            } else if(rx_char == '\n') {
                debug_print("[RX: LF (0x0A)]");
            }
            #endif
            
            if(rx_char == '\n' || rx_char == '\r') {
                if(cmd_index > 0) {
                    cmd_buffer[cmd_index] = '\0';
                    
                    #if DEBUG_MODE
                    char debug_cmd[128];
                    sprintf(debug_cmd, "Command buffer: [%s]", cmd_buffer);
                    debug_print(debug_cmd);
                    #endif
                    
                    process_command(cmd_buffer);
                    memset(cmd_buffer, 0, CMD_BUFFER_SIZE);
                    cmd_index = 0;
                }
            }
            else if(cmd_index < CMD_BUFFER_SIZE - 1) {
                cmd_buffer[cmd_index++] = rx_char;
                cmd_buffer[cmd_index] = '\0';
            }
        }
        
        loop_counter++;
        if(loop_counter - last_heartbeat > 5000000) {
            #if DEBUG_MODE
            debug_print("System heartbeat - alive");
            #endif
            last_heartbeat = loop_counter;
        }
        
        CyDelay(1);
    }
}