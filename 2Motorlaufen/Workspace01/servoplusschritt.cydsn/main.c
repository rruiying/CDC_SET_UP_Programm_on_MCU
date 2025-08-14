#include "project.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============ 步进电机参数 ============
#define STEPS_PER_MM    100     // 200步/转 ÷ 2mm/转 = 100步/mm
#define STEPPER_DELAY   2       // 步进脉冲延时(ms)

// ============ 伺服电机参数 ============
#define SERVO_PERIOD    20000   // 20ms周期 (50Hz)
#define SERVO_MIN_PULSE 500     // 0.5ms最小脉宽 (0度)
#define SERVO_MAX_PULSE 2500    // 2.5ms最大脉宽 (180度)
#define SERVO_CENTER    1500    // 1.5ms中心脉宽 (90度)

// ============ 系统状态 ============
typedef enum {
    MODE_IDLE,
    MODE_STEPPER,
    MODE_SERVO,
    MODE_DEMO
} SystemMode;

// ============ 全局变量 ============
// 系统模式
SystemMode current_mode = MODE_IDLE;

// 步进电机状态
int32_t stepper_position = 0;  // 当前位置（步数）

// 伺服电机状态
uint16 servo_angle = 90;       // 当前角度
uint16 servo_pulse = SERVO_CENTER;  // 当前脉宽
uint8 servo_sweep_active = 0;  // 自动扫描标志

// ============ 通用函数 ============
void uart_print(const char* str) {
    UART_UartPutString(str);
}

void uart_print_number(int32_t num) {
    char buffer[20];
    sprintf(buffer, "%ld", (long)num);
    uart_print(buffer);
}

char uart_get_char(void) {
    uint32 c = UART_UartGetChar();
    return (c == 0) ? 0 : (char)(c & 0xFF);
}

void delay_ms(uint32 ms) {
    CyDelay(ms);
}

// 带命令检查的延时
uint8 delay_with_check(uint32 ms) {
    uint32 count = ms / 10;
    for(uint32 i = 0; i < count; i++) {
        if(uart_get_char() != 0) {
            return 0;  // 有按键，提前退出
        }
        delay_ms(10);
    }
    return 1;  // 正常完成
}

// ============ 步进电机控制函数 ============
void stepper_move_mm(int32_t distance_mm) {
    int32_t steps = distance_mm * STEPS_PER_MM;
    uint8_t dir = (steps > 0) ? 1 : 0;
    int32_t abs_steps = (steps > 0) ? steps : -steps;
    
    // 设置方向
    Pin_DIR_Write(dir);
    delay_ms(10);
    
    // 显示信息
    uart_print("STEPPER: Moving ");
    uart_print_number(distance_mm);
    uart_print(" mm (");
    uart_print_number(steps);
    uart_print(" steps)\r\n");
    
    // 执行移动
    for(int32_t i = 0; i < abs_steps; i++) {
        Pin_STEP_Write(1);
        delay_ms(STEPPER_DELAY);
        Pin_STEP_Write(0);
        delay_ms(STEPPER_DELAY);
        
        // 检查是否有中断命令
        if(i % 100 == 0 && uart_get_char() != 0) {
            uart_print("STEPPER: Movement interrupted\r\n");
            stepper_position += (dir ? i : -i);
            return;
        }
    }
    
    // 更新位置
    stepper_position += steps;
    uart_print("STEPPER: Done! Position: ");
    uart_print_number(stepper_position / STEPS_PER_MM);
    uart_print(" mm\r\n");
}

void stepper_move_to(int32_t position_mm) {
    int32_t target_steps = position_mm * STEPS_PER_MM;
    int32_t delta = target_steps - stepper_position;
    
    if(delta != 0) {
        stepper_move_mm(delta / STEPS_PER_MM);
    } else {
        uart_print("STEPPER: Already at position\r\n");
    }
}

void stepper_home(void) {
    uart_print("STEPPER: Homing...\r\n");
    stepper_move_to(0);
    stepper_position = 0;  // 重置位置
    uart_print("STEPPER: Home position set\r\n");
}

