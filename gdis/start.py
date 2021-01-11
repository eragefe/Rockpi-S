import os

eth = os.popen('ip addr show eth0 | grep "\<inet\>" | awk \'{ print $2 }\' | awk -F "/" \'{ print $1 }\'').read().strip()
wlan = os.popen('ip addr show wlan0 | grep "\<inet\>" | awk \'{ print $2 }\' | awk -F "/" \'{ print $1 }\'').read().strip()

if eth != "":
   os.system('aplay /root/ethernet.wav')

if wlan != "":
   os.system('aplay /root/wifi.wav')
