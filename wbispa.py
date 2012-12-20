#!/usr/bin/python

import twitter
import Queue
import serial
import threading
import datetime
import time
import csv

class ReceiveThread(threading.Thread):
  """Threaded serial receive"""
  def __init__(self, queue, ser):
    threading.Thread.__init__(self)
    self.queue = queue
    self.ser   = ser

  def run(self):
    while True:
       self.queue.put(self.ser.readline())

class SendThread(threading.Thread):
  """Threaded serial send"""
  def __init__(self, queue, ser):
    threading.Thread.__init__(self)
    self.queue = queue
    self.ser   = ser

  def run(self):
     while True:
        line = self.queue.get()
        self.ser.write(line+"\n")
        self.queue.task_done()        
 
class TasksThread(threading.Thread):
   """Threaded measurement"""
   def __init__(self, queue):
     threading.Thread.__init__(self)
     self.queue = queue
   
   def run(self):
      while True:
         time.sleep(10)
         self.queue.put("22;")
         self.queue.put("23;")
         self.queue.put("26;")
         self.queue.put("27;")

api = twitter.Api(consumer_key='M6VHvhNpXOSuFHT0dFwQ', consumer_secret='3vji5YgYXDU4sFYAf0kU7HaZwf3cMsXWQPcLYWRAY', access_token_key='995619222-I4dvj9BCTgcvBzvrfANk3ClT1ooqoAkVY0KXysQU', access_token_secret='xYQbcQcovlc16OkrFKK6794guhMfwfEVtMZFAIVA')

receiveQueue = Queue.Queue()
sendQueue    = Queue.Queue()

ser = serial.Serial('/dev/ttyUSB0', 9600)

ftemp = open('temp.csv', 'wt')
tempwriter = csv.writer(ftemp)

fpwm = open('pwm.csv', 'wt')
pwmwriter = csv.writer(fpwm)

fsolar = open('solar.csv', 'wt')
solarwriter = csv.writer(fsolar)

fflower = open('flower.csv', 'wt')
flowerwriter = csv.writer(fflower)

receive = ReceiveThread(receiveQueue,ser)
receive.setDaemon(False)
receive.start()

send = SendThread(sendQueue,ser)
send.setDaemon(False)
send.start()

tasks = TasksThread(sendQueue)
tasks.setDaemon(False)
tasks.start()

while 1:
   line = str(receiveQueue.get())
   element = line.split(',')
   command = int(element[0])
   if (command == 1):
     print "ACK----->"+line 
   if (command == 2):
     print "2"
   if (command == 3):
     print "3"
   if (command == 4):
     timestr = "20,"+time.strftime("%s", time.localtime())+";"
     sendQueue.put(timestr)
   if (command == 5):
     num = int(element[1])
     tempwriter.writerow((time.strftime("%s",time.localtime()),element[2],element[3]))
     ftemp.flush()
   if (command == 6):
     num = int(element[1])
     pwmwriter.writerow((time.strftime("%s",time.localtime()),element[2]))
     fpwm.flush()
   if (command == 7):
     print "7"
   if (command == 8):
     print "TIME----->"+line
   if (command == 9):
     solarwriter.writerow((time.strftime("%s",time.localtime()),element[1]))
     fsolar.flush() 
   if (command == 10):
     flowerwriter.writerow((time.strftime("%s",time.localtime()),element[1]))
     fflower.flush()
 
   receiveQueue.task_done()   

