
from __future__ import with_statement
import sys
import binascii
import os
import serial
import serial.tools.list_ports
import time
import struct
import subprocess

file_line = ["S 55 AA 55 AA", "R 5A A5", "SF 65 header0.bin 40", "RF 65 CRC32", "SF 66 keyhashblob.bin 80", "RF 66 CRC32", "SF 67 fwbin0.bin 80", "RF 67 CRC32", "SC 70 CRC32", "SF 65 header0.bin 40", "RF 65 CRC32", "RC 70 9 CRC32", "SC 69 CRC32", "RC 69 CRC32"] 

###########0    1    2      3    4    5   6     7     8      9############
srpt_list=['s', 'r', 'f', 'rs', 'c','sc','rc','crc32','sf','rf']    #Script short forms
options = ['crc32']
fail_resp_cnt_dict = { 0x65: 6 , 0x66: 6, 0x67: 6, 0x68: 6, 0x69 : 6, 0x6A: 6, 0x70: 5}

cmd_3byte_offset = [0x65, 0x66, 0x67 ]
cmd_4byte_offset = [0x68 ]

NULL_BYTE = b''
INV_CMD = 3

  

def check_resp(b_rx, b_tx ):


    if b_rx != b'':
        b_hex1 = int(binascii.hexlify(b_rx),16)
        b_hex2 = int(b_tx,16)

        if  b_hex1 == b_hex2:
            return True
        elif (b_hex2 | 0x80) == (b_hex1):
            return False
        else:
            return INV_CMD
    else:
        return INV_CMD



def uart_communicator(uart):

    global FLNM, srpt_list, NULL_BYTE
    find_s = False
    find_sc = False
    find_sf = False
    find_r = False
    find_rc = False
    find_rf = False
    find_f = False
    find_rs = False
    loop_sts = False
    calc_crc_sts = False
    calc_crc_after_response = False
    crc_input_bytes = bytearray()
    loop_cmd = []
    sd_count = 0
    sd_content=[]
    s_count=1
    sc_count =1
    rc_data_count =0
    sf_count = 1
    rf_count = 0
    s_content=[]
    sc_content=[]
    sf_content=[]
    r_content=[]
    rc_content=[]
    rf_content=[]
    r_cont_disp=[]
    rc_cont_disp=[]
    rf_cont_disp=[]
    s_cont_disp=[]
    sc_cont_disp=[]
    sf_cont_disp=[]
    rs_content=[]
    rs_cont_disp=[]

 
    ############################ converting srpt_list str to binary type ##########################

    for lines in file_line:   			#for each command line (e.g. 1st line : 'S 55 AA 55 AA'
        content=lines.split()
        print("Command:>",content)

        if content != []:

            if find_s == False:
                s_content=[]
                s_cont_disp=[]

                if content[0].lower() == srpt_list[0]: #if 'S'
                    print ("\n")
                    find_s = True
                    for items in content[1:]:
                        h=binascii.unhexlify(items)
                        s_content.append(h)
                        s_cont_disp.append(items.lower())

                if content[0].lower() == srpt_list[4]: #if 'C'
                    s_count= int(content[-1],0)
                    if s_count==0:
                        s_count=1


            if find_sc == False:
                sc_content=[]
                sc_cont_disp=[]
                crc_input_bytes = bytearray()
                calc_crc_sts = False

                if content[0].lower() == srpt_list[5]: #if 'SC'
                    print ("\n")
                    find_sc = True

                    if content[-1].lower() == srpt_list[7]:
                        calc_crc_sts = True
                        last_byte = -1
                    # It will send data before last byte since last is CRC
                    else:
                        calc_crc_sts = False
                        last_byte = len(content)
                    # It will send data including last byte since there is no CRC needed

                    for items in content[1:last_byte]:
                        h=binascii.unhexlify(items)
                        sc_content.append(h)
                        sc_cont_disp.append(items.lower())
                        if calc_crc_sts == True:
                            crc_input_bytes.append(int(items,16))
                    if calc_crc_sts == True:
                        sc_crc32 = binascii.crc32(crc_input_bytes)

                        for i in range(4):
                            data = sc_crc32 & 0xFF
                            byte_conv = struct.pack('B',data)
                            sc_content.append(byte_conv)
                            sc_cont_disp.append(byte_conv)
                            sc_crc32 = sc_crc32 >> 8


                if content[0].lower() == srpt_list[4]: #if 'C'
                    sc_count= int(content[-1],16)
                    if sc_count==0:
                        sc_count=1

                #Add retry and incremental here

            if find_sf == False:
                sf_content=[]
                payload_size=0
                tx_file = ''
                tx_file_size =0

                sf_cmd = 0

                if content[0].lower() == srpt_list[8]:   #if 'SF'
                    print( "\n")
                    find_sf = True
                    payload_size = int(content[3].lower(),16)
                    tx_file = (content[2].lower()) 
                    sf_cmd = int(content[1].lower(),16)

                    h = binascii.unhexlify(content[1].lower())
                    sf_content.append(h)
                    h = binascii.unhexlify(content[3].lower())
                    sf_content.append(h)

                    try:
                        tx_file_size = os.path.getsize(tx_file)
                        if tx_file_size <1:
                            print(tx_file," has no data content \nPlease check the file")
                    except:
                        print(tx_file,": File is inaccessible\n")
                        sys.exit(1)


                if  content[0].lower() == srpt_list[4]:  #if 'C'
                    sf_count = int(content[-1],16)
                    if sf_count == 0:
                        sf_count = 1

            if find_s == True:
                r_content=[]
                if content[0].lower() == srpt_list[1]: #if 'R'
                    find_r = True
                    for items in content[1:]:
                        r_content.append(items.lower())


                if content[0].lower() == srpt_list[2]: #if 'F'
                    find_f = True
                    bin_file = open(content[1],"rb+")
                    binfl=content[1]
                    f_len=content[-1]

