# Body-Control-CAN-Network V0.8

## 目标

V0.8 在 V0.7 持久化 DTC 管理的基础上，为节点 A 增加旋转编码器和独立菜单按键，将原本只能通过 CAN 工具查询的 DTC 汇总、明细和冻结帧显示到 OLED，并支持在本机执行受保护的历史 DTC 清除。

本版本重点是建立一个非阻塞的本地诊断界面。旋转编码器、按键、OLED 菜单、超声波测距、灯光控制、蜂鸣器、CAN 周期通信和看门狗能够同时运行。

## 硬件连接

### EC11 旋转编码器模块

| 模块引脚 | 节点 A | 说明 |
| --- | --- | --- |
| `VCC` | `3.3V` | 模块供电 |
| `GND` | `GND` | 模块电源地 |
| `C` | `GND` | 编码器公共端 |
| `A` | `PA0 / TIM2_CH1` | 编码器 A 相 |
| `B` | `PA1 / TIM2_CH2` | 编码器 B 相 |

实物方向定义：

- 向右旋转：`CW`。
- 向左旋转：`CCW`。

### 独立菜单按键

普通按键连接在 `PA2` 和 `GND` 之间。`PA2` 配置为上拉输入，按下时为低电平。

原有 `PB11` 灯光控制按键保持独立，功能不变。

## CubeMX 配置

节点 A 使用 CubeMX/HAL 配置 TIM2 编码器接口，不在用户代码中直接配置 TIM2 寄存器。

| 参数 | 配置 |
| --- | --- |
| Encoder Mode | Encoder Mode TI1 and TI2 |
| Prescaler | `0` |
| Counter Period | `65535` |
| Internal Clock Division | No Division |
| CH1 / CH2 Polarity | Rising Edge |
| CH1 / CH2 IC Selection | Direct |
| CH1 / CH2 Prescaler | Division by 1 |
| CH1 / CH2 Input Filter | `8` |
| PA0 / PA1 Pull | Pull-up |
| TIM2 NVIC | 不启用 |

CubeMX 同时保存以下正式配置：

- `PA2`：`GPIO_Input`、`Pull-up`、标签 `BTN_MENU`。
- CAN：`Automatic Bus-Off Management = Enable`。
- SPI1：继续由 CubeMX/HAL 初始化，W25Q128 驱动继续使用 HAL SPI API。

## 编码器驱动

`bsp_encoder` 通过 `HAL_TIM_Encoder_Start()` 启动 TIM2，并轮询读取 16 位计数器。计数器初值设为 `0x8000`，计数差使用有符号 16 位结果处理，因此能够正确跨越 `0x0000/0xFFFF` 回绕点。

EC11 每个机械档位产生 4 个计数变化，驱动将其累计并归一化为一次应用事件：

- `BSP_ENCODER_EVENT_CLOCKWISE`
- `BSP_ENCODER_EVENT_COUNTERCLOCKWISE`

编码器采用非阻塞轮询，不使用延时，也不启用 TIM2 中断。

## 按键事件

`bsp_button` 同时管理原有灯光按键和新增菜单按键。菜单按键采用 20 ms 软件消抖，并产生三种事件：

| 操作 | 事件 | 用途 |
| --- | --- | --- |
| 短按 | `SHORT_PRESS` | 在明细页和冻结帧页选择下一条可见 DTC |
| 按住 800 ms | `LONG_PRESS` | 从非清除页面返回主页 |
| 按住 2 s | `HOLD_2S` | 在清除页面确认清除历史 DTC |

页面切换只由旋转编码器完成。主页、汇总页和清除页中的短按不执行操作，避免与旋转换页重复。

## OLED 页面

启动标题更新为：

```text
BODY CONTROL V0.8
```

OLED 共包含 5 个循环页面，向任意方向旋转均可在首尾之间回绕。

### 1. 主界面

显示泊车距离、CAN/DTC 状态、节点 A 灯光状态和节点 B 灯光回报。离开主页时，后台仍继续缓存最新状态；返回主页后立即显示最新数据。

### 2. DTC 汇总

```text
DTC SUMMARY 2/5
ACTIVE: 00
HISTORY: 02
ROTATE: PAGE
```

- `ACTIVE`：当前活动 DTC 的种类数。
- `HISTORY`：当前保存的历史 DTC 种类数，不是累计发生次数。

### 3. DTC 明细

```text
DTC DETAIL 3/5
D01 ULTRASONIC
A:0 H:1 C:00002
KEY: NEXT DTC
```

- `A`：当前是否活动。
- `H`：该 DTC 是否保存在历史中。
- `C`：该 DTC 的累计确认次数。

短按 `PA2` 可以在所有活动或历史 DTC 之间循环选择。

### 4. 冻结帧

```text
FREEZE D01 4/5
DIST:00195MM
H:00 L:00 R:04
T:00120 U:00086
```

