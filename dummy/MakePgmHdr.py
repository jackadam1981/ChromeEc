#!/usr/bin/python

#from __future__ import with_statement
import sys
import binascii
import os
import serial
import serial.tools.list_ports
import time
import struct
import array

start_add   = 0x0
data_length = 0x80000
operation = 1 #Default operation is WR
prompt_text = ''
menu_select = 1

newBuf=[]


def utility():

    try:
        #Copy PgmHdrFile.bin to Update_PgmHdrFile.bin
        read_bin_file = open('PgmHdrFile.bin', 'rb')
    except:
        print("Cant' open ", read_bin_file,": Please check the file\n")
        os._exit(1)

    try:
        write_bin_file = open('update_pgmhdrfile.bin', 'wb')
    except:
        print("Cant' open ", read_bin_file,": Please check the file\n")
        os._exit(1)

    bstrings = read_bin_file.readlines()
    write_bin_file.writelines(bstrings)

    read_bin_file.close() 

    #Update SPI_UTILITY_COMMAND
    if (operation == 1):  #Program
        byte6 = 0x01
    elif (operation == 2): #Read
        byte6 = 0x08
    elif (operation == 3): #Erase
        byte6 = 0x02
    elif (operation == 4): #Partial Erase
        byte6 = 0x10
    elif (operation == 5): #Verify
        byte6 = 0x04

    write_bin_file.seek(0x06, 0)
    newBuf = (byte6).to_bytes(1, 'big')
    write_bin_file.write(newBuf)

    #UPDATE FLASH_START_ADDRESS
    write_bin_file.seek(0x08, 0)
    newBuf = start_add.to_bytes(4, 'big')
    write_bin_file.write(newBuf)

    #UPDATE DATA_LENGTH
    write_bin_file.seek(0x0C, 0)
    newBuf = data_length.to_bytes(4, 'big')
    write_bin_file.write(newBuf)





def main():

    global start_add, data_length, operation, prompt_text, menu_select
    selections = [1, 2, 0]

    print ('*****************************************************************************')
    print ('** MakePgmHdr Utility - Verion 0.1 - 11/5/21')
    print ('*****************************************************************************')
    print ('** This utility allows you to configure the SPI operation needed and')
    print ('** generate Update_PgmHdrFile.bin header for use ')
    print ('** with CrisisRcvryIntSPIFlashUtil.py')
    print ('** ')
    print ('** Step 1 - Make changes to current setup for the operations desired')
    print ('** Step 2 - Generate Update_PgmHdrFile.bin')
    print ('***************************************************************************\n')
    
    #while (menu_select != 0):  
    for menu_select in selections:
        if (operation != 3):     
            print ('\n-------------------------------- Current Setup --------------------------------')
#            print ('-- Operation=', operation, '(1=WR, 2=RD, 3=Erase Entire Flash, 4=Partial Erase, 5=Verify)')
            print ('-- Operation=', operation, '(1=WR, 2=RD, 3=Erase)')
            print ('-- Address Offset=', hex(start_add))
            print ('-- Data Length in Bytes=', hex(data_length))
            print ('-------------------------------------------------------------------------------\n')
        else:
            print ('\n-------------- Current Setup -------------')
            print ('-- Erase entire flash')
            print ('------------------------------------------\n')

        print ('*******************************************************************************')
        print ('** Enter 1 to make changes to current setup')
        print ('** Enter 2 to run utility to generate Update_PgmHdrFile.bin with current setup')
        print ('** Enter 0 to exit')
        print ('*******************************************************************************')
        #prompt_text = input ('>')
        #menu_select = int(prompt_text)
        
        if (menu_select==1):
            print ('\n-- Select from following options --') 
#            prompt_text = input ('-- Operation: 1=Write, 2=Read, 3=Erase, 4=Partial Erase, 5=Verify: ')
            #prompt_text = input ('-- Operation: 1=Write, 2=Read, 3=Erase: ')

            #operation = int(prompt_text)
            operation = 1
   
            if (operation==3):                
                start_add = 0x0
                data_length = 0x80000

#            if (operation > 5):
            elif (operation > 3):
                print ('Invalid Entry \n')

            else:
                print ('\n-- Address Offset must start on a 4k boundary of flash memory.')
                print ('-- For example: 0x0, 0x1000, 0x2000, 0x3F000')
                #prompt_text = input ("Enter address offset> ") 
                #start_add = int(prompt_text, 16)
                start_add = 0x0

                print ('\n-- Data length must end on a 4k boundary')
                print ('-- For example: 0x1000, 0x20000, 0x40000, 0x60000, 0x80000')
                #prompt_text = input ("Enter data length in bytes> ") 
                #data_length = int(prompt_text, 16)        
                data_length = 0x40000
          
        if (menu_select == 2):
            utility()
            print('\nUtility ran.  Check folder for update_pgmhdrfile.bin.')
            
        if (menu_select == 0):
            break
  
        if (menu_select > 2):
            print('Invalid Entry\n')
    

if __name__ == '__main__':
    main()