#####################################################

            if find_sc == True:
                rc_content=[]
                rc_data_count =0
                crc_input_bytes = bytearray()
                if content[0].lower() == srpt_list[6]: #if 'RC'
                    find_rc = True

                    if content[-1].lower() == srpt_list[7]:
                        calc_crc_sts = True
                    rc_content.append(content[1].lower())

            ######### Check for Receive Data count eg. get fw info #############
                    if content[2].lower() == srpt_list[7]:
                        rc_data_count = 0

                    else:
                        rc_data_count = int(content[2].lower(),16)

#####################################################

            if find_sf == True:
                rf_content =[]
                crc_input_bytes = bytearray()
                rf_count = 0

                if content[0].lower() == srpt_list[9]:   #if 'RF'
                    find_rf = True

                    if content[-1].lower() == srpt_list[7]:
                        calc_crc_sts = True
                        last_byte = -1

                    else:
                        calc_crc_sts = False
                        last_byte = len(content)


                    for items in content[1:last_byte]:
                        rf_content.append(items.lower())

                        if calc_crc_sts == True:
                            crc_input_bytes.append(int(items,16))

                    if calc_crc_sts == True:
                        rf_crc32 = binascii.crc32(crc_input_bytes)

                        for i in range(4):
                            data = rf_crc32 & 0xFF
                            byte_conv = struct.pack('B',data)

                            rf_content.append(byte_conv.hex())
                            rf_crc32 = rf_crc32 >> 8


                    ###############

            if content[0].lower() == srpt_list[3]: #if 'RS'
                rs_content=[]
                rs_cont_disp=[]
                find_rs = True
                for items in content[1:-1]:
                    h=binascii.unhexlify(items)
                    rs_content.append(h)
                    rs_cont_disp.append(items.lower())

                rs_count= int(content[-1],16)

        if(find_rs == True):

            find_rs= False
            for n in range(rs_count):
                for items in rs_content:
                    uart.write(items)
                    print('write')

            print  ('\nSend Repeatedly ', rs_cont_disp, 'No Of Times = ', rs_count)


        if (find_s == True and find_r == True): #for Combination of S AND R
            match_pat=False
            find_s = False
            find_r = False

            uart.reset_input_buffer() 
 
            r_len=len(r_content)                     #This is the S 55 AA 55 AA : R 5A A5 section
            print  ('Send data ----------------------- Receive Data ')
            for n in range(s_count):
                for items in s_content:
#                    print("data sent[",items,"]")
                    uart.write(items) #wr
                r_cont_disp=[]

                i=0 
                retry = 1
                while (i<2):
                    read = uart.read(2)
                    if (len(read) == 0):
                        i+=1
                    else:
#                        print ("read = ", read)
                        if (read[0] == 165 and read[1] == 90):
                            print ("Response Pattern matchced")
                            match_pat=True
                            break
                        elif (read[0] == 90 and read[1] ==165):
                            print ("Response Pattern matchced")
                            match_pat=True
                            break
                        else:
#                            if (retry == 0):
#                                match_pat=False
                            match_pat = True  #EC responded, but sometimes servo does not read the right data.  Needs debugging.
#                                print("Err!!! Response Pattern Not Matched!\n")
#                                sys.exit(1)
                            break
