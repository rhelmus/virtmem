#!/usr/bin/env python3

# global modules
import queue
import sys
import threading

# local modules
import serialiohandler

# --- config ---

class Config:
    serialPort = '/dev/ttyACM0'
    serialBaud = 115200
    serialInitValue = 0xFF

# ---

doQuit = False
inputQueue = queue.Queue()

def updateSerial():
    while not doQuit:
        serialiohandler.update()
        try:
            serialiohandler.processInput(inputQueue.get(timeout=0.05))
        except queue.Empty:
            pass

def monitorInput():
    try:
        for line in sys.stdin:
            inputQueue.put(line)
    except KeyboardInterrupt:
        pass

    # if we are here the user sent an EOF (e.g. ctrl+D) or aborted (e.g. ctrl+C)

def init():
    serialiohandler.connect(Config.serialPort, Config.serialBaud, Config.serialInitValue)
    global serIOThread
    serIOThread = threading.Thread(target = updateSerial)
    serIOThread.start()

def main():
    init()
    monitorInput()
    global doQuit
    doQuit = True
    serIOThread.join()

if __name__ == "__main__":
   main()