// ============ 伺服电机控制函数 ============
void servo_set_angle(uint16 angle) {
    uint16 pulse_width;
    
    // 限制角度范围
    if(angle > 180) angle = 180;
    
    // 计算脉宽
    pulse_width = SERVO_MIN_PULSE + (angle * (SERVO_MAX_PULSE - SERVO_MIN_PULSE)) / 180;
    
    // 设置PWM比较值
    PWM_Servo_WriteCompare(pulse_width);
    
    // 更新全局变量
    servo_angle = angle;
    servo_pulse = pulse_width;
    
    // 显示信息
    uart_print("SERVO: Angle: ");
    uart_print_number(angle);
    uart_print("°, Pulse: ");
    uart_print_number(pulse_width);
    uart_print("us\r\n");
}

void servo_sweep(void) {
    uint16 angle;
    
    uart_print("SERVO: Starting sweep...\r\n");
    servo_sweep_active = 1;
    
    // 0到180度扫描
    for(angle = 0; angle <= 180 && servo_sweep_active; angle += 30) {
        servo_set_angle(angle);
        if(!delay_with_check(1000)) {
            servo_sweep_active = 0;
            uart_print("SERVO: Sweep stopped\r\n");
            return;
        }
    }
    
    // 180到0度扫描
    for(angle = 180; angle > 0 && servo_sweep_active; angle -= 30) {
        servo_set_angle(angle);
        if(!delay_with_check(1000)) {
            servo_sweep_active = 0;
            uart_print("SERVO: Sweep stopped\r\n");
            return;
        }
    }
    
    // 返回中心
    if(servo_sweep_active) {
        servo_set_angle(90);
        uart_print("SERVO: Sweep complete, centered\r\n");
    }
    servo_sweep_active = 0;
}

// ============ 综合演示函数 ============
void run_demo(void) {
    uart_print("\r\n=== STARTING DEMO MODE ===\r\n");
    uart_print("Press any key to stop...\r\n\r\n");
    
    // 步进电机演示
    uart_print("--- Stepper Motor Demo ---\r\n");
    stepper_move_mm(30);
    if(!delay_with_check(1000)) return;
    stepper_move_mm(-30);
    if(!delay_with_check(1000)) return;
    
    // 伺服电机演示
    uart_print("\r\n--- Servo Motor Demo ---\r\n");
    servo_set_angle(0);
    if(!delay_with_check(1000)) return;
    servo_set_angle(90);
    if(!delay_with_check(1000)) return;
    servo_set_angle(180);
    if(!delay_with_check(1000)) return;
    servo_set_angle(90);
    if(!delay_with_check(1000)) return;
    
    // 协同动作演示
    uart_print("\r\n--- Coordinated Demo ---\r\n");
    for(int i = 0; i < 3; i++) {
        uart_print("Cycle ");
        uart_print_number(i + 1);
        uart_print("\r\n");
        
        stepper_move_mm(20);
        servo_set_angle(45);
        if(!delay_with_check(500)) return;
        
        stepper_move_mm(-20);
        servo_set_angle(135);
        if(!delay_with_check(500)) return;
    }
    
    // 复位
    uart_print("\r\n--- Resetting to home ---\r\n");
    stepper_home();
    servo_set_angle(90);
    
    uart_print("\r\n=== DEMO COMPLETE ===\r\n");
}

// ============ 命令处理函数 ============
void show_help(void) {
    uart_print("\r\n========== MOTOR CONTROL SYSTEM ==========\r\n");
    uart_print("GENERAL COMMANDS:\r\n");
    uart_print("  M1  - Switch to Stepper mode\r\n");
    uart_print("  M2  - Switch to Servo mode\r\n");
    uart_print("  D   - Run demo (both motors)\r\n");
    uart_print("  S   - Show status\r\n");
    uart_print("  H/? - Show this help\r\n");
    uart_print("\r\nSTEPPER COMMANDS (Mode 1):\r\n");
    uart_print("  1   - Move up/down 50mm\r\n");
    uart_print("  2   - Go home (0mm)\r\n");
    uart_print("  3   - Go to 50mm\r\n");
    uart_print("  4   - Continuous test\r\n");
    uart_print("  U   - Move up 10mm\r\n");
    uart_print("  J   - Move down 10mm\r\n");
    uart_print("\r\nSERVO COMMANDS (Mode 2):\r\n");
    uart_print("  A<angle> - Set angle (0-180)\r\n");
    uart_print("  W   - Start sweep\r\n");
    uart_print("  P   - Stop sweep\r\n");
    uart_print("  0   - Go to 0°\r\n");
    uart_print("  5   - Go to 90°\r\n");
    uart_print("  9   - Go to 180°\r\n");
    uart_print("==========================================\r\n\r\n");
}

