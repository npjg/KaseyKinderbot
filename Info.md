This repo contains knowledge I have gathered for dumping the ROMS on Kasey the
Kinderbot cartridges.

# Waist Pinout (CON_W)

```
0  |-| (Red)    +4.5v
1  |-| (Yellow) Clock
2  |-| (Green)  Home
3  |-| (Grey)   EN (-?)
4  |-| (Orange) Pwr (soft)
5  |-| (Black)  -4.5v
6  |-| (Brown)  EN (+?)
7  |-| (Blue)   Pwr (soft)
8  |-| (White)  Pwr (hard) ??
9  |-| (Black)  Motor (-?)
10 |-| (Red)    Motor (+?)
         CON_W
```

# ROM Pinout (Socket Side)

```
        SOCKET PCB
CON2                CON1
0  |-| +3v?         0  |-|
1  |-| +            1  |-|
2  |-| +            2  |-|
3  |-| +            3  |-|
4  |-| +            4  |-|
5  |-| +            5  |-|
6  |-| +            6  |-|
7  |-| +            7  |-|
8  |-| +            8  |-|
9  |-| +            9  |-| -3v?    
```