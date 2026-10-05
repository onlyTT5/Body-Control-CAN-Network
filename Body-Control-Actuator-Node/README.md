# 节点 B：Actuator / Sensor Node

该目录是传感器与执行节点工程，基于 STM32F103C8T6 和 STM32 标准外设库。

主要职责：

- 执行节点 A 的灯光命令并反馈实际状态
- 非阻塞超声波测距与五点中值滤波
- 带迟滞的四级泊车蜂鸣器
- 周期心跳、距离和健康状态上报
- CAN warning / passive / bus-off 检测与恢复
- 独立看门狗、复位原因上报和测试故障注入

Keil 工程：`Body-Control-Actuator-Node.uvprojx`

完整项目说明、接线和 CAN 协议请从仓库根目录的 [README](../README.md) 开始阅读。
