from flask import Flask, render_template, request
import subprocess
import os
import time

app = Flask(__name__)
app.debug = True


@app.route('/', methods = ['GET', 'POST'])
def index():
    return render_template('app.html')

@app.route('/wifi')
def wifi():
    wifi_ap_array = scan_wifi_networks()

    return render_template('wifi.html', wifi_ap_array = wifi_ap_array)

@app.route('/manual_ssid_entry')
def manual_ssid_entry():
    return render_template('manual_ssid_entry.html')

@app.route('/tidal')
def tidal():
    os.system('bash /root/net')
    return render_template('app.html')

@app.route('/tidal_save_credentials', methods = ['GET', 'POST'])
def tidal_save_credentials():
    ssid = request.form['ssid']
    wifi_key = request.form['wifi_key']
    create_upmpdcli(ssid, wifi_key)
    os.system('mv upmpdcli.tmp /etc/upmpdcli.conf')
    time.sleep(1)
    os.system('reboot')

@app.route('/save_credentials', methods = ['GET', 'POST'])
def save_credentials():
    ssid = request.form['ssid']
    wifi_key = request.form['wifi_key']
    create_wpa_supplicant(ssid, wifi_key)
    os.system('mv wifi.tmp /root/wifi')
    os.system('sed -i "$ i bash /root/startwifi" /etc/rc.local')
    os.system('bash /root/wifi')
    time.sleep(1)
    os.system('reboot')

@app.route('/dispon', methods = ['GET', 'POST'])
def dispon():
    os.system('python /root/neoplus2/oled/oled2.py &')
    return render_template('app.html')

@app.route('/dispoff', methods = ['GET', 'POST'])
def dispoff():
    pid = os.popen('pgrep -f oled').read().strip()
    os.system('pkill -f oled')
    os.system('python /root/neoplus2/oled/off.py &')
    return render_template('app.html')

@app.route('/reboot', methods = ['GET', 'POST'])
def reboot():
    time.sleep(1)
    os.system('reboot')

@app.route('/poweroff', methods = ['GET', 'POST'])
def poweroff():
    time.sleep(1)
    os.system('poweroff')

@app.route('/streamer', methods = ['GET', 'POST'])
def streamer():
    os.system('bash /root/streamer')
    return render_template('app.html')

@app.route('/optical1', methods = ['GET', 'POST'])
def optical1():
    os.system('bash /root/optical1')
    return render_template('app.html')

@app.route('/optical2', methods = ['GET', 'POST'])
def optical2():
    os.system('bash /root/optical2')
    return render_template('app.html')

@app.route('/coaxial1', methods = ['GET', 'POST'])
def coaxial1():
    os.system('bash /root/coaxial1')
    return render_template('app.html')

@app.route('/coaxial2', methods = ['GET', 'POST'])
def coaxial2():
    os.system('')
    return render_template('app.html')

@app.route('/sharp', methods = ['GET', 'POST'])
def sharp():
    os.system('i2cset -y 1 17 1 0x02')
    os.system('i2cset -y 1 17 2 0x00')
    return render_template('app.html')

@app.route('/slow', methods = ['GET', 'POST'])
def slow():
    os.system('i2cset -y 1 17 1 0x02')
    os.system('i2cset -y 1 17 2 0x01')
    return render_template('app.html')

@app.route('/sdsharp', methods = ['GET', 'POST'])
def sdsharp():
    os.system('i2cset -y 1 17 1 0x22')
    os.system('i2cset -y 1 17 2 0x00')
    return render_template('app.html')

@app.route('/sdslow', methods = ['GET', 'POST'])
def sdslow():
    os.system('i2cset -y 1 17 1 0x22')
    os.system('i2cset -y 1 17 2 0x01')
    return render_template('app.html')

@app.route('/superslow', methods = ['GET', 'POST'])
def superslow():
    os.system('i2cset -y 1 17 5 0x01')
    return render_template('app.html')

@app.route('/s1', methods = ['GET', 'POST'])
def s1():
    os.system('i2cset -y 1 17 8 0x00')
    return render_template('app.html')

@app.route('/s2', methods = ['GET', 'POST'])
def s2():
    os.system('i2cset -y 1 17 8 0x01')
    return render_template('app.html')

@app.route('/s3', methods = ['GET', 'POST'])
def s3():
    os.system('i2cset -y 1 17 8 0x02')
    return render_template('app.html')

@app.route('/s4', methods = ['GET', 'POST'])
def s4():
    os.system('i2cset -y 1 17 8 0x03')
    return render_template('app.html')

@app.route('/s5', methods = ['GET', 'POST'])
def s5():
    os.system('i2cset -y 1 17 8 0x04')
    return render_template('app.html')

######## FUNCTIONS ##########

def scan_wifi_networks():
    iwlist_raw = subprocess.Popen(['iwlist', 'scan'], stdout=subprocess.PIPE)
    ap_list, err = iwlist_raw.communicate()
    ap_array = []

    for line in ap_list.decode('utf-8').rsplit('\n'):
        if 'ESSID' in line:
            ap_ssid = line[27:-1]
            if ap_ssid != '':
                ap_array.append(ap_ssid)

    return ap_array

def create_wpa_supplicant(ssid, wifi_key):

    temp_conf_file = open('wifi.tmp', 'w')

    temp_conf_file.write('#!/bin/bash\n')
    temp_conf_file.write('\n')
    temp_conf_file.write('nmcli r wifi on\n')
    temp_conf_file.write('nmcli d wifi connect ' + ssid + '  password  ' + wifi_key + '\n')
    temp_conf_file.close

def create_upmpdcli(ssid, wifi_key):

    temp_conf_file = open('upmpdcli.tmp', 'w')

    temp_conf_file.write('uprclautostart = 1\n')
    temp_conf_file.write('friendlyname = GDis-NP\n')
    temp_conf_file.write('msfriendlyname = GDis-Tidal-server\n')
    temp_conf_file.write('tidaluser = ' + ssid + '\n')
    temp_conf_file.write('tidalpass = ' + wifi_key + '\n')
    temp_conf_file.write('tidalquality = lossless\n')
    temp_conf_file.close

if __name__ == '__main__':
    app.run(host = '0.0.0.0', port = 80)
