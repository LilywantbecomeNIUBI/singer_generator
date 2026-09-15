# 信号发生器 STM32CubeMX 配置方案 V0.1

## 1. 当前范围

STM32F407ZGTx 的板级基础配置已经生成；当前完成 M1 最小代码闭环，只驱动 AD9959 的第一个物理通道输出固定点频信号。

- 当前里程碑：M1，先让 AD9959 CH1 输出稳定点频信号。
- 预配置探索者开发板配套 TFTLCD 的 FSMC 总线、触摸 GPIO、板载按键和调试串口。
- 暂不启用 FreeRTOS、LVGL、DAC8552、VCA810、ADC、DMA、SDIO、FATFS、I2S。
- 保留 SWD 调试口。

## 2. 时钟树

探索者 STM32F407 板载 HSE 为 8 MHz，建议使用 HSE 晶振作为主 PLL 输入。

| 项目 | 配置 |
|---|---|
| HSE | Crystal/Ceramic Resonator，8 MHz |
| LSE | Crystal/Ceramic Resonator，32.768 kHz（为后续 RTC 预留） |
| PLL Source | HSE |
| PLLM | 8 |
| PLLN | 336 |
| PLLP | 2 |
| PLLQ | 4（当前工程实配；M1 不使用 USB/SDIO 48 MHz 域） |
| SYSCLK | 168 MHz |
| AHB | 168 MHz，/1 |
| APB1 | 42 MHz，/4，APB1 Timer Clock 84 MHz |
| APB2 | 84 MHz，/2，APB2 Timer Clock 168 MHz |
| 48 MHz Domain | 84 MHz（当前未启用依赖该时钟域的外设） |
| Flash Latency | 5 WS |

## 3. 系统与调试

| 外设 | 配置 |
|---|---|
| SYS Debug | Serial Wire |
| Timebase | SysTick |
| NVIC Priority Group | Group 4 |
| 应用中断 | 本轮不启用 |

这样保留 `PA13/SWDIO` 和 `PA14/SWCLK`，同时释放 JTAG 专用的 `PA15/PB3/PB4` 供普通 GPIO 或 SPI 使用。

## 4. AD9959：SPI1 与相邻控制脚

AD9959 第一阶段采用单线串行写入，只接 `SDIO0`，不接 `SDIO1~3`。`P0~P3` 暂时固定为低电平；后续做硬件调制或四线串行模式时再扩展。

选择探索者右侧扩展排针顶部相邻区域，复用板级模板已有的 `PB3/PB4/PB5` SPI1 总线：

| STM32 引脚 | CubeMX 功能/标签 | AD9959 模块端 | 初始状态 | 说明 |
|---|---|---|---|---|
| PB3 | SPI1_SCK | SCLK | SPI idle low | SPI1 时钟 |
| PB4 | SPI1_MISO | 不接 | - | 保留板载 SPI Flash 读回能力 |
| PB5 | SPI1_MOSI | SDIO0 | SPI | 只写数据线 |
| PB6 | GPIO Output / `AD9959_CS` | CS | High | 上电先取消片选 |
| PB7 | GPIO Output / `AD9959_IO_UPDATE` | I/O_UPDATE | Low | 写寄存器后再脉冲更新 |
| PA15 | GPIO Output / `AD9959_RESET` | RESET | Low | Serial Wire 模式下可复用 |
| PG15 | GPIO Output / `AD9959_PWR_DWN` | PDC | Low | 低电平保持正常工作 |
| PB14 | GPIO Output / `SPI_FLASH_CS` | 板载 Flash CS | High | AD9959 通信时必须保持板载 Flash 未选中 |

AD9959 模块还需要独立、足量的 5 V 供电（手册给出最大约 400 mA）并与 STM32 共地。模块串行 I/O 为 3.3 V 电平。

### SPI1 参数

| 参数 | 配置 |
|---|---|
| Mode | Full-Duplex Master |
| NSS | Software |
| Data Size | 8 bit |
| First Bit | MSB first |
| CPOL / CPHA | Low / 1 Edge（SPI Mode 0） |
| Baud Prescaler | /2，SCK = 84 MHz / 2 = 42 MHz |
| CRC | Disabled |
| DMA / NVIC | 本轮不启用 |

板载 SPI Flash 与 AD9959 共享 `PB3/PB4/PB5` 时，必须使用独立片选，并保证任何时刻只选中一个器件。

## 5. 探索者 TFTLCD：FSMC 16 位 8080 总线

LCD 接口由开发板 PCB 固定，不能为了排针相邻而重映射。

