# Установка armbian.


## Дерево устройств.

### CAN mcp2515

#### Установка необходимых библиотек
```bash
apt install can-utils
```

#### Конфигурация в дереве устройств
```bash
cat > mcp2515.dts <<EOF

EOF
```

### Расширитель входов/выходов pcf8575

#### Установка необходимых библиотек
```bash
apt install gpiod
```

#### Конфигурация в дереве устройств
```bash
cat > pcf8575.dts <<EOF
&i2c1 {
    /* ... other i2c devices ... */

    pcf8575: gpio@20 {
        compatible = "nxp,pcf8575";
        reg = <0x20>;
        gpio-controller;
        #gpio-cells = <2>;
        /* Optional: interrupt-parent = <&gpio1>; */
        /* Optional: interrupts = <17 IRQ_TYPE_LEVEL_LOW>; */
    };
};
EOF
```


### DTC компилятор.

#### Чтение текущего дерева.
```bash
dtc -I fs -O dts -o extracted.dts /proc/device-tree
```
#### Компиляция.
```bash
dtc -I dts -O dtb -o /boot/overlay-user/mcp2515.dtbo mcp2515.dts
dtc -I dts -O dtb -o /boot/overlay-user/pcf8575.dtbo pcf8575.dts
```
