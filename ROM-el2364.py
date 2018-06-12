#!/usr/bin/python3
import serial
import hashlib
import os
import sys
import math

progname = "Arduino EEPROM Programmer for AT49F512 chips"

helpmsg='''usage: '''+sys.argv[0]+''' action [-v start_address [end_address]] [filename]

action           Can be one of write, read, or verify

                 read:   Read the contents of the chip starting at start_address
                         and ending at end_address. The output is printed to
                         stdout, or to the given file.

                 write:  Write the contents of filename to the chip, starting at
                         start_address and ending at end_address or the end of
                         the file, whichever comes first.

                 verify: Verify the contents of the chip, starting at
                         start_address and ending at end_address or, if a file
                         is given, start_address + file length (in bytes). The
                         chip contents will be compared to the file given, or
                         printed to stdout as a SHA256 checksum.

                 erase:  Erase the entire chip contents.

-v               verify after a write or read operation. This option has no
                 meaning in verify mode

start_address    Starting address for the operation. This is in hex format
                 and may optionally be preceeded by '0x'

end_address      The address at which to end the operation, optionally preceeded
                 by '0x'. The default is 0x10000 or, if a file is specified, it
                 will default to start_address + the length of the file (in
                 bytes). NOTE: The end_address is non-inclusive; the address is
                 never actually altered or read (i.e., an address range of of
                 0x0c00 to 0x0e00 will read/write addresses 0x0c00 to 0x0cff).

filename         The file to write to (read mode), read from (write mode), or
                 verify against (verify mode) the EEPROM'''

args={'mode':'','do_verify':False,'start_address':-1,'end_address':-1,'filename':''}

def print_usage_error(message):
    print(progname)
    print()
    print("ERROR: "+message+"\n")
    print(helpmsg+"\n")

if len(sys.argv) < 2:
    print(progname + "\n")
    print(helpmsg)
    sys.exit(1);

if sys.argv[1].lower()[0] == 'r':
    args['mode'] = 'read'
elif sys.argv[1].lower()[0] == 'w':
    args['mode'] = 'write'
elif sys.argv[1].lower()[0] == 'v':
    args['mode'] = 'verify'
elif sys.argv[1].lower()[0] == 'e':
    args['mode'] = 'erase'
else:
    print_usage_error("Invalid action specified. Valid actions are: 'read', 'write', 'verify', or 'erase'")
    sys.exit(1)

for i in range(2,len(sys.argv)):
    if sys.argv[i] == '-v':
        args['do_verify'] = True;
        continue
    try:
        address = int(sys.argv[i],base=16)
        if args['start_address'] == -1:
            args['start_address'] = address
            if args['start_address'] > 0xFFFF:
                args['start_address'] = 0xFFFF
                print("WARNING: Start address is past end of chip, using 0x10000 instead")
        elif args['end_address'] == -1:
            args['end_address'] = address
            if args['end_address'] > 0x10000:
                args['end_address'] = 0x10000
                print("WARNING: End address is past end of chip, using 0x10000 instead")
        if args['start_address'] != -1 and args['end_address'] != -1 and args['start_address'] > args['end_address']:
            tmp = args['start_address']
            args['start_address'] = args['end_address']
            args['end_address'] = tmp
        #elif len(args['filename']) == 0:
        #    args['filename'] = sys.argv[i]
    except ValueError:
        print("ValueError on: " + sys.argv[i])
        #It has to be the filename here
        args['filename'] = sys.argv[i]

if args['mode'] == 'write':
    if len(args['filename']) == 0:
        print_usage_error('Write mode, but no filename given')
        sys.exit(1)
    if not os.path.exists(args['filename']):
        print_usage_error('File "'+args['filename']+'" does not exist.')
        sys.exit(1)
    if not os.path.isfile(args['filename']):
        print_usage_error('File "' + args['filename'] + '" is not a file.')
        sys.exit(1)
    filesize = 0
    try:
        filesize = os.path.getsize(args['filename'])
    except:
        print_usage_error('File"' + args['filename'] + '" is not accessible.')
        sys.exit(1)
    if args['start_address'] == -1:
        args['start_address'] = 0
    if args['end_address'] == -1:
        #divide the file size by 8 to get the number of addresses needed, then subtract one since the first byte is #0
        args['end_address'] = args['start_address']+filesize
        if args['end_address'] > 0x10000:
            print("WARNING: File is larger than chip, whole file will not be written.")
            args['end_address'] = 0x10000
    if args['end_address'] > args['start_address']+filesize:
        args['end_address'] = args['start_address']+filesize

