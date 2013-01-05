#!/usr/bin/python

import csv
import ConfigParser
import time
import os
import sys
from twitter import *

CONFIG_FILE = 'wbispa.cfg'
if len(sys.argv) == 2:
    CONFIG_FILE = sys.argv[1]
    print CONFIG_FILE

config = ConfigParser.RawConfigParser()
config.read(CONFIG_FILE)

MY_TWITTER_CREDS = os.path.expanduser('~/.twitter_oauth')
CONSUMER_KEY     = config.get('Twitter','consumer_key')
CONSUMER_SECRET  = config.get('Twitter','consumer_secret')

if not os.path.exists(MY_TWITTER_CREDS):
    oauth_dance("WBISpa", CONSUMER_KEY, CONSUMER_SECRET, MY_TWITTER_CREDS)

oauth_token, oauth_token_secret = read_token_file(MY_TWITTER_CREDS)

twitter = Twitter(auth=OAuth(oauth_token, oauth_token_secret, CONSUMER_KEY, CONSUMER_SECRET))

workingtime    = 0
sumwattperhour = 0
firsttime      = 0
earliesttime   = int(time.time()) - int(config.get('PWMCalc','duration'))*60*60 - 60

with open(config.get('CSV','filename'), 'rb') as csvfile:
     reader = csv.reader(csvfile, delimiter=',', quotechar=',')
     for row in reader:
         now = int(row[0])
         pwm = int(row[3])
         watt = float(config.get('PWMCalc','maxwatt'))*(float(pwm)/255.0)
         if now < earliesttime:
            continue
         if workingtime == 0:
             workingtime  = now
             firsttime    = now
         timediff = now - workingtime
         sumwattperhour += watt*(float(timediff)/(60.0*60.0))
         workingtime = now
     hours = str((workingtime - firsttime)/(60*60))
     price = (sumwattperhour / 1000.0) * (float(config.get('PWMCalc','price_per_kwh')))
     string = "In the last " + hours + " hours we used " + str(round(sumwattperhour,3)) + " #watt/hour or " + str(round(price,2)) + " Eurocent for the WBI smart home floor heating system. #hacking #arduino #smarthome"
     twitter.statuses.update(status=string)
