
from __future__ import with_statement
import sys
import binascii
import os
import serial
import serial.tools.list_ports
import time
import struct
import subprocess


operation = 1 #1=WR, 2=RD, 3=ERASE
file_line=[""]

###########0    1    2      3    4    5   6     7     8      9    10    11   ############
srpt_list=['s', 'r', 'f', 'rs', 'c','sc','rc','crc32','sf','rf', 'gf', 'rg']    #Script short forms
fail_resp_cnt_dict = { 0x65: 6 , 0x66: 6, 0x67: 6, 0x68: 6, 0x69 : 6, 0x6A: 6, 0x70: 5}
                       
cmd_3byte_offset = [0x65, 0x67, 0x66 ]
cmd_4byte_offset = [0x68]
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
        global srpt_list, NULL_BYTE
        find_s = False
        find_sc = False
        find_sf = False
        find_r = False
        find_rc = False
        find_rf = False
        find_f = False
        find_rs = False
        find_gf = False          
        find_rg = False

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
        gf_count=1             
        rg_data_count =0       
        gf_content=[]          
        gf_cont_disp=[]        
        rg_content=[]
        rg_cont_disp=[]


# Enable debug
#        import pudb; pudb.set_trace()
        ############################ converting srpt_list str to binary type ##########################

        try:
            for lines in file_line:
                content=lines.split()
                print("Line:>",content)
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
                        
                            if content[-1].lower() == srpt_list[7]:  # CRC32
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
                                    print( "DbgLine #170[",hex(data),"]" )
                                    byte_conv = struct.pack('B',data)
                                    sc_content.append(byte_conv)
                                    sc_cont_disp.append(byte_conv)
                                    sc_crc32 = sc_crc32 >> 8

                            
                        if content[0].lower() == srpt_list[4]: #if 'C'
                            sc_count= int(content[-1],16)
                            if sc_count==0:
                                sc_count=1

                    if find_gf == False:            
                        gf_content=[]
                        gf_payload_size=0
                        rx_file = ''
                        rx_file_size =0
                        gf_cmd = 0

                        if content[0].lower() == srpt_list[10]:   #if 'GF'
#                            print ("\n #if 'GF' ")
                            find_gf = True

                            gf_payload_size = int(content[3].lower(),16)
                            print ("\n gf_payload_size <",gf_payload_size,">" )
                            rx_file = (content[2].lower())  
                            print("Read File name: ", rx_file)
                            gf_cmd = int(content[1].lower(),16)
                            print ("\n gf_cmd <",gf_cmd,">" )

                            h = binascii.unhexlify(content[1].lower())
                            gf_content.append(h)
                            h = binascii.unhexlify(content[3].lower())
                            gf_content.append(h)
 
                            if content[0].lower() == srpt_list[4]: #if 'C'
                                gf_count= int(content[-1],16)
                                if gf_count==0:
                                    gf_count=1
           
                    if find_sf == False:
                        sf_content=[]
                        sf_payload_size=0
                        tx_file = ''
                        tx_file_size =0

                        sf_cmd = 0

                        if content[0].lower() == srpt_list[8]:   #if 'SF'
                            print( "\n")
