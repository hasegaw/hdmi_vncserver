# HDMI over VNC

## Requirements

* PYNQ-Z1 Board
* microSD card (>= 16GB recommended)

## How to build

```
c++  -L/usr/lib -L/opt/opencv/lib -L/usr/local/lib -I/opt/opencv/include  -I/usr/local/include  hdmi_vncserver.c -lsds_lib -lvncserver -lopencv_imgproc -lopencv_core -fpermissive
```

## How to run

```
sudo bash
(root) export LD_LIBRARY_PATH=/usr/local/lib
(root) ./a.out
```
