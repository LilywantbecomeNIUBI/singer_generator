# LVGL 9.5.0 与 ATK-MD0280 TFTLCD 移植说明

## 1. 当前实现范围

- LVGL 9.5.0 裸机移植，使用项目根目录 `lv_conf.h`；
- 正点原子 ATK-MD0280 2.8 寸屏，横屏分辨率 `320 x 240`；
- FSMC Bank 4、16 位 8080 并口、RGB565；
- 自动读取并识别 `ILI9341` 或 `ST7789`；
- LVGL 单缓冲、局部刷新、同步 FSMC 写屏；
- HR2046 电阻触摸屏模拟 SPI 读取；
- KEY0、KEY1、KEY2、KEY_UP 轮询消抖并注册为 LVGL keypad；
- 仅创建移植验收页，正式 UI、菜单和参数编辑后续实现。

本次没有修改 `bsp_ad9959.c/.h`、`app_signal_generator.c/.h`、SPI1 配置或
AD9959 的 GPIO 模拟时序。

## 2. 初始化与运行入口

`main.c` 仍由 CubeMX 管理，仅在 `USER CODE` 区增加：

```c
(void)App_HMI_Init();
```

主循环增加：

```c
App_HMI_Process();
```

屏幕缺失、ID 读取失败或型号不支持时，HMI 初始化会返回失败并通过 USART1
打印错误，但不会更改 AD9959 已建立的输出状态。

## 3. LVGL 配置与内存

- `LV_COLOR_DEPTH = 16`，显示格式为 `LV_COLOR_FORMAT_RGB565`；
- `LV_USE_OS = LV_OS_NONE`，不引入 FreeRTOS；
- LVGL 内部堆为 32 KiB；
- 软件绘制临时层目标为 8 KiB；
- 显示缓冲为 `320 x 20 x 2 = 12,800` 字节，使用单缓冲；
- LVGL tick 直接读取 `HAL_GetTick()`，不修改 SysTick 中断；
- `lv_timer_handler()` 每 5 ms 至多调用一次。

当前同步刷屏不需要 DMA 或中断。以后若启用双缓冲和 DMA，必须先在 CubeMX
统一配置 DMA 与 NVIC，再调整 flush 完成时机。

## 4. LCD 总线与控制器

当前 `.ioc` 使用 FSMC NOR/PSRAM Bank 4、16 位数据宽度，`PF12/FSMC_A6`
作为 RS。对应内存映射：

| 用途 | 地址 |
|---|---:|
| LCD 命令 | `0x6C00007E` |
| LCD 数据 | `0x6C000080` |

驱动依次尝试：

1. 通过命令 `0xD3` 读取 `ILI9341` ID `0x9341`；
2. 通过命令 `0x04` 读取 ST7789 模块返回的 `0x8552` 并归一化为 `0x7789`；
3. 识别成功后写入相应初始化序列，设置横屏扫描方向并清黑屏；
4. 最后拉高 `LCD_BL`，避免初始化阶段白屏闪烁。

目前沿用 CubeMX 中偏保守的 FSMC 时序。实际屏幕稳定后可以统一缩短写时序，
但不应在 HMI 应用文件中直接修改生成的 `fsmc.c`。

## 5. 触摸与按键

ATK-MD0280 使用 HR2046 单点电阻触摸。当前使用以下已有 GPIO：

| 信号 | GPIO |
|---|---|
| T_CLK | PB0 |
| T_PEN | PB1，低电平表示按下 |
| T_MISO | PB2 |
| T_MOSI | PF11 |
| T_CS | PC13，低有效 |

驱动对 X、Y 各采样 5 次并取中值。默认原始范围为 200～3900，横屏转换采用
`swap_xy=1, invert_x=0, invert_y=1`。这组数值只用于首次联调，实际板卡必须
通过四点校准后调用 `BSP_Touch_SetCalibration()` 更新。

按键默认映射：

| 按键 | LVGL 键值 |
|---|---|
| KEY0 | `LV_KEY_ESC` |
| KEY1 | `LV_KEY_ENTER` |
| KEY2 | `LV_KEY_NEXT` |
| KEY_UP | `LV_KEY_PREV` |

输入端口会创建默认 `lv_group_t` 并绑定 keypad；后续创建可聚焦控件时可直接
沿用该默认 group，无需重复注册按键设备。

## 6. 上板联调顺序

1. 编译并烧录，串口使用 115200 8N1；
2. 确认串口出现 `HMI ready: LVGL 9.5.0` 和识别到的控制器名称；
3. 屏幕应显示深色背景、`LVGL display ready` 以及分辨率信息；
4. 若串口提示 LCD ID 不可读，先检查排线方向、供电、FSMC Bank/RS 引脚和读时序；
5. 显示正常后再读取触摸四角原始值，更新校准参数；
6. 最后进入菜单、参数编辑与信号应用 API 的联调。

软件构建只能验证代码、链接与内存占用，不能替代屏幕 ID、颜色方向、触摸坐标
和实际刷新效果的板上验证。

## 7. 构建验证

在工程根目录使用 STM32 扩展自带的 Arm GCC 工具链执行：

```powershell
cmake --preset Debug
cmake --build --preset Debug --parallel 8
arm-none-eabi-size build/Debug/signal_generator.elf
```

本次提交前的 Debug 固件已完成编译和链接；生成文件位于
`build/Debug/signal_generator.elf`。`build/` 是本地构建产物，不纳入提交。