#                            print ("\n #if 'SF' ")
                            find_sf = True
                            sf_payload_size = int(content[3].lower(),16)
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
#                            print ("\n if 'C' ")
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
                        
                    if find_sc == True:
                        rc_content=[]
                        rc_data_count =0
                        crc_input_bytes = bytearray()
                        if content[0].lower() == srpt_list[6]: #if 'RC'
                            find_rc = True

                            if content[-1].lower() == srpt_list[7]: #CRC32
                                calc_crc_sts = True
                            rc_content.append(content[1].lower())

                    ######### Check for Receive Data count eg. get fw info #############
                            if content[2].lower() == srpt_list[7]:  #CRC32
                                rc_data_count = 0
                            else:
                                rc_data_count = int(content[2].lower(),16)                                                         
                                 

                    if find_gf == True:
                        rg_content=[]
                        crc_input_bytes = bytearray()
                        rg_count =0
 
                        if content[0].lower() == srpt_list[11]: #if 'RG'
                            find_rg = True

                            if content[-1].lower() == srpt_list[7]: #CRC32
                                calc_crc_sts = True
                                last_byte = -1

                            else:
                                calc_crc_sts = False
                                last_byte = len(content)

                            for items in content[1:last_byte]:
                                rg_content.append(items.lower())

                                if calc_crc_sts == True:
                                    crc_input_bytes.append(int(items,16))

                            if calc_crc_sts == True:
                                rg_crc32 = binascii.crc32(crc_input_bytes)

                                for i in range(4):
                                    data = rg_crc32 & 0xFF
                                    byte_conv = struct.pack('B',data)
                                    rg_content.append(byte_conv.hex())
                                    rg_crc32 = rg_crc32 >> 8


                    if find_sf == True:
                        rf_content =[]
                        crc_input_bytes = bytearray()
                        rf_count = 0

                        if content[0].lower() == srpt_list[9]:   #if 'RF'
                            find_rf = True

                            if content[-1].lower() == srpt_list[7]: #CRC32
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
                
                
                    r_len=len(r_content)                           #****this is the S 33 CC : R 3C C3 section
                    print  ('Send data ----------------------- Receive Data ')
                    for n in range(s_count):
                        for items in s_content:
                            print("data sent[",items,"]\n")
                            uart.write(items) #wr
                        r_cont_disp=[]

                        time.sleep(0.15)  #wait for EC to respond
                       
                        for m in range(r_len):
                            read = uart.read(1) 
                            print("Read data :<",read,">")
                            b=read.hex()    
                            r_cont_disp.append(b)
                        print  (s_cont_disp,"                    ", r_cont_disp)
                        if r_cont_disp == r_content:
                            print ("Response Pattern matched")
                            print ("exiting Send pattern - Total no of iternation = ", n+1 ,'/', s_count)
                            match_pat=True
                            break
                    if match_pat==False:
                        print ("!!!ERROR: CRC Response Pattern Not Matched!!!")
                        sys.exit(1)
        
                    s_count=1

                if (find_sc == True and find_rc == True): #for Combination of SC and RC - this is the 'SC 69' & 'SC 70' section
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
                            print("SC data SENT [",items,"]")
                            uart.write(items) #wr
                        rc_cont_disp=[]

      ######################## Command Sent !!!!!! Clear the Buffer and Receive the Response ########
                        uart.reset_input_buffer()

                        first_cmd_read = uart.read(1)
                        print("SC READ data :<",first_cmd_read,">")
                        rc_len = rc_len -1
                        b = binascii.hexlify(first_cmd_read)
                        rc_cont_disp.append(b)

                        resp_sts = check_resp(first_cmd_read, rc_content[0])

                        if ((first_cmd_read == NULL_BYTE) or (resp_sts == INV_CMD) or (resp_sts == False)):

                            if rc_data_count == 0:
                                for m in range(rc_len):
                                    read = uart.read(1)
                                    print( "RC READ2 data :<",read,">")
                                    b = binascii.hexlify(read)
                                    rc_cont_disp.append(b)
                            elif rc_data_count >0:
                                for m in range(rc_len):
                                    read = uart.read(1)
                                    print( "RC READ3 data :<",read,">")
                                    b = binascii.hexlify(read)
                                    rc_cont_disp.append(b)
                            
                                if calc_crc_sts == True:
                                    for m in range(4):
                                        read = uart.read(1)
                                        print("RC READ4 data:<",read,">")
                                        b = binascii.hexlify(read)
                                        rc_cont_disp.append(b)
                     
                        elif resp_sts == True:
                            if rc_data_count> 0:
                                for m in range(rc_data_count):
                                    read = uart.read(1)
                                    print("RC READ5 data :<",read,">")
                                    b = binascii.hexlify(read)
                                    rc_cont_disp.append(b)
                                    rc_content.append(b)

                                if calc_crc_sts == True:
                                    for m in range(4):
                                        read = uart.read(1)
                                        print("RC READ6 data :<",read,">")
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
                                    print( "RC READ7 data :<",read,">")
                                    b = binascii.hexlify(read)
                                    rc_cont_disp.append(b)                            
                            
                        
                        print(sc_cont_disp,"\n",rc_cont_disp)
                        if rc_cont_disp == rc_content:
                            print("Response command Pattern matched")
                            print("Exiting Send pattern - Total no of iteration = ",n+1,'/', sc_count)
                            match_pat = True
                            break
                    if match_pat == False:
                        print ("!!!ERROR: CRC Response Pattern Not Matched!!!")
                        sys.exit(1)
                
                    sc_count = 1
                    rc_data_count = 0
                        

                if (find_gf == True and find_rg == True):    # for Combination of GF and RF
                    match_pat = False
                    find_gf = False
                    find_rg = False
                    crc_input_bytes = bytearray()
                    rg_len = len(rg_content)
                    gf_payload_offset = 0
                    
                    byte_count =0
                    tmp_pld_offset =0
                    gf_cont_disp = []

                    rg_len = len(rg_content)
                    #try sending CMD_GET_RD_CNT (0x68)  to F/W to have it return the number of bytes (from PgmHdrFile.bin) that we are reading
                    uart.write(b'\x68')

                    # get response from F/W
                    rx_file_size = 0

                    for i in range(4):
                        shift_it = i * 8
                        temp_size = uart.read(1)
                        size_var = int.from_bytes(temp_size, "big")
                        size_var = size_var & 0xFF
                        size_num = size_var << shift_it
                        rx_file_size = rx_file_size + size_num

                    print("\n rx file size ", rx_file_size)

                    rx_data_size = rx_file_size
                       
                    print (' Send File params GF--------------------------- Receive file response data ')
                    try:
                        rx_file_obj = open(rx_file, 'wb')
                        print(" rx_file opened")

                        for n in range (gf_count):
                            while(rx_file_size > 0 ):

                                print(" rx file size ", rx_file_size)
                                gf_cont_disp = []
                                crc_input_bytes = bytearray()
                            
                                if rx_data_size > gf_payload_size:
                                    rx_data_size = gf_payload_size

                                if rx_data_size != gf_payload_size:
                                    gf_content.pop()
                                    b_data = struct.pack('B', rx_data_size)
                                    gf_content.append(b_data)

    ######################### Transmit command and payload length###############
                                for items in gf_content:
#                                    print("GF-Header data SENT [",items,"]")
                                    b_data = binascii.hexlify(items)    ## Return the 2-digit hexadecimal representation of the binary data in "items"
                                    crc_input_bytes.append(int(b_data,16))  ##b_data added as ascii
                                    gf_cont_disp.append(b_data)
                                    uart.write(items) #wr

#                                print( "1st crc_input_bytes", crc_input_bytes)  ##this is only 2 bytes = 'f'(0x66), 0x80
                        
        ######################### Transmit payload offset ##########################
                                if gf_cmd in cmd_3byte_offset:
                                    gf_payload_offset = gf_payload_offset & 0x00FFFFFF
                                    byte_count = 3
                                    
                                elif gf_cmd in cmd_4byte_offset:
                                    gf_payload_offset = gf_payload_offset & 0xFFFFFFFF
                                    byte_count = 4

                                tmp_pld_offset = gf_payload_offset    
                                for i in range(byte_count):
                                    byte_val = tmp_pld_offset & 0xFF  ##From 'gf_payload_offset'
                                    ## from 'gf_payload_offset'
                                    temp_byte = struct.pack('B',byte_val)
                                    tmp_pld_offset = tmp_pld_offset >> 8
                                    crc_input_bytes.append(byte_val)
#                                    print( "GF-Offset data sent [",hex(byte_val),"]" )
                                    b_data = binascii.hexlify(temp_byte)
                                    gf_cont_disp.append(b_data)
                                    uart.write(temp_byte)

#                                print( "2nd crc_input_bytes", crc_input_bytes)    ##This is 5 bytes = 'f'(0x66), 0x80, 0x00, 0x00, 0x00  cmd+chunk+(3)offset

        ######################### RECEIVE payload data in chunks(payload length) ##########################
#                                print( "rx_data_size", rx_data_size)
#                                print( "gf_payload_size", gf_payload_size)

                                if rx_data_size > gf_payload_size:
                                    rx_data_size = gf_payload_size                           
                                    file_content = rx_data_size
#                                print( "rx_data_size", rx_data_size)
                                
                                for i in range(0,rx_data_size):
                                    temp_byte = uart.read(1) 
#                                    print( "GF-File2 data recvd [",temp_byte,"]" ) 
                                    rx_file_obj.write(temp_byte)
                                    rx_pay_data = binascii.hexlify(temp_byte)    ##Return the 2-digit hexadecimal representation of the binary data in "items"
                                    crc_input_bytes.append(int(rx_pay_data,16))  ##rb_data added as ascii


        ################ Transmit CRC32 [cmd, payld_size, payld_offset, payld_data] #######################
