# 节点 A：Body Control / Diagnostic Node

该目录是控制与诊断节点工程，基于 STM32F103C8T6、STM32CubeMX 和 HAL。

主要职责：

- 灯光按键输入、命令序号与状态闭环
- EC11 + PA2 按键驱动的 OLED 五页面诊断界面
- 接收节点 B 的心跳、距离和健康状态
- DTC 状态管理、冻结帧和事件日志
- W25Q128 掉电保存
- CAN 离线检测与重新同步

Keil 工程：`MDK-ARM/Body Control Simulator Project.uvprojx`

CubeMX 配置：`Body Control Simulator Project.ioc`

完整项目说明、接线和 CAN 协议请从仓库根目录的 [README](../README.md) 开始阅读。