| LCD 信号 | STM32/FSMC 引脚 |
|---|---|
| CS | PG12 / FSMC_NE4 |
| RS | PF12 / FSMC_A6 |
| RD | PD4 / FSMC_NOE |
| WR | PD5 / FSMC_NWE |
| D0 | PD14 / FSMC_D0 |
| D1 | PD15 / FSMC_D1 |
| D2 | PD0 / FSMC_D2 |
| D3 | PD1 / FSMC_D3 |
| D4~D12 | PE7~PE15 / FSMC_D4~D12 |
| D13 | PD8 / FSMC_D13 |
| D14 | PD9 / FSMC_D14 |
| D15 | PD10 / FSMC_D15 |
| BL | PB15 / GPIO Output / `LCD_BL` |
| RST | 开发板系统复位线 |

### FSMC 参数

| 参数 | 配置 |
|---|---|
| Bank | NOR/PSRAM Bank 4（NE4） |
| Memory Type | SRAM |
| Data Width | 16 bit |
| Address/Data Multiplexing | Disabled |
| Write Operation | Enabled |
| Extended Mode | Enabled |
| Burst / Wait / Write Burst | Disabled |
| Read Address Setup | 15 HCLK（保守起步值） |
| Read Data Setup | 60 HCLK |
| Write Address Setup | 9 HCLK |
| Write Data Setup | 9 HCLK |
| Access Mode | A |

先使用保守时序点亮和读 ID，稳定后再依据实际屏控制器逐步缩短时序。`LCD_BL` 建议上电初始为 Low，LCD 初始化成功后再置 High，避免白屏闪烁。

## 6. 触摸接口预配置

探索者配套 MCU 屏的触摸底层可能是电阻式或电容式；板级模板采用以下通用 GPIO，由后续驱动根据 LCD/触摸控制器型号重新配置方向或开漏属性：

| STM32 引脚 | 标签 | 初始配置 |
|---|---|---|
| PB0 | `T_CLK` | Output Push-Pull，High Speed，初始 Low |
| PB1 | `T_PEN` | Input，Pull-up |
| PB2 | `T_MISO` | Input，Pull-up |
| PF11 | `T_MOSI` | Output Push-Pull，High Speed，初始 Low |
| PC13 | `T_CS` | Output Push-Pull，初始 High |

本轮不启用 EXTI。开始写触摸驱动前，要先读取屏幕 ID 并确认具体触摸控制器。

## 7. 板载按键、LED 与调试串口

| STM32 引脚 | 标签/外设 | 配置 |
|---|---|---|
| PE2 | `KEY2` | Input，Pull-up |
| PE3 | `KEY1` | Input，Pull-up |
| PE4 | `KEY0` | Input，Pull-up |
| PA0 | `KEY_UP` | Input，Pull-down |
| PF9 | `LED0` | Output，初始 High（熄灭） |
| PF10 | `LED1` | Output，初始 High（熄灭） |
| PA9 | USART1_TX | 115200 8N1 |
| PA10 | USART1_RX | 115200 8N1 |

USART1 使用轮询发送，不启用 DMA 或中断；用于打印上电、AD9959 初始化和错误状态。

## 8. 本轮明确不启用

- FreeRTOS、LVGL；
- DAC8552 和 VCA810 控制；
- ADC、TIM 采样、DMA；
- SDIO、FATFS、I2S；
- 以太网、USB OTG、CAN；
- AD9959 的 SDIO1~3 与 P0~P3 硬件控制。

## 9. CubeMX 生成选项

- Toolchain/IDE：CMake；
- Firmware：STM32Cube FW_F4 V1.28.3；
- Keep User Code：Enabled；
- 外设初始化生成独立 `.c/.h` 文件；
- 生成后应至少出现 `MX_GPIO_Init()`、`MX_SPI1_Init()`、`MX_USART1_UART_Init()`、`MX_FSMC_Init()`；
- 应用层初始化调用仅放入 `main.c` 的 CubeMX `USER CODE` 区域。

## 10. M1 代码与验证边界

- `bsp_ad9959.c/.h`：HAL SPI 寄存器写入、复位、I/O_UPDATE、频率/幅度/相位换算；
- `app_signal_generator.c/.h`：上电配置物理 CH0（方案中的逻辑 CH1）为 1 MHz、0 相位、满量程数字幅度；
- USART1 在 115200 8N1 下打印初始化结果；调试变量 `g_app_m1_status == APP_M1_SPI_CONFIG_SENT` 仅表示 SPI 配置已发送；
- 1 MHz 实际输出、幅度和波形质量必须在 AD9959 模块 CH0 的 SMA 端用示波器确认，软件无法闭环证明模拟输出正常。