#                            else:
#                                retry=0
    
                    if (i==2):
                        print("Timeout Error - Did not receive response from EC.")
                        sys.exit(1)
                        break             
               
        if (find_sc == True and find_rc == True): #for Combination of SC and RC  ***this is the 'SC 69' & 'SC 70' section
            match_pat=False
            find_sc = False
            find_rc = False
            rc_len = 0
            crc_input_bytes = bytearray()

            if rc_data_count == 0 and calc_crc_sts == True:
                for items in rc_content:
                    crc_input_bytes.append(int(items,16))

                rc_crc32 = binascii.crc32(crc_input_bytes)

                for i in range (4):
                    data = rc_crc32 & 0xFF
                    byte_conv = struct.pack('B',data)
                    rc_content.append(binascii.hexlify(byte_conv))
                    rc_crc32 = rc_crc32 >> 8
            rc_len = len(rc_content)

            if rc_data_count > 0:
                rc_len = rc_len + rc_data_count

            print   (' Send Command data ------------------ Receive command response data  ')
            for n in range (sc_count):
                for items in sc_content:
#                    print("data sent [",items,"]\n")
                    uart.write(items) #wr
                rc_cont_disp=[]

  ######################## Command Sent !!!!!! Clear the Buffer and Receive the Response ########
       
                uart.reset_input_buffer()

                first_cmd_read = uart.read(1)
#                print("Read data :<",first_cmd_read,">\n")
                rc_len = rc_len -1
                b = first_cmd_read.hex()

                rc_cont_disp.append(b)

                resp_sts = check_resp(first_cmd_read, rc_content[0])

                if ((first_cmd_read == NULL_BYTE) or (resp_sts == INV_CMD) or (resp_sts == False)):

                    if rc_data_count == 0:
                        for m in range(rc_len):
                            read = uart.read(1)
#                            print( "Read data :<",read,">\n")
                            b = binascii.hexlify(read)
                            rc_cont_disp.append(b)
                    elif rc_data_count >0:
                        for m in range(rc_len):
                            read = uart.read(1)
#                            print( "Read data :<",read,">\n")
                            b = binascii.hexlify(read)
                            rc_cont_disp.append(b)

                        if calc_crc_sts == True:
                            for m in range(4):
                                read = uart.read(1)
#                                print(" Read data :<",read,">\n")
                                b = binascii.hexlify(read)
                                rc_cont_disp.append(b)

                elif resp_sts == True:
                    if rc_data_count> 0:
                        for m in range(rc_data_count):
                            read = uart.read(1)
#                            print("Read data :<",read,">\n")
                            b = binascii.hexlify(read)
                            rc_cont_disp.append(b)
                            rc_content.append(b)

                        if calc_crc_sts == True:
                            for m in range(4):
                                read = uart.read(1)
#                                print("Read data :<",read,">\n")
                                b = binascii.hexlify(read)
                                rc_cont_disp.append(b)

                        for items in rc_content:
                            crc_input_bytes.append(int(items,16))

                        rc_crc32 = binascii.crc32(crc_input_bytes)

                        for i in range (4):
                            data = rc_crc32 & 0xFF
                            byte_conv = struct.pack('B',data)
                            rc_content.append(binascii.hexlify(byte_conv))
                            rc_crc32 = rc_crc32 >> 8

                    if rc_data_count == 0:
                        for m in range(rc_len):
                            read = uart.read(1)
#                            print( "Read data :<",read,">\n")
                            b = binascii.hexlify(read)
                            rc_cont_disp.append(b)

#                print(sc_cont_disp,"\n",rc_cont_disp)
#                print("rc_cont_disp:", rc_cont_disp, "rc_content:", rc_content)

                if rc_cont_disp == rc_content:
                    print("Response command Pattern matched")
                    print("Exiting Send pattern - Total no of iteration = ",n+1,'/', sc_count)
                    match_pat = True
                    break
            if match_pat == False:
                print ("Err!!! Response Pattern Not Matched")
                print ("Completed total no of Iteration:", s_count)
                sys.exit(1)

            sc_count = 1
            rc_data_count = 0


        if (find_sf == True and find_rf == True):    # for Combination of SF and RF

            match_pat = False
            find_sf = False
            find_rf = False
            crc_input_bytes = bytearray()
            rf_len = len(rf_content)
            payload_offset = 0
            tx_data_size = tx_file_size
            byte_count =0
            tmp_pld_offset =0
            sf_cont_disp = []

            rf_len = len(rf_content)

            print (' Send File data SF--------------------------- Receive file response data ')
            try:
                tx_file_obj = open(tx_file,'rb+')

                for n in range(sf_count):
                    while(tx_file_size > 0 ):

                        print(" tx file size ", tx_file_size)
                        sf_cont_disp = []
                        crc_input_bytes = bytearray()

                        if tx_data_size > payload_size:
                            tx_data_size = payload_size

                        if tx_data_size != payload_size:
                            sf_content.pop()
                            b_data = struct.pack('B', tx_data_size)
                            sf_content.append(b_data)

######################### Transmit command and payload length###############

                        for items in sf_content:
#                            print("FileHeader data sent [",items,"]")
                            b_data = binascii.hexlify(items)
                            crc_input_bytes.append(int(b_data,16))
                            sf_cont_disp.append(b_data)
                            uart.write(items)

######################### Transmit payload offset ##########################
                        if sf_cmd in cmd_3byte_offset:
                            payload_offset = payload_offset & 0x00FFFFFF
                            byte_count = 3

                        elif sf_cmd in cmd_4byte_offset:
                            payload_offset = payload_offset & 0xFFFFFFFF
                            byte_count = 4

                        tmp_pld_offset = payload_offset
                        for i in range(byte_count):
                            byte_val = tmp_pld_offset & 0xFF
                            temp_byte = struct.pack('B',byte_val)
                            tmp_pld_offset = tmp_pld_offset >> 8
                            crc_input_bytes.append(byte_val)
#                            print( "File data sent [",temp_byte,"]")
                            b_data = binascii.hexlify(temp_byte)
                            sf_cont_disp.append(b_data)
                            uart.write(temp_byte)

######################### Transmit payload data in chunks(payload length) ##########################

                        if tx_data_size > payload_size:
                            tx_data_size = payload_size
                        file_content = tx_file_obj.read(tx_data_size)

                        for data in (file_content):
                            temp_byte = struct.pack('B',data)
                            crc_input_bytes.append(data)
#                            print( "File data sent [",temp_byte,"]")
                            b_data = binascii.hexlify(temp_byte)
                            sf_cont_disp.append(b_data)
                            uart.write(temp_byte)


    ################ Transmit CRC32 [cmd, payld_size, payld_offset, payld_data] #######################
#                        print( "crc_input_bytes: ", crc_input_bytes )   # TR

                        time.sleep(0.05) #Servo needs slight delay here to catch up

                        sf_crc32 = binascii.crc32(crc_input_bytes)

                        for i in range(4):
                            data = sf_crc32 & 0xFF
                            byte_conv = struct.pack('B',data)
#                            print( "File data sent [",byte_conv,"]")
                            b_data = binascii.hexlify(byte_conv)
                            sf_cont_disp.append(b_data)
                            uart.write(byte_conv)
                            sf_crc32 = sf_crc32 >> 8
        ######################## Command Sent !!!!!! Clear the Buffer and Receive the Response ########
                        uart.reset_input_buffer()
                        rf_cont_disp = []

                        for m in range(rf_len):
                            read = uart.read(1)
#                            print("File read data [",read,"]")
                            b=read.hex()

                            rf_cont_disp.append(b)
#                        print("rf_content", rf_content)
#                        print("rf_cont_disp", rf_cont_disp)


                        if rf_content == rf_cont_disp:
                            match_pat = True
#                            print("Response command Pattern matched\n")

                            payload_offset = payload_offset + tx_data_size
                            tx_file_size = tx_file_size - payload_size
                            tx_data_size = tx_file_size

                        else:
                            print ("Err!!! Response Pattern Not Matched\n")
#                            print ("Completed total no of Iteration:", s_count)
                            match_pat = False
                            break

                tx_file_obj.close()

            except:
                print("Can't open ",tx_file,": Please check the file\n")


                        #########################


        if (find_s == True and find_f == True): #for Combination of S AND F
            find_s = False
            find_f = False
            for items in s_content:
                uart.write(items) #wr

            print  ('Send Header --', s_cont_disp)
            print  ('Sending Bin file', binfl)
            #send file
            for byte in bin_file:
                uart.write(byte)



def main():
    print ('\n')
    print ('**************************************************************************')
    print ('UART.exe utility to send and receive commands from the Command prompt\n')
    print ('                        Version 1.0.2  09/16/21')
    print ('**************************************************************************')


#Servo - first find the port name by using $dut-control raw_ec_uart_pty cmmand, then make the port name change in the line below
    uart_port = subprocess.check_output("dut-control raw_ec_uart_pty", shell=True)
    uart_port= uart_port.decode("utf-8")
    uart_port = uart_port.replace("raw_ec_uart_pty:", "")
    uart_port = uart_port.replace("\n", "")
    #port = serial.Serial("/dev/pts/6", baudrate=9600, stopbits=1, timeout=3)
    port = serial.Serial(uart_port, baudrate=9600, stopbits=1, timeout=3)

#This line is for FTDI USB-RS232 cable - $dmsg | grep tty to find the port name of the cable and make the name change below
#    port = serial.Serial("/dev/ttyUSB0", baudrate=9600, stopbits=1, timeout=10)

    uart_ = port

    uart_communicator(uart_)
    uart_.close()

    print("!-!-!-!-! Exiting !-!-!-!-!")




if __name__ == '__main__':
    main()
