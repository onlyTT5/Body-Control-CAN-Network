# 硬件与接线

## 1. CAN 总线

两个节点各通过一个 CAN 收发器连接到同一对 CANH/CANL，总线分析仪并联在总线上。

```text
120 Ω ── CANH/CANL ── 节点 A ── 节点 B ── CAN 分析仪 ── 120 Ω
                     └────────── GND 共地 ──────────────┘
```

- 波特率：500 kbit/s
- 帧格式：11 位标准数据帧
- 总线两端各保留一个 120 Ω 终端电阻；不要在每个模块上都重复启用终端电阻。
- 所有节点和分析仪必须共地。
- CAN 收发器的逻辑电平和供电必须与所用开发板兼容。

## 2. 节点 A 接线

| 功能 | STM32 引脚 | 方向/外设 | 备注 |
|---|---|---|---|
| CAN RX | `PB8` | CAN1_RX | CAN1 重映射 |
| CAN TX | `PB9` | CAN1_TX | CAN1 重映射 |
| OLED SCL | `PB6` | I2C1_SCL | 100 kHz |
| OLED SDA | `PB7` | I2C1_SDA | 地址 `0x3C` |
| Flash SCK | `PA5` | SPI1_SCK | SPI1 分频 8 |
| Flash MISO | `PA6` | SPI1_MISO | W25Q128 |
| Flash MOSI | `PA7` | SPI1_MOSI | W25Q128 |
| Flash CS | `PA4` | GPIO 输出 | 低有效 |
| EC11 A | `PA0` | TIM2_CH1 | 编码器模式 |
| EC11 B | `PA1` | TIM2_CH2 | 编码器模式 |
| 菜单按键 | `PA2` | GPIO 输入上拉 | 按键接 GND |
| 灯光按键 | `PB11` | GPIO 输入上拉 | 按键接 GND |
| 板载状态灯 | `PC13` | GPIO 输出 | 常见最小板为低有效 |

EC11 模块按板上丝印连接：`A → PA0`、`B → PA1`、`C/GND → GND`、`VCC → 3.3 V`。当前软件以每个机械档位 4 个计数处理，左转显示 `CCW`，右转显示 `CW`。

## 3. 节点 B 接线

| 功能 | STM32 引脚 | 方向/外设 | 备注 |
|---|---|---|---|
| CAN RX | `PB8` | CAN1_RX | CAN1 重映射 |
| CAN TX | `PB9` | CAN1_TX | CAN1 重映射 |
| 超声波 ECHO | `PA0` | TIM2_CH1 | 输入捕获 |
| 超声波 TRIG | `PA1` | GPIO 输出 | 触发脉冲 |
| 蜂鸣器 | `PB12` | GPIO 输出 | 当前实现高电平鸣叫 |
| 灯光/板载 LED | `PC13` | GPIO 输出 | 常见最小板为低有效 |

若超声波模块的 ECHO 输出为 5 V，应根据模块和 MCU 数据手册使用分压或电平转换，避免把超规格电压直接送入 MCU。蜂鸣器电流超过 GPIO 能力时，应增加三极管/MOSFET 驱动与续流/去耦措施。

## 4. CubeMX 关键配置（节点 A）

### TIM2 编码器

- Combined Channels：Encoder Mode
- Encoder Mode：TI1 and TI2
- Prescaler：0
- Counter Period：65535
- Internal Clock Division：No Division
- CH1/CH2：Rising Edge、Direct、No Prescaler、Input Filter 8
- PA0/PA1：Pull-up
- 不启用 TIM2 中断

CubeMX 某些版本把 `Division by 1` 显示为 `No Division`，两者含义相同。

### CAN1

- Prescaler：4
- BS1：13 TQ
- BS2：4 TQ
- SJW：1 TQ
- Automatic Bus-Off Management：Enable
- Automatic Retransmission：Disable（节点 A 当前策略）
- Normal Mode，PB8/PB9 重映射

### SPI1 / I2C1

- SPI1：Full Duplex Master，软件 NSS，分频 8
- I2C1：100 kHz，7 位地址
- W25Q128 和 OLED 均使用 3.3 V 逻辑

## 5. 上电前检查

1. 万用表确认电源与地没有短路。
2. 确认 CANH/CANL 未接反，所有设备共地。
3. 断电测量 CANH 与 CANL 之间约为 60 Ω，表示两个 120 Ω 终端并联。
4. 确认 W25Q128、OLED、编码器使用 3.3 V 兼容电平。
5. 首次上电先观察供电电流，再连接执行器和超声波模块。