#                                print( "3rd crc_input_bytes: ", crc_input_bytes )   # TR

                                gf_crc32 = binascii.crc32(crc_input_bytes)

                                for i in range(4):
                                    data = gf_crc32 & 0xFF
                                    byte_conv = struct.pack('B',data)
#                                    print( "GF-File3 data sent [",hex(data),"]" )
                                    b_data = binascii.hexlify(byte_conv)
                                    gf_cont_disp.append(b_data)
                                    uart.write(byte_conv)
                                    gf_crc32 = gf_crc32 >> 8

                ######################## Command Sent !!!!!! Clear the Buffer and Receive the Response ########
                                
                                rg_cont_disp = []
                            
                                for m in range(rg_len):
                                    read = uart.read(1)
#                                    print("GF-File read resp-CRC32 data [",read,"]")
                                    b=read.hex() 
                                    rg_cont_disp.append(b)  ###This is the response from F/W only 5 bytes command+4 byte CRC                               
#                                print("_GF_cont_disp",gf_cont_disp,"\n_GF_rg_cont_disp",rg_cont_disp)  #This line must be here
#                                print("_GF_rg_cont", rg_content)

                                if rg_content == rg_cont_disp:
                                    match_pat = True
#                                    print("Response command Pattern matched")
                                
                                    gf_payload_offset = gf_payload_offset + rx_data_size
                                    rx_file_size = rx_file_size - gf_payload_size
                                    rx_data_size = rx_file_size

                                else:
                                    print ("!!!ERROR: CRC Response Pattern Not Matched!!!")
                                    match_pat = False
                                    sys.exit(1)
                                    break
                        rx_file_obj.close()

                    except:
                        print("Can't open ",rx_file,": Please check the file\n")                
                        sys.exit(1)

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
                            
                                if tx_data_size > sf_payload_size:
                                    tx_data_size = sf_payload_size

                                if tx_data_size != sf_payload_size:
                                    sf_content.pop()
                                    b_data = struct.pack('B', tx_data_size)
                                    sf_content.append(b_data)

        ######################### Transmit command and payload length###############
                            
                                for items in sf_content:
#                                    print("SF_FileHeader data sent [",items,"]")
                                    b_data = binascii.hexlify(items)
                                    crc_input_bytes.append(int(b_data,16))
                                    sf_cont_disp.append(b_data)
                                    uart.write(items)

#                                print( "1st crc_input_bytes", crc_input_bytes)  ##this is only 2 bytes = 'f'(0x65), 0x80
                                
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
#                                    print( "SF_File data sent [",hex(byte_val),"]" )
                                    b_data = binascii.hexlify(temp_byte)
                                    sf_cont_disp.append(b_data)
                                    uart.write(temp_byte)

#                                print( "2nd crc_input_bytes", crc_input_bytes)    ##this is 5 bytes = 'e'(0x65), 0x80, 0x00, 0x00, 0x00  cmd+chunk+(3)offset
                                
        ######################### Transmit payload data in chunks(payload length) ##########################

                                if tx_data_size > sf_payload_size:
                                    tx_data_size = sf_payload_size                           
                                file_content = tx_file_obj.read(tx_data_size)       ###Here's the read to file_content
#                                print ("\nFILE_CONTENT:", file_content)

                                for data in (file_content):         ##file_content is the entire chunk of data read from binary file
                                    temp_byte = struct.pack('B',data)
                                    crc_input_bytes.append(data)
#                                    print( "SF_File2 data sent [",hex(data),"]" )
                                    b_data = binascii.hexlify(temp_byte)
                                    sf_cont_disp.append(b_data)
                                    uart.write(temp_byte)
                                 
        ################ Transmit CRC32 [cmd, payld_size, payld_offset, payld_data] #######################
#                                print( "_SF_crc_input_bytes: ", crc_input_bytes )   # TR
                                sf_crc32 = binascii.crc32(crc_input_bytes)

                                for i in range(4):
                                    data = sf_crc32 & 0xFF
                                    byte_conv = struct.pack('B',data)
#                                    print( "SF_File3 data sent [",hex(data),"]" )

                                    b_data = binascii.hexlify(byte_conv)
                                    sf_cont_disp.append(b_data)   ##this is all the data sent starting from 0x65, 0xF0, offset, then all data in binary header file
                                    uart.write(byte_conv)
                                    sf_crc32 = sf_crc32 >> 8
                ######################## Command Sent !!!!!! Clear the Buffer and Receive the Response ########
                                uart.reset_input_buffer()