elif args['mode'] == 'read':
    if args['start_address'] == -1:
        args['start_address'] = 0
    if args['end_address'] == -1:
        args['end_address'] = 0x10000

elif args['mode'] == 'verify':
    #If file is given, check that it exists
    if len(args['filename']) > 0:
        if not os.path.exists(args['filename']):
            print_usage_error('File "'+args['filename']+'" does not exist')
            sys.exit(1)
        if not os.path.isfile(args['filename']):
            print_usage_error('File "'+args['filename']+'" is not a file')
            sys.exit(1)
        filesize = os.path.getsize(args['filename'])
        if filesize != args['end_address'] - args['start_address']:
            print_usage_error('File "'+args['filename']+'"\'s size does not match the address range given, verification WILL fail.')
    else:
        if args['end_address'] == -1:
            args['end_address'] = 0x10000

elif args['mode'] == 'erase':
    if args['start_address'] != -1 or args['end_address'] != -1:
        print_usage_error("Address-based erasure is not supported on this chip. In order to erase an address range, the whole chip must be erased.")
        sys.exit(1)

print("Args:")
for arg in args:
    if arg == 'start_address' or arg == 'end_address':
        print(arg + ": " + str(args[arg]) + " ({:04x})".format(args[arg]))
    else:
      print(arg + ": " + str(args[arg]))
print();

def init_arduino():
    print("Setting up serial.")
    ser = serial.Serial('/dev/ttyACM0', 115200, timeout=600)
    print("Waiting for Arduino...")
    while ser.in_waiting < 4:
        continue
    data = ser.read(4)
    if data != b'INIT':
        print("ERROR: Unexpected response from Arduino:")
        print(data)
        sys.exit(1)
    print("Initialized.")
    return ser

def print_progress_bar(start, end, width, count):
    end = end - 1
    message = "\r\x1b[K["
    barwidth = width - 10
    percent = count / (end - start)
    percent_per_step = 1 / (barwidth * 3)
    steps_elapsed = percent / percent_per_step
    steps_remaining = (1 - percent) / percent_per_step
    message += "#"*(math.floor(steps_elapsed/3))
    if math.floor(steps_elapsed) % 3 == 1:
        message += "-"
    elif math.floor(steps_elapsed) % 3 == 2:
        message += "="
    if steps_remaining == 0:
        message = message[:-1] + '#'
    message += ' '*(barwidth-math.ceil(math.floor(steps_elapsed)/3))
    if steps_elapsed == count:
        message += "] Done!"
    else:
        message += "]{: >-6.2f}%".format(percent*100, steps_elapsed, steps_remaining)
    print(message,end='')

def read_range(start, end, ser, dump=True, file=None):
    hash = hashlib.sha256()
    outputbuffer = b''
    buffersize = 16
    print(end - start)
    for i in range(0,end - start):
        #repeat last command, with next address in Arduino
        packet = b'\x01R'
        #Initial packet, set starting address
        if i == 0:
            packet = b'\x03R' + start.to_bytes(2,'big')
        ser.write(packet);
        packet_len = ser.read(1)
        while int(packet_len[0]) != 1:
            print(ser.read(packet_len[0]).decode(),end='')
            packet_len = ser.read(1)
        if int(packet_len[0]) == 1:
            data = ser.read(1)
            hash.update(data)
            if dump == True:
                if file != None:
                    file.write(data)
                else:
                    outputbuffer += data;
                    if len(outputbuffer) == buffersize:
                        address = (i+start) - buffersize + 1
                        address = format(address,'x')
                        output = "\r\x1b[K" + '0'*(4-len(address)) + address + '    '
                        for j in range(0, buffersize):
                            output += "%02x" % outputbuffer[j] + ' '
                            if j == 7:
                                output += '   '
                        if buffersize < 16:
                            output += '   '*(16-buffersize)
                            if buffersize < 8:
                                output += '    '
                        output += '   |'
                        for j in range(0, buffersize):
                            if int(outputbuffer[j]) < 32 or int(outputbuffer[j]) > 126:
                                output += '.'
                            else:
                              output += chr(outputbuffer[j])
                        if buffersize < 16:
                            output += ' '*(buffersize-16)
                            if buffersize < 8:
                                output += ' '
                        output += '|'
                        print(output)
                        output=''
                        outputbuffer=b''
                        if (end-start) - i < 16:
                            buffersize = end - i
            print_progress_bar(start, end, 80, i)
    print()
    hexdigest = hash.hexdigest();
    return hexdigest

