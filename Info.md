This repo contains knowledge I have gathered for dumping the ROMs on Kasey the
Kinderbot cartridges.


# Cart Socket PCB Layout
The Socket PCB splits the raw cart pins into two connectors (CON1 and CON2) that go to the motherboard.

```
        Socket PCB
CON2                CON1
0  |-|              0  |-|
1  |-| +            1  |-|
2  |-| +            2  |-|
3  |-| +            3  |-|
4  |-| +            4  |-|
5  |-| +            5  |-|
6  |-| +            6  |-|
7  |-| +            7  |-|
8  |-| +            8  |-|
9  |-| +            9  |-|
REV: A            02/02/28
```

## CON2 Pinout
```
        SOCKET PCB
CON2                          ->                 CON_ROM2
0  |-| (Red)    VCC (+3.3v)
1  |-| (Green)  AUDIO ACTIVE 1
2  |-| (Yellow) AUDIO ACTIVE 2
3  |-| (Grey)   C0
4  |-| (Purple) D4
5  |-| (Brown)  D3?
6  |-| (Orange) D2?
7  |-| (White)  D1?
8  |-| (Blue)   D0?
9  |-| (Black)  PWM AUDIO 2
                                              10 (Brown) Volume PCB
                                              11 (Brown) Volume PCB
                                              12 (Black) Contrast PCB
```

## CON1 Pinout
```
CON1                          ->                 CON_ROM1
0  |-| (Black)  VCC (+3.3V)
1  |-| (Red)    GND
2  |-| (Grey)   CART DETECT (low when cart present)
3  |-| (Purple) C1
4  |-| (Green)  D5?
5  |-| (Yellow) D6?
6  |-| (Brown)  D7?
7  |-| (Black)  CLK (4 MHz)
8  |-| (White)  PWM AUDIO 1
9  |-| (Blue)   GND
```

PWM audio seems to be active low, with a PWM period around 40 kHz.

# Waist Pinout (CON_W)
```
0  |-| (Red)    +4.5v
1  |-| (Yellow) Clock
2  |-| (Green)  Home
3  |-| (Grey)   EN (-?)
4  |-| (Orange) Pwr (soft)
5  |-| (Black)  GND
6  |-| (Brown)  EN (+?)
7  |-| (Blue)   Pwr (soft)
8  |-| (White)  Pwr (hard) ??
9  |-| (Black)  Motor (-?)
10 |-| (Red)    Motor (+?)
         CON_W
```

# Head Pinout (CON_H)
```
0 |-| (White) Mouth LED
1 |-| (Orange)
2 |-| (Grey)
3 |-| (Blue) PWM AUDIO
4 |-| (Brown)
5 |-| (Yellow)
6 |-| (Green)
7 |-| (Black)
8 |-| (Red)
```