- `DIST`：故障确认时的距离，单位 mm。
- `H`：故障确认时节点 B 的 Health Flags，与明细页的 History 含义不同。
- `L`：CAN Last Error Code。
- `R`：节点 B 复位原因。
- `T`：节点 A 故障确认时间，单位秒。
- `U`：节点 B 当时的运行时间，单位秒。

冻结帧是故障确认瞬间的快照，不是实时数据。每种 DTC 只显示最近一次确认时保存的冻结帧。

### 5. 本地 DTC 清除

```text
CLEAR DTC 5/5
ACTIVE: 00
HISTORY: 02
HOLD 2S TO CLEAR
```

只有在当前没有活动 DTC 时，按住 `PA2` 满 2 秒才会执行清除。结果显示如下：

| 显示 | 含义 |
| --- | --- |
| `CLEARING...` | 正在执行 Flash 清除 |
| `CLEAR OK` | RAM 历史、冻结帧和 Flash 记录均已清除 |
| `ACTIVE ERROR` | 仍有活动故障，拒绝清除 |
| `FLASH ERROR` | DTC Flash 存储不可用或擦除失败 |

本地清除和原有 CAN `0x115` 受保护清除命令共用同一套检查及 Flash 清除逻辑，避免两种入口行为不一致。CAN 清除协议保持兼容。

## OLED 字库补充

为完整显示 V0.8 页面文字，5×7 ASCII 字库补充了此前缺失的以下字符：

```text
J Q U X Z + - /
```

页面使用的英文大写字母、数字和诊断符号均可正常显示。

## 非阻塞与并发运行

- 编码器采用 TIM2 硬件正交解码和主循环轮询。
- PA2 按键采用基于 `HAL_GetTick()` 的状态检测，不使用 `HAL_Delay()`。
- OLED 页面只改变显示和操作入口，不暂停 CAN、测距、灯光或蜂鸣器任务。
- W25Q128 扇区擦除仍可能阻塞约 2 秒，执行前刷新看门狗；此行为与 V0.7 CAN 清除一致。

## 已完成的实物测试

- 编码器向左识别为 `CCW`、向右识别为 `CW`。
- 单步和快速连续旋转均能稳定切换页面，没有丢失方向或影响其他功能。
- 五个 OLED 页面能够双向循环切换，页面内容及新增字母和符号显示正常。
- PA2 短按、800 ms 长按和 2 s 清除确认均按预期工作。
- DTC 汇总、明细、累计次数和冻结帧能够正确显示。
- 本地清除成功后 `ACTIVE`、`HISTORY` 均为 0，重启后仍保持清空状态。
- 存在活动 DTC 时，本地清除返回 `ACTIVE ERROR`，不会删除故障信息。
- 编码器和菜单操作期间，`0x111`、`0x101`、`0x114` 周期报文保持发送。
- 快速切换页面时，超声波测距、分级蜂鸣器、PB11 灯光控制及节点 B 回报保持正常。
- 原有 `0x115/0x116/0x117/0x118` DTC 查询及清除协议保持正常。
- 节点 A、节点 B 均使用 Keil ARMCC 5.06 update 6 完整编译通过，结果为 `0 Error(s), 0 Warning(s)`。

## 正式版本配置

节点 A 的测试和故障注入开关保持关闭：

```c
#define APP_CAN_PROTOCOL_SELF_TEST_ENABLE 0U
#define APP_CAN_LOOPBACK_TEST_ENABLE      0U
#define APP_FLASH_RW_SELF_TEST_ENABLE     0U
#define APP_EVENT_LOG_BOOT_TEST_ENABLE    0U
#define APP_WATCHDOG_TEST_ENABLE          0U
```

节点 B 的超声波与五点中值滤波保持启用，看门狗故障注入保持关闭：

```c
#define ACTUATOR_ULTRASONIC_ENABLE         1U
#define ACTUATOR_WATCHDOG_TEST_ENABLE      0U
#define BSP_ULTRASONIC_MEDIAN_FILTER_SIZE  5U
```

## 已知限制

- EC11 模块没有独立按键输出，因此菜单确认使用外接 PA2 按键。
- 当前页面和选中的 DTC 不保存到 Flash，重启后从主页和 DTC 01 开始。
- DTC 明细页的 `H` 表示 History，冻结帧中的 `H` 表示 Health Flags，应结合页面标题区分。
- 每种 DTC 只保留最近一次确认时的冻结帧，不保存每次发生的完整历史。
- W25Q128 单扇区擦除期间主循环会短暂停止；清除操作只能在用户长按确认或收到受保护 CAN 命令后发生。

## V0.8 验收结论

V0.8 已完成 TIM2 硬件编码器驱动、独立菜单按键、五页 OLED 诊断界面、DTC 本地查询和受保护清除，并通过与 CAN、超声波、灯光、蜂鸣器、Flash 和看门狗功能的整机并发回归测试。

当前版本在保留 V0.7 远程诊断协议的同时，增加了无需 CAN 上位机即可操作的本地诊断入口，适合实物演示、调试和项目展示。
