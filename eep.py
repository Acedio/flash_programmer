#!/bin/python

import serial
import argparse
import math
import sys

parser = argparse.ArgumentParser(description='Interact with the flash programmer.')
parser.add_argument('-b', default=250000, help="Set the baud rate used for communicating with the programmer. Default: 250000.")
parser.add_argument('-c', default=0x40000, help="Set the ROM size, in bytes. Default: 0x40000 (256KiB)")
parser.add_argument('-s', default="/dev/ttyUSB0", help="Set the serial port to use for communicating with the programmer. Default: /dev/ttyUSB0")
parser.add_argument('--keep_open', action='store_true', help="Keep the serial connection open even after the operation has completed. Useful for debugging.")
reqd = parser.add_mutually_exclusive_group(required=True)
reqd.add_argument('--clear', action='store_true', help="Clear the ROM, resetting all bytes to 0xFF to prepare for writing.")
reqd.add_argument('-d', help="Dump the ROM's contents to the given file.")
reqd.add_argument('-f', help="Write the given file's contents to the ROM. The ROM should be --clear'd beforehand.")
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

def printAddressProgress(address):
    """
    Outputs progress for nice round (hex) milestones.
    """
    if address % 0x1000 == 0:
        sys.stdout.write(hex(address))
    elif address % 0x100 == 0:
        sys.stdout.write('.')
        if address % 0x1000 == 0xf00:
            sys.stdout.write('\n')
    sys.stdout.flush()

ser = serial.Serial(args.s, args.b)

print('ちょっと待ってください…')
# Poke it to see if it's alive.
ser.write(b'a')
waitText(ser, b'eepeepcmderror')
print("よかった!")

if args.clear:
    ser.write(b'c')
elif args.d != None:
    f = open(args.d, 'wb')
    ser.write(b'd')
    waitText(ser,b'eepeepcmdd')
    print("Dumping flash to ", args.d)
    for i in range(math.floor(args.c/256)):
        line = ser.read(256)
        printAddressProgress(i*256)
        f.write(line)
    f.close()
elif args.f != None:
    f = open(args.f, 'rb')
    ser.write(b'w')
    waitText(ser,b'eepeepcmdw')
    print("Writing ", args.f, " to flash...")
    for i in range(args.c):
        data = f.read(1)
        ser.write(data)
        check = ser.read(1)
        printAddressProgress(i)
        while check != data:
            print('Write to ', hex(i), ' failed. Waiting for correction.')
            check = ser.read(1)
    f.close()
while args.keep_open:
    sys.stdout.write(ser.read(1).decode('utf-8'))
    sys.stdout.flush()
ser.close()
