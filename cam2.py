#!/usr/bin/python

import time
import usb
import logging
import os
import sys
import subprocess
import uuid
import json
import base64
import threading
from kafka import SimpleProducer, KafkaClient


kafka_server = '192.168.7.248:9092'

class Heartbeat(threading.Thread):
    producer = None

    def getProducer(self):
        if self.producer is None:
            kafka = KafkaClient(kafka_server)
            self.producer = SimpleProducer(kafka)
        return self.producer

    def heartbeat(self):
        data = json.dumps({
            'status': 200,
            'id': str(uuid.uuid4()),
            'serviceName': 'cam2',
        })
        self.getProducer().send_messages(b'heartbeats', data)

    def run(self):
        while True:
            self.heartbeat()
            logging.info('Sent heartbeat!!')
            time.sleep(5)
    
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

    subprocess.call(
        "chdkptp -ec -e\"rec\" -e\"rs %s\"" % (imagePath[:-4]),
        shell=True
    )

    if not os.path.isfile(imagePath):
        logging.warn("Error during taking picture")
    return

    with open(imagePath, "rb") as imageFile:
        imageEncoded = base64.b64encode(imageFile.read())

        upload = {
            'id': str(uuid.uuid4()),
            'picture': imageEncoded,
            'takenTime': int(time.time()),
            'ride': 'cam2',
        }
    data = json.dumps(upload)
    logging.info("Message size %d" % len(data))
    
    kafka = KafkaClient(kafka_server)
    producer = SimpleProducer(kafka)
    producer.send_messages(b'pictures', data)

def main():
    logging.basicConfig(level=logging.DEBUG)
    logging.info('Started')

    t = Heartbeat()
    t.start()

    dev = findButton()
    if dev is None:
        logging.info(
            'redbutton.py executed but no Big Red Button could be found'
        )
        print '>>> The Big Red Button could not be found. Please plug it in.'
        sys.exit()

    handle = dev.open()
    interface = dev.configurations[0].interfaces[0][0]
    endpoint = interface.endpoints[0]

    try:
        handle.detachKernelDriver(interface)
    except Exception:
        # It may already be unloaded.
        pass

    handle.claimInterface(interface)

    logging.info('Started Big Red Button listener')

    button_depressed = 0
    button_depressed_last = 0
    while 1:
        # USB setup packet. I think it's a USB HID SET_REPORT.
        result = handle.controlMsg(
            requestType=0x21,  # OUT | CLASS | INTERFACE
            request=0x09,      # SET_REPORT
            value=0x0200,      # report type: OUTPUT
            buffer="\x00\x00\x00\x00\x00\x00\x00\x02"
        )

        try:
            result = handle.interruptRead(endpoint.address, endpoint.maxPacketSize)
        except Exception:
            continue

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


    time.sleep(endpoint.interval * 0.001)  # 10ms
    
    handle.releaseInterface(interface)
    logging.info('Finished')

if __name__ == '__main__':
    main()
