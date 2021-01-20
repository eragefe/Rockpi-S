#! /usr/bin/env python

from luma.core.interface.serial import i2c
from luma.core.render import canvas
from luma.oled.device import ssd1306, ssd1325, ssd1331, sh1106

serial = i2c(port=0, address=0x3C)
device = sh1106(serial, rotate=2)

device.hide()
