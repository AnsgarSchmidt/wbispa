#!/usr/bin/python

import Queue
import serial
import threading
import time
import csv
import eeml
import os.path
import ConfigParser
import _mysql

class ReceiveThread(threading.Thread):
    """Threaded serial receive"""

    def __init__(self, queue, ser):
        threading.Thread.__init__(self)
        self.queue = queue
        self.ser = ser

    def run(self):
        while True:
            self.queue.put(self.ser.readline())


class SendThread(threading.Thread):
    """Threaded serial send"""

    def __init__(self, queue, ser):
        threading.Thread.__init__(self)
        self.queue = queue
        self.ser = ser

    def run(self):
        while True:
            line = self.queue.get()
            self.ser.write(line + "\n")
            self.queue.task_done()


class TasksThread(threading.Thread):
    """Threaded measurement"""

    def __init__(self, queue):
        threading.Thread.__init__(self)
        self.queue = queue

    def run(self):
        while True:
            time.sleep(30)
            self.queue.put("22;") # getTemp
            self.queue.put("23;") # getPWM
            self.queue.put("26;") # getSolar
            self.queue.put("27;") # getFlowers

config = ConfigParser.RawConfigParser()
configModifiedTime = 0

if not os.path.exists('wbispa.cfg'):
   print "Creating Config File"
   config.add_section('Hardware')
   config.set('Hardware', 'Port', '/dev/ttyACM0')
   config.set('Hardware', 'Speed', '9600')
   config.set('Hardware', 'MinPWM', '100')
   config.set('Hardware', 'MinTemp','2300')
   config.set('Hardware', 'MaxTemp', '2600')
   config.add_section('CSV')
   config.set('CSV', 'active', 'false')
   config.set('CSV', 'filename','wbispa.csv')
   config.add_section('COSM')
   config.set('COSM','active', 'false')
   config.set('COSM','API_KEY','kjdwehdwuhed7d72dhhui2hdi2hd2idh29dh29dh2ddweidwendwd')
   config.set('COSM','API_URL','/v2/feeds/12345.xml')
   config.add_section('MySQL')
   config.set('MySQL','active','false')
   config.set('MySQL','Server','127.0.0.1')
   config.set('MySQL','Database','wbispa')
   config.set('MySQL','Username','wbispa')
   config.set('MySQL','Password','blub')
   with open('wbispa.cfg', 'wb') as configfile:
      config.write(configfile)

config.read('wbispa.cfg')
configReadTime = 0

receiveQueue = Queue.Queue()
sendQueue    = Queue.Queue()

ser = serial.Serial(config.get('Hardware','Port'), config.getint('Hardware','Speed'))

receive = ReceiveThread(receiveQueue, ser)
receive.setDaemon(True)
receive.start()

send = SendThread(sendQueue, ser)
send.setDaemon(True)
send.start()

tasks = TasksThread(sendQueue)
tasks.setDaemon(True)
tasks.start()

setTempValue     = 0
currentTempValue = 0
pwmValue         = 0
solarValue       = 0
flower1Value     = 0
flower2Value     = 0

setTempUpdated     = False
currentTempUpdated = False
pwmUpdated         = False
solarUpdated       = False
flower1Updated     = False
flower2Updated     = False

while 1:
   line    = str(receiveQueue.get())
   element = line.split(',')
   command = int(element[0])
   if (command == 1):
                # Ack
                print "ACK----->" + line
   if (command == 2):
                # Ready
                print "WBISPA ready"
   if (command == 3):
                # Error
                print "ERROR--->" + line
   if (command == 4):
                # Request time
                timestr = "20," + time.strftime("%s", time.localtime()) + ";"
                sendQueue.put(timestr)
   if (command == 5):
                # Temp
                num                = int(element[1])
                setTempValue       = int(element[2])
                setTempUpdated     = True
                currentTempValue   = int(element[3])
                currentTempUpdated = True
   if (command == 6):
                # PWM
                num        = int(element[1])
                pwmValue   = int (element[2])
                pwmUpdated = True
   if (command == 7):
                # config on arduino board
                configMaxTemp = int(element[1])
                configMinTemp = int(element[2])
                configPwm     = int(element[3])
                if configMaxTemp != config.getint('Hardware','maxtemp') or configMinTemp != config.getint('Hardware','mintemp') or configPwm != config.getint('Hardware','minpwm'):
                    configstring = "24,%04d%04d%04d;" % (config.getint('Hardware','maxtemp'),config.getint('Hardware','mintemp'),config.getint('Hardware','minpwm'))
                    sendQueue.put(configstring)
   if (command == 8):
                # time on arduino board
                print "TIME----->" + line
   if (command == 9):
                # solar value
                solarValue   = int(element[1])
                solarUpdated = True
   if (command == 10):
                # flower level
                flower1Value   = int(element[1])
                flower1Updated = True
                flower2Value   = int(element[2])
                flower2Updated = True

   receiveQueue.task_done()

   if setTempUpdated == True and currentTempUpdated == True and pwmUpdated == True and solarUpdated == True and flower1Updated == True and flower2Updated == True:
       setTempUpdated     = False
       currentTempUpdated = False
       pwmUpdated         = False
       solarUpdated       = False
       flower1Updated     = False
       flower2Updated     = False
       if config.getboolean('CSV','active'):
           file      = open(config.get('CSV','filename'), 'a')
           csvwriter = csv.writer(file)
           csvwriter.writerow((time.strftime("%s",time.localtime()),float(setTempValue/100.0),float(currentTempValue/100.0),255-pwmValue,solarValue,flower1Value,flower2Value))
           file.flush()
           file.close()
       if config.getboolean('MySQL','active'):
           con = _mysql.connect('127.0.0.1', 'wbispa', 'FuEWtZs8AefmXAXS', 'wbispa')
           sql = 'INSERT INTO wbispa.measures (setTemp,currentTemp,pwm,flower1,flower2,solar)VALUES(%f,%f,%d,%d,%d,%d);' % (float(setTempValue/100.0),float(currentTempValue/100.0),255-pwmValue,flower1Value,flower2Value,solarValue)
           con.query(sql)
           con.close()
       if config.getboolean('COSM','active'):
           try:
              pac = eeml.Cosm(config.get('COSM','api_url'), config.get('COSM','api_key'))
              pac.update([eeml.Data('SetTemperature', float(setTempValue/100.0), unit=eeml.Celsius()),
                          eeml.Data('CurrentTemperature', float(currentTempValue/100.0), unit=eeml.Celsius()),
                          eeml.Data('PWM', 255-pwmValue),
                          eeml.Data('Solar', solarValue),
                          eeml.Data('FlowerWaterLevel1',flower1Value),
                          eeml.Data('FlowerWaterLevel2',flower2Value)])
              pac.put()
           except:
               print "Error talking to cosm.com:", sys.exc_info()[0]

   if os.path.getmtime('wbispa.cfg') > configReadTime:
       print "Config file changed"
       configReadTime = os.path.getmtime('wbispa.cfg')
       config.read('wbispa.cfg')
       sendQueue.put("25;")
