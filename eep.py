#!/bin/python

import serial
import argparse
import math

CHIP_SIZE = 0x40000

parser = argparse.ArgumentParser(description='Interact with the eeprom.')
reqd = parser.add_mutually_exclusive_group(required=True)
reqd.add_argument('--clear', action='store_true')
reqd.add_argument('-d')
reqd.add_argument('-f')
args = parser.parse_args()

def strPreProc(text):
    skipLen = [-1, 0]

    for i in range(1,len(text)-1):
        if text[i] == text[skipLen[i]]:
            skipLen.append(skipLen[i]+1)
        else:
            skipLen.append(0)
    return skipLen

def waitText(s,text):
    skipLens = strPreProc(text)
    l = len(text)

    matched = 0

    while matched < l:
        c = ser.read()[0]
        while c != text[matched]:
            matched = skipLens[matched]
            if matched < 0:
                break
        matched += 1

ser = serial.Serial("/dev/ttyUSB0", 250000)

print('ちょっと待ってください…')
ser.write(b'a')
waitText(ser, b'eepeepcmderror')
print("よかった!")
#ser.write(b'eepeepack')

if args.clear:
    ser.write(b'c')
elif args.d != None:
    f = open(args.d, 'wb')
    ser.write(b'd')
    waitText(ser,b'eepeepcmdd')
    for i in range(math.floor(CHIP_SIZE/256)):
        line = ser.read(256)
        print('line ' + hex(i*256))
        f.write(line)
        #print(line)
    f.close()
elif args.f != None:
    f = open(args.f, 'rb')
    ser.write(b'w')
    waitText(ser,b'eepeepcmdw')
    for i in range(CHIP_SIZE):
        data = f.read(1)
        ser.write(data)
        check = ser.read(1)
        print(hex(i))
        while check != data:
            print('Failed.')
            check = ser.read(1)
while True:
    print(ser.readline(10).decode('utf-8'))
