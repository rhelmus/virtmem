#!/usr/bin/env python3

# global modules
import argparse
import queue
import serial
import sys
import threading

# local modules
import serialiohandler

# --- config ---

# Values here are used for argument parsing defaults. After parsing values are replaced.
class Config:
    serialPort = '/dev/ttyACM0'
    serialBaud = 115200
    serialInitValue = 0xFF
    serialPassDev = None
    serialPassBaud = 115200

# ---

doQuit = False
inputQueue = queue.Queue()
serPassInterface = None

def trace(frame, event, arg):
    print("{}, {}:{}".format(event, frame.f_code.co_filename, frame.f_lineno))
    return trace

def checkCommandArguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--port", help="serial device connected to the arduino. Default: %(default)s",
                        default=Config.serialPort)
    parser.add_argument("-b", "--baud", help="Serial baudrate. Default: %(default)d", type=int,
                        default=Config.serialBaud)
    parser.add_argument("-l", "--pass", help="serial pass through device",
                        default=Config.serialPassDev, dest='passdev')
    parser.add_argument("-r", "--passbaud", help="baud rate of serial port pass through device. Default: %(default)d",
                        type=int, default=Config.serialPassBaud)

    # Update configs
    args = parser.parse_args()
    Config.serialPort = args.port
    Config.serialBaud = args.baud
    Config.serialPassDev = args.passdev
    Config.serialPassBaud = args.passbaud

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
    checkCommandArguments()

    if Config.serialPassDev:
        global serPassInterface
        serPassInterface = serial.Serial(port=Config.serialPassDev, baudrate=Config.serialPassBaud, timeout=50)
        serialiohandler.connect(Config.serialPort, Config.serialBaud, Config.serialInitValue, serPassInterface)
    else:
        serialiohandler.connect(Config.serialPort, Config.serialBaud, Config.serialInitValue, sys.stdout.buffer)

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