#                                time.sleep(0.05)  #no need for this

                                rf_cont_disp = []
                                
                                if (tx_file_size==128 or tx_file_size==262272):
                                    print("\n*** Waiting for EC to program.  This will take about 22s. ***")
                                    time.sleep(22)    #at 0x40000 and 0x80000 boundary, takes up to 22s to program before EC sends ACK

                                for m in range(rf_len):
                                    read = uart.read(1)
#                                    print("SF_File read resp-CRC32 data [",read,"]")
                                    b = read.hex() 
                                    rf_cont_disp.append(b)     ### this is the response from F/W only 5 bytes command+4 byte CRC
#                                print("_SF_sf_cont_disp",sf_cont_disp,"\n_SF_rf_cont_disp",rf_cont_disp)
#                                print ("_SF_rf_content", rf_content)
#                                print("_SF_rf_cont_disp",rf_cont_disp, " ")
                                
                                if rf_content == rf_cont_disp:
                                    match_pat = True
#                                    print("Response command Pattern matched\n")
                                
                                    payload_offset = payload_offset + tx_data_size
                                    tx_file_size = tx_file_size - sf_payload_size
                                    tx_data_size = tx_file_size

                                else:
                                    print ("!!!ERROR: CRC Response Pattern Not Matched!!!\n")
                                    match_pat = False
                                    sys.exit(1) 
                                    break
                    
                        tx_file_obj.close()
                        print("Close tx_file")

                    except:
                        print("Can't open ",tx_file,": Please check the file\n")                
                        os._exit(1)
                        
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
                    '''    
                    uart.read(1)  
                    uart.read(1)                 
                    r_cont_disp=[]
                    for m in range(14):    
                        read = uart.read(1)
                        b=binascii.hexlify(read)
                        r_cont_disp.append(b)
                    print r_cont_disp
                    '''

        except KeyboardInterrupt:
            print ("\n!!!Keyboard Interrupt: Script Terminated!!!")
            sys.exit(1)                              
    
	
def main():

    global operation, file_line
    prompt_text=''

    print ('************************************************')
    print ('**         !!! REQUIREMENT !!!                **')
    print ('** Please run MakePgmHdr.py to generate       **')
    print ('** Update_PgmHdrFile.bin before continuing    **')
    print ('************************************************')
    print ('** Select Operation to Perform on SPI Flash   **')
    print ('** 1 - Program                                **')
    print ('** 2 - Read                                   **')
    print ('** 3 - Erase                                  **')
#    print ('** 4 - Partial Erase                          **')
#    print ('** 5 - Verify                                 **')
    print ('** 4 - Quit                                   **')
    print ('************************************************')
    #prompt_text = input ('>')
    #operation = int (prompt_text)
    operation = 1

    
    if (operation == 1):
        file_line=["S 33 CC","R 3C C3","SF 65 update_pgmhdrfile.bin F0","RF 65 CRC32","SF 67 spi_fw.bin 80", "RF 67 CRC32"]   
    if (operation == 2):
        file_line=["S 33 CC","R 3C C3","SF 65 update_pgmhdrfile.bin F0","RF 65 CRC32","GF 66 read512k.bin 80", "RG 66 CRC32"]
    if (operation == 3):
        file_line=["S 33 CC","R 3C C3","SF 65 update_pgmhdrfile.bin F0","RF 65 CRC32"]
    if (operation == 4):
        sys.exit(1)
   
       
#First find the port name by using $dut-control ec_uart_pty, then make the port name change in the line below	
    uart_port = subprocess.check_output("dut-control raw_ec_uart_pty", shell=True)
    uart_port= uart_port.decode("utf-8")
    uart_port = uart_port.replace("raw_ec_uart_pty:", "")
    uart_port = uart_port.replace("\n", "")
    #port = serial.Serial("/dev/pts/6", baudrate=57600, stopbits=1, timeout=10)
    port = serial.Serial(uart_port, baudrate=57600, stopbits=1, timeout=10)
#This line is for FTDI USB-RS232 cable - $dmsg | grep tty to find the port name of the cable and make the name change below
#    port = serial.Serial("/dev/ttyUSB0", baudrate=57600, stopbits=1, timeout=10)    
    uart_ = port
    
    uart_communicator(uart_)
    uart_.close()

    print("!-!-!-!-! Exiting !-!-!-!-!")
        
        
     

if __name__ == '__main__':
    main()
