#!/usr/bin/python

import time
import usb
import logging
import os
import sys
import subprocess
import uuid
import json

def onButtonDown():
    logging.info('BIG RED BUTTON PRESSED!!')
  
def onButtonUp():
    logging.info('BIG RED BUTTON RELEASED!!')
    takePicture()

def findButton():
    for bus in usb.busses():
        for dev in bus.devices:
            if dev.idVendor == 0x1d34 and dev.idProduct == 0x000d:
                return dev

def takePicture():
    imagePath = '/tmp/image.jpg'
    try:
        os.remove(imagePath)
    except OSError:
        pass
    subprocess.call("chdkptp -ec -e\"rec\" -e\"rs %s\"" % (imagePath[:-4]), shell=True)    
    
    if not os.path.isfile(imagePath):
        logging.warn("Error during taking picture")

    with open(imagePath, "rb") as imageFile:
        imageEncoded = base64.b64encode(imageFile.read())

        upload = {
            'id': uuid.uuid4(),
            'picture': imageEncoded,
            'takenTime' : int(time.time()),
            'ride': 'cam2',
        }

        kafka = KafkaClient('192.168.6.27:9092')
        producer = SimpleProducer(kafka)      
        producer.send_messages(b'pictures', json.dumps(upload))


def main():
    logging.basicConfig(level=logging.DEBUG)
    logging.info('Started')

    dev = findButton()
    if dev is None:
        logging.info('redbutton.py executed but no Big Red Button could be found')
        print '>>> The Big Red Button could not be found. Please plug it into a USB port.'
        sys.exit()

    handle = dev.open()
    interface = dev.configurations[0].interfaces[0][0]
    endpoint = interface.endpoints[0]

    try:
        handle.detachKernelDriver(interface)
    except Exception, e:
        # It may already be unloaded.
        pass

    handle.claimInterface(interface)
    
    logging.info('Started Big Red Button listener')
    
    button_depressed = 0
    button_depressed_last = 0
    while 1:
      # USB setup packet. I think it's a USB HID SET_REPORT.
      result = handle.controlMsg(requestType=0x21, # OUT | CLASS | INTERFACE
                                request= 0x09, # SET_REPORT
                                value= 0x0200, # report type: OUTPUT
                                buffer="\x00\x00\x00\x00\x00\x00\x00\x02")
      
      try:
        result = handle.interruptRead(endpoint.address, endpoint.maxPacketSize)
        if result[0] == 22:
          button_depressed = 1
        else:
          button_depressed = 0
        
        if (button_depressed_last != button_depressed):
          if (button_depressed):
            onButtonDown()
          else:
            onButtonUp()
        
        button_depressed_last = button_depressed
        
        #print [hex(x) for x in result]
      except Exception, e:
        # Sometimes this fails. Unsure why.
        pass
      
      time.sleep(endpoint.interval * 0.001) # 10ms
    
    handle.releaseInterface(interface)
    logging.info('Finished')
        
if __name__ == '__main__':
    main()