if args['mode'] == 'read':
    if len(args['filename']) != 0:
        try:
            file = open(args['filename'], 'wb')
        except OSError as e:
            print('ERROR: Failed to open file "'+args['filename']+'": '+e.strerror)
            sys.exit(1)
    else:
        file = None

    ser = init_arduino()

    print("Reading " + str(args['end_address'] - args['start_address'] + 1) + " bytes")
    read_range(args['start_address'], args['end_address'], ser, True, file)

if args['mode'] == 'write':
    try:
        file = open(args['filename'],"rb")
    except OSError as e:
        print('ERROR: Failed to open file "' + args['filename'] + '": ' + e.strerror)
        sys.exit(1)

    filesize = os.path.getsize(args['filename'])
    if(filesize > args['end_address'] - args['start_address']):
        print("WARNING: file is larger than selected address range\n")

    print('Reading file...')
    #read the whole file into memory, shouldn't be any issue with 512K max
    filebuffer = file.read(args['end_address'] - args['start_address'])
    filehash = hashlib.sha256()
    filehash.update(filebuffer)
    filehexdigest = filehash.hexdigest()
    message = 'File checksum '
    if filesize > len(filebuffer):
        message += '(first {} bytes) '.format(len(filebuffer))
    print(message + 'is:\n{}'.format(filehexdigest))

    ser = init_arduino();

    print('Writing file to EEPROM...')

    for i in range(0,args['end_address'] - args['start_address']):
        packet = b'\x02W' + int(filebuffer[i]).to_bytes(1, 'big')
        if i == 0:
            packet = b'\x04\x57' + args['start_address'].to_bytes(2, 'big') + int(filebuffer[i]).to_bytes(1, 'big')
        ser.write(packet)
        packet_len = ser.read(1)
        while int(packet_len[0]) != 1:
          print(ser.readline().decode(),end='')
          packet_len = ser.read(1)
        if int(packet_len[0]) == 1:
            #Read the packet
            status = ser.read(packet_len[0])
            if status[0] == 0:
                print_progress_bar(0, args['end_address'] - args['start_address'], 80, i)
            elif status[0] == 1:
                print("\nERROR: Failed to write byte at address {:04x}!".format(i+args['start_address']))
                sys.exit(1)
            else:
                print(ser.readline().decode())
        else:
            print(ser.readline().decode())
    print()

    print('Done!')

    if args['do_verify']:
        print("Verifying EEPROM...")
        eepromhexdigest = read_range(args['start_address'], args['end_address'],ser,dump=False,file=None)
        print("EEPROM checksum:\n{}".format(eepromhexdigest))
        if eepromhexdigest == filehexdigest:
            print("Verification succeeded!")
        else:
            print("Verification failed!")


if args['mode'] == 'verify':
    ser = init_arduino();

    print("Verifying " + str(args['end_address'] - args['start_address']) + " bytes")

    sha256sum = read_range(args['start_address'], args['end_address'], ser, False)

    print("SHA256: " + sha256sum)

if args['mode'] == 'erase':
    ser = init_arduino();
    ser.write(b'\x01E');
    packet_len = ser.read(1);
    while int(packet_len[0]) != 1:
        print(ser.readline().decode(),end='')
        packet_len = ser.read(1)
    response = ser.read(packet_len[0])
    print(response[0])
