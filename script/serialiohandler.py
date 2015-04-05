import datetime
import serial
import struct
import threading
import time

class Commands:
    init, initPool, read, write, inputAvailable, inputRequest, inputPeek = range(0, 7)

class State:
    initialized = False
    processState = 'idle'
    initValue, memoryPool = None, None
    inputData = bytearray()
    inputLock = threading.Lock()
    doQuit = False
    outdev = None

serInterface = serial.Serial()

def readInt():
#    """
    s = bytearray()
#    while len(s) < 4:
#        s += serInterface.read(4 - len(s))
    ind = 0
    while ind < 4:
        byte = serInterface.read(1)
        if byte:
            s += byte
            ind += 1

    print("readInt(): ", s, struct.unpack('i', s)[0])
    return struct.unpack('i', s)[0]
    """
    return struct.unpack('i', serInterface.read(4))[0]
    """

def writeInt(i):
    serInterface.write(struct.pack('i', i))

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
#        print("Got init!")
        State.processState = 'gotinit'
    else:
        assert(State.processState == 'idle')
        if printunknown:
            State.outdev.write(byte)

def handleCommand(command):
#    print("handleCommand: ", command)

    serInterface.timeout = 50 # None # temporarily switch to blocking mode

    if command == Commands.init:
        State.initialized = True
        State.memoryPool = None # remove pool
        sendCommand(Commands.init) # reply
    elif not State.initialized:
        pass
    elif command == Commands.initPool:
        State.memoryPool = bytearray(readInt())
        print("init pool:", len(State.memoryPool))
    elif command == Commands.inputAvailable:
        with State.inputLock:
            writeInt(len(State.inputData))
    elif command == Commands.inputRequest:
        with State.inputLock:
            count = min(readInt(), len(State.inputData))
            writeInt(count)
            serInterface.write(State.inputData[:count])
            del State.inputData[:count]
    elif command == Commands.inputPeek:
        if len(State.inputData) == 0:
            serInterface.write(0)
        else:
            with State.inputLock:
                serInterface.write(1)
                serInterface.write(State.inputData[0])
    elif State.memoryPool == None:
        print("WARNING: tried to read/write unitialized memory pool")
    elif command == Commands.read:
        index, size = readInt(), readInt()
        serInterface.write(State.memoryPool[index:size+index])
#        print("read memPool: ", State.memoryPool[index:size+index])
#        print("read memPool: ", index, size)
    elif command == Commands.write:
        index, size = readInt(), readInt()
        State.memoryPool[index:size+index] = serInterface.read(size)
#        print("write memPool: ", State.memoryPool)
#        print("write memPool: ", index, size)

    serInterface.timeout = 0

def handshake():
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

def ensureConnection():
    print("Waiting until port {} can be opened...\n".format(serInterface.port))
    while not State.doQuit:
        try:
            serInterface.open()
            break
        except OSError:
            time.sleep(0.5)

    if State.doQuit:
        return

    time.sleep(1) # wait to settle after open (only needed if board resets)

    print("Waiting for serial handshake...")
#    handshake()
    print("Connected and initialized!")

def connect(port, baud, initval, outdev):
    serInterface.port = port
    serInterface.baudrate = baud
    serInterface.timeout = 0

    State.initValue = initval
    State.outdev = outdev

    ensureConnection()

def update():
    global serInterface

    try:
        byte = serInterface.read(1)
        while byte:
            processByte(byte)
            byte = serInterface.read(1)
    # NOTE: catch for TypeError as workaround for indexing bug in PySerial
    except (serial.serialutil.SerialException, TypeError):
        print("Caught serial exception, port disconnected?")
        State.initialized = False
        p, b = serInterface.port, serInterface.baudrate
        serInterface.close()
        serInterface = serial.Serial()
        connect(p, b, State.initValue, State.outdev)

def processInput(line):
    print("Processing input line:", line)
    with State.inputLock:
        State.inputData += line

def quit():
    State.doQuit = True