void show_status(void) {
    uart_print("\r\n=== SYSTEM STATUS ===\r\n");
    
    uart_print("Mode: ");
    switch(current_mode) {
        case MODE_IDLE:    uart_print("IDLE"); break;
        case MODE_STEPPER: uart_print("STEPPER"); break;
        case MODE_SERVO:   uart_print("SERVO"); break;
        case MODE_DEMO:    uart_print("DEMO"); break;
    }
    uart_print("\r\n");
    
    uart_print("Stepper Position: ");
    uart_print_number(stepper_position / STEPS_PER_MM);
    uart_print(" mm (");
    uart_print_number(stepper_position);
    uart_print(" steps)\r\n");
    
    uart_print("Servo Angle: ");
    uart_print_number(servo_angle);
    uart_print("° (pulse: ");
    uart_print_number(servo_pulse);
    uart_print(" us)\r\n");
    
    uart_print("====================\r\n\r\n");
}

void process_command(char cmd) {
    static char angle_buffer[10];
    static uint8 angle_index = 0;
    
    // 处理角度输入（A命令）
    if(current_mode == MODE_SERVO && angle_index > 0) {
        if(cmd >= '0' && cmd <= '9') {
            angle_buffer[angle_index++] = cmd;
            if(angle_index >= 3) {  // 最多3位数
                angle_buffer[angle_index] = '\0';
                uint16 angle = atoi(angle_buffer);
                servo_set_angle(angle);
                angle_index = 0;
            }
            return;
        } else {
            // 结束角度输入
            angle_buffer[angle_index] = '\0';
            uint16 angle = atoi(angle_buffer);
            servo_set_angle(angle);
            angle_index = 0;
        }
    }
    
    // 通用命令
    switch(cmd) {
        case 'M':
        case 'm':
            uart_print("Select mode: 1=Stepper, 2=Servo: ");
            // 等待下一个字符
            while(1) {
                cmd = uart_get_char();
                if(cmd != 0) {
                    UART_UartPutChar(cmd);  // 回显
                    uart_print("\r\n");
                    if(cmd == '1') {
                        current_mode = MODE_STEPPER;
                        uart_print("Switched to STEPPER mode\r\n");
                        break;
                    } else if(cmd == '2') {
                        current_mode = MODE_SERVO;
                        uart_print("Switched to SERVO mode\r\n");
                        break;
                    } else {
                        uart_print("Invalid mode. Use M1 or M2\r\n");
                        break;
                    }
                }
                delay_ms(10);
            }
            break;
        
        case '1':
            if(current_mode == MODE_STEPPER) {
                uart_print("Test: Up/Down 50mm\r\n");
                stepper_move_mm(50);
                delay_ms(1000);
                stepper_move_mm(-50);
            } else {
                current_mode = MODE_STEPPER;
                uart_print("Auto-switched to STEPPER mode\r\n");
                uart_print("Test: Up/Down 50mm\r\n");
                stepper_move_mm(50);
                delay_ms(1000);
                stepper_move_mm(-50);
            }
            break;
            
        case '2':
            if(current_mode == MODE_STEPPER) {
                stepper_home();
            } else {
                current_mode = MODE_SERVO;
                uart_print("Auto-switched to SERVO mode\r\n");
            }
            break;
            
        case 'D':
        case 'd':
            current_mode = MODE_DEMO;
            run_demo();
            current_mode = MODE_IDLE;
            break;
            
        case 'S':
        case 's':
            show_status();
            break;
            
        case 'H':
        case 'h':
        case '?':
            show_help();
            break;
            
        case '3':
            if(current_mode == MODE_STEPPER) {
                stepper_move_to(50);
            } else {
                uart_print("Command '3' is for STEPPER mode. Current mode: ");
                switch(current_mode) {
                    case MODE_IDLE: uart_print("IDLE"); break;
                    case MODE_SERVO: uart_print("SERVO"); break;
                    default: break;
                }
                uart_print("\r\n");
            }
            break;
            
        case '4':
            if(current_mode == MODE_STEPPER) {
                uart_print("Continuous test (press any key to stop)\r\n");
                while(uart_get_char() == 0) {
                    stepper_move_mm(30);
                    delay_ms(500);
                    stepper_move_mm(-30);
                    delay_ms(500);
                }
                uart_print("Stopped\r\n");
            } else {
                uart_print("Command '4' is for STEPPER mode. Use M1 first.\r\n");
            }
            break;
            
        case 'U':
        case 'u':
            if(current_mode == MODE_STEPPER) {
                stepper_move_mm(10);
            } else {
                uart_print("Command 'U' is for STEPPER mode. Use M1 first.\r\n");
            }
            break;
            
        case 'J':
        case 'j':
            if(current_mode == MODE_STEPPER) {
                stepper_move_mm(-10);
            } else {
                uart_print("Command 'J' is for STEPPER mode. Use M1 first.\r\n");
            }
            break;
            
        // 伺服电机特定命令
        case 'A':
        case 'a':
            if(current_mode == MODE_SERVO) {
                uart_print("Enter angle (0-180): ");
                angle_index = 0;
            } else {
                uart_print("Command 'A' is for SERVO mode. Use M2 first.\r\n");
            }
            break;
            
        case 'W':
        case 'w':
            if(current_mode == MODE_SERVO) {
                servo_sweep();
            } else {
                uart_print("Command 'W' is for SERVO mode. Use M2 first.\r\n");
            }
            break;
            
        case 'P':
        case 'p':
            if(current_mode == MODE_SERVO) {
                servo_sweep_active = 0;
                uart_print("Sweep stopped\r\n");
            } else {
                uart_print("Command 'P' is for SERVO mode. Use M2 first.\r\n");
            }
            break;
            
        case '0':
            if(current_mode == MODE_SERVO) {
                servo_set_angle(0);
            } else {
                uart_print("Command '0' is for SERVO mode. Use M2 first.\r\n");
            }
            break;
            
        case '5':
            if(current_mode == MODE_SERVO) {
                servo_set_angle(90);
            } else {
                uart_print("Command '5' is for SERVO mode. Use M2 first.\r\n");
            }
            break;
            
        case '9':
            if(current_mode == MODE_SERVO) {
                servo_set_angle(180);
            } else {
                uart_print("Command '9' is for SERVO mode. Use M2 first.\r\n");
            }
            break;
            
        default:
            uart_print("Unknown command. Type 'H' for help.\r\n");
            break;
    }
}

