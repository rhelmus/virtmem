#!/usr/bin/env python3

# global modules
import queue
import serial
import sys
import threading

# local modules
import serialiohandler

# --- config ---

class Config:
    serialPort = '/dev/ttyACM0'
    serialBaud = 115200
    serialInitValue = 0xFF
    serialPassDev = '/dev/pts/3'
    serialPassBaud = 115200

# ---

doQuit = False
inputQueue = queue.Queue()
serPassInterface = None

def trace(frame, event, arg):
    print("{}, {}:{}".format(event, frame.f_code.co_filename, frame.f_lineno))
    return trace

def updateSerial():
#    sys.settrace(trace)
    while not doQuit:
        serialiohandler.update()
        try:
            serialiohandler.processInput(inputQueue.get(False))
        except queue.Empty:
            pass

def monitorInput():
    try:
        if Config.serialPassDev:
            while True:
                line = serPassInterface.readline()
                if line:
                    inputQueue.put(line)
        else:
            for line in sys.stdin:
                inputQueue.put(bytearray(line, 'ascii'))
    except KeyboardInterrupt:
        pass

    # if we are here the user sent an EOF (e.g. ctrl+D) or aborted (e.g. ctrl+C)

def init():
    if Config.serialPassDev:
        global serPassInterface
        serPassInterface = serial.Serial(port=Config.serialPassDev, baudrate=Config.serialPassBaud, timeout=50)
        serialiohandler.connect(Config.serialPort, Config.serialBaud, Config.serialInitValue, serPassInterface)
    else:
        serialiohandler.connect(Config.serialPort, Config.serialBaud, Config.serialInitValue, sys.stdout)

    global serIOThread
    serIOThread = threading.Thread(target = updateSerial)
    serIOThread.start()

def main():
    init()
    monitorInput()
    global doQuit
    doQuit = True
    serialiohandler.quit()
    serIOThread.join()

if __name__ == "__main__":
    main()
