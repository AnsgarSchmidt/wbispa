#!/usr/bin/python

import csv
import ConfigParser
import time
import os
from twitter import *

config = ConfigParser.RawConfigParser()
config.read('wbispa.cfg')

MY_TWITTER_CREDS = os.path.expanduser('~/.twitter_oauth')

CONSUMER_KEY     = config.get('Twitter','consumer_key')
CONSUMER_SECRET  = config.get('Twitter','consumer_secret')

if not os.path.exists(MY_TWITTER_CREDS):
    oauth_dance("wbispa", CONSUMER_KEY, CONSUMER_SECRET,
        MY_TWITTER_CREDS)

oauth_token, oauth_token_secret = read_token_file(MY_TWITTER_CREDS)

twitter = Twitter(auth=OAuth(oauth_token, oauth_token_secret, CONSUMER_KEY, CONSUMER_SECRET))

lasttime       = 0
sumwattperhour = 0
firsttime      = 0
earliesttime   = int(time.time()) - int(config.get('PWMCalc','duration'))*60*60 - 60

with open(config.get('CSV','filename'), 'rb') as csvfile:
     reader = csv.reader(csvfile, delimiter=',', quotechar=',')
     for row in reader:
         #nowtime = datetime.datetime.fromtimestamp(int(row[0]))
         now = int(row[0])
         pwm = int(row[3])
         watt = 24*(float(pwm)/255.0)
         if now < earliesttime:
            continue
         if lasttime == 0:
             lasttime  = now
             firsttime = now
         timediff = now-lasttime
         sumwattperhour += watt*(float(timediff)/(60.0*60.0))
         lasttime = now
     hours = str((lasttime-firsttime)/(60*60))
     price = (sumwattperhour / 1000.0) * (float(config.get('PWMCalc','price_per_kwh')))
     string = "In the last " + hours + " hours we used " + str(round(sumwattperhour,3)) + " #watt/hour or " + str(round(price,2)) + " Eurocent for the WBI smart home floor heating system. #hacking #arduino #smarthome"
     twitter.statuses.update(status=string)
     print string