// ============ 主函数 ============
int main(void) {
    CyGlobalIntEnable;  // 使能全局中断
    
    // 初始化UART
    UART_Start();
    delay_ms(100);
    
    // 初始化步进电机引脚
    Pin_STEP_Write(0);
    Pin_DIR_Write(0);
    Pin_ENABLE_Write(0);  // 使能步进电机
    
    // 初始化PWM（伺服）
    PWM_Servo_Start();
    servo_set_angle(90);  // 伺服归中
    
    // 显示启动信息
    uart_print("\r\n");
    uart_print("*******************************************\r\n");
    uart_print("*     PSoC 4500S MOTOR CONTROL SYSTEM    *\r\n");
    uart_print("*         Stepper + Servo Control        *\r\n");
    uart_print("*******************************************\r\n");
    uart_print("Version 1.0 - Integrated Test System\r\n");
    uart_print("\r\n");
    uart_print("Initialization complete!\r\n");
    uart_print("  - Stepper motor: READY (at 0mm)\r\n");
    uart_print("  - Servo motor: READY (at 90°)\r\n");
    uart_print("\r\n");
    uart_print("Type 'H' for help, 'D' for demo\r\n");
    uart_print("> ");
    
    // 主循环
    char cmd;
    for(;;) {
        // 检查UART命令
        cmd = uart_get_char();
        if(cmd != 0) {
            // 回显字符
            UART_UartPutChar(cmd);
            
            if(cmd == '\r' || cmd == '\n') {
                uart_print("\r\n> ");
            } else {
                uart_print("\r\n");
                process_command(cmd);
                uart_print("> ");
            }
        }
        
        delay_ms(10);
    }
}