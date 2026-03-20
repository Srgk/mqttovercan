# Установка armbian


## Дерево устройств

### DTC компилятор

#### чтение текущего дерева
```bash
dtc -I fs -O dts -o extracted.dts /proc/device-tree
```
#### компиляция
```bash
dtc -I dts -O dtb -o /boot/overlay-user/mcp2515.dtbo mcp2515.dts
dtc -I dts -O dtb -o /boot/overlay-user/pcf8575.dtbo pcf8575.dts
```
