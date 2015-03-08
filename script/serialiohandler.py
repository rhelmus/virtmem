import datetime
from collections import deque
import serial
import struct
import time

class Commands:
    init, initPool, read, write, requestInput = range(0, 5)

class State:
    initialized = False
    processState = 'idle'
    initValue, memoryPool = None, None
    inputQueue = deque()

serInterface = serial.Serial()

def readInt():
    return struct.unpack('i', serInterface.read(4))[0]

def sendCommand(cmd):
    serInterface.write(bytes([State.initValue]))
    serInterface.write(bytes([cmd]))
    print("send: ", State.initValue, "/", bytes([cmd]))

def processByte(byte, printunknown=True):
    val = ord(byte)
    if State.processState == 'gotinit':
        handleCommand(val)
        State.processState = 'idle'
    elif val == State.initValue:
        assert(State.processState == 'idle')
        print("Got init!")
        State.processState = 'gotinit'
    else:
        assert(State.processState == 'idle')
        if printunknown:
            print(chr(val), end='')

def handleCommand(command):
    print("handleCommand: ", command)

    serInterface.timeout = None # temporarily switch to blocking mode

    if command == Commands.init:
        State.initialized = True
        State.memoryPool = None # remove pool
    elif not State.initialized:
        pass
    elif command == Commands.initPool:
        State.memoryPool = bytearray(readInt())
        print("init pool:", len(State.memoryPool))
    elif command == Commands.requestInput:
        sendCommand(Commands.requestInput)
        while State.inputQueue:
            serInterface.write(State.inputQueue.pop())
    elif State.memoryPool == None:
        print("WARNING: tried to read/write unitialized memory pool")
    elif command == Commands.read:
        index, size = readInt(), readInt()
        sendCommand(Commands.read)
        serInterface.write(State.memoryPool[index:size+index])
#        print("read memPool: ", State.memoryPool[index:size+index])
        print("read memPool: ", index, size)
    elif command == Commands.write:
        index, size = readInt(), readInt()
        State.memoryPool[index:size+index] = serInterface.read(size)
#        print("write memPool: ", State.memoryPool)
        print("write memPool: ", index, size)

    serInterface.timeout = 0

def ensureConnection():
    # send init command every 0.5 second until we got a response
    while True:
        print("Sending handshake command")
        sendCommand(Commands.init)
        curtime = datetime.datetime.now()

        while (datetime.datetime.now() - curtime).total_seconds() < 0.5:
            byte = serInterface.read(1)
            while byte:
#                print("byte: ", byte)
                processByte(byte, True)
                if State.initialized:
                    return
                byte = serInterface.read(1)


def connect(port, baud, initval):
    serInterface.port = port
    serInterface.baudrate = baud
    serInterface.timeout = 0

    State.initValue = initval

    print("Waiting until port {} can be opened...\n".format(port))
    while True:
        try:
            serInterface.open()
            break
        except OSError:
            time.sleep(0.5)

    time.sleep(1) # wait to settle after open (only needed if board resets)

    print("Waiting for serial handshake...")
    ensureConnection()
    print("Connected and initialized!")

def update():
    byte = serInterface.read(1)
    while byte:
        processByte(byte)
        byte = serInterface.read(1)

def processInput(line):
    print("Processing input line:", line, end='')
    State.inputQueue.appendleft(line)
