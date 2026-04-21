# Установка armbian.

Обрудование:
 - Orange Pi Zero 2w
 - mcp2515
 - pcf8575


## CAN `mcp2515`

#### Подключение

|Orange Pi Zero 2w|mcp2515|
|:-:|:-:|
|1|1|
|1|1|
|1|1|


#### Установка необходимых библиотек
```bash
apt install can-utils
```

#### Конфигурация в дереве устройств
```bash
cat > mcp2515.dts <<EOF
/dts-v1/;
/plugin/;

/ {
    compatible = "allwinner,sun50i-h616";

    fragment@0 {
        target-path = "/";
        __overlay__ {
            can0_osc: can0_osc {
                compatible = "fixed-clock";
                #clock-cells = <0>;
                clock-frequency = <8000000>;
            };
        };
    };

    fragment@1 {
        target = <&pio>;
        __overlay__ {
            mcp2515_irq_pin: mcp2515_irq_pin {
                pins = "PI6";
                function = "irq";
                bias-pull-up;
            };
        };
    };

    fragment@2 {
        target = <&spi1>;
        pinctrl-0 = <&spi1_pins>, <&spi1_cs0_pin>;
        __overlay__ {
            status = "okay";
            #address-cells = <1>;
            #size-cells = <0>;

            can0: mcp2515@0 {
                compatible = "microchip,mcp2515";
                reg = <0>;
                spi-max-frequency = <2000000>;
                clocks = <&can0_osc>;

                pinctrl-names = "default";
                pinctrl-0 = <&mcp2515_irq_pin>;

                interrupt-parent = <&pio>;
                interrupts = <8 6 8>;

                status = "okay";
            };
        };
    };
};
EOF
```
### Конфигурация интерфейса
```bash
sudo cat > /etc/systemd/network/80-can0.network <<EOF
[Match]
Name=can0

[CAN]
BitRate=50K
EOF
```
```bash
sudo systemctl enable systemd-networkd
sudo systemctl restart systemd-networkd
```
На ручнике
```bash
sudo ip link set can0 up type can bitrate 50000
```
## Расширитель входов/выходов `pcf8575`.

#### Подключение
|Orange Pi Zero 2w|pcf8575|
|:-:|:-:|
|3.3v|3.3v|
|PI8|SDA|
|PI7|SCL|
|GND|GND|


#### Установка необходимых библиотек.
```bash
apt install gpiod
```

#### Конфигурация в дереве устройств.
```bash
cat > pcf8575.dts <<EOF
/dts-v1/;
/plugin/;

/ {
    compatible = "allwinner,sun50i-h616";

    fragment@1 {
        target = <&i2c1>;
        __overlay__ {
            #address-cells = <1>;
            #size-cells = <0>;
            pcf8575: gpio@20 {
                compatible = "nxp,pcf8575";
                reg = <0x20>;
                gpio-controller;
                #gpio-cells = <2>;
            };
        };
    };
};
EOF
```

### DTC компилятор.

#### Чтение текущего дерева.
```bash
dtc -I fs -O dts -o extracted.dts /proc/device-tree
```
#### Компиляция оверлеев.
```bash
dtc -@ -I dts -O dtb -o /boot/overlay-user/mcp2515.dtbo mcp2515.dts
dtc -@ -I dts -O dtb -o /boot/overlay-user/pcf8575.dtbo pcf8575.dts
```
