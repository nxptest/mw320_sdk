#!usr/bin/python
#
# Copyright 2021 NXP
# All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
# Usage: python mw_img_conv.py <option> <input_file> <output_img> [<load_addr>]
#   option:
#     layout: Convert layout config file to layout flash image.
#     mcufw:  Convert MCU image to MCU flash image. <load_addr> needed.
#     wififw: Convert WiFi image to WiFi flash image

import os, sys, time
import zlib

MAX_FL_COMP     = 16
MCU_FW_OFFSET   = 0x100  # MCU firmware application offset from image header
SEG_CNT         = 9

g_comp_type = {
    'FC_COMP_BOOT2'    : 0,
    'FC_COMP_FW'       : 1,
    'FC_COMP_WLAN_FW'  : 2,
    'FC_COMP_FTFS'     : 3,
    'FC_COMP_PSM'      : 4,
    'FC_COMP_USER_APP' : 5,
    'FC_COMP_BT_FW'    : 6
}

class PartTable:
    def __init__(self):
        self.magic = "WMPT"
        self.version = 1
        self.partition_entries_no = 0 # 2B
        self.gen_level = 0 # 4B

    def toByteArray(self):
        codes = self.magic.encode(encoding="utf-8")
        codes += self.version.to_bytes(2, "little") 
        codes += self.partition_entries_no.to_bytes(2, "little")
        codes += self.gen_level.to_bytes(4, "little")
        return codes

class PartEntry:
    def __init__(self):
        self.type = 0 # 1B
        self.device = 0 # 1B
        self.name = None # 8B
        self.start = 0 # 4B
        self.size = 0 # 4B
        self.gen_level = 0 # 4B

    def toByteArray(self):
        codes = self.type.to_bytes(1, "little")
        codes += self.device.to_bytes(1, "little") 
        codes += self.name[:8].encode(encoding="utf-8")
        pad = 10 - len(self.name[:8])
        codes += (0).to_bytes(pad, "little")
        codes += self.start.to_bytes(4, "little")
        codes += self.size.to_bytes(4, "little")
        codes += self.gen_level.to_bytes(4, "little")
        return codes

class ImgHdr:
    def __init__(self, entry):
        self.magic_str = "MRVL" # 4B
        self.magic_sig = 0x2E9CF17B # 4B
        self.time = int(time.time()) # 4B
        self.seg_cnt = 1 # 4B
        self.entry = entry # 4B

    def getSize(self):
        return 20

    def toByteArray(self):
        codes = self.magic_str.encode(encoding="utf-8")
        codes += self.magic_sig.to_bytes(4, "little") 
        codes += self.time.to_bytes(4, "little")
        codes += self.seg_cnt.to_bytes(4, "little")
        codes += self.entry.to_bytes(4, "little")
        return codes

class SegHdr:
    def __init__(self, laddr):
        self.type = 2 # 4B
        self.offset = MCU_FW_OFFSET # 4B
        self.len = 0 # 4B
        self.laddr = laddr # 4B
        self.crc = 0 # 4B

    def getSize(self):
        return 20

    def toByteArray(self):
        codes = self.type.to_bytes(4, "little")
        codes += self.offset.to_bytes(4, "little") 
        codes += self.len.to_bytes(4, "little")
        codes += self.laddr.to_bytes(4, "little")
        codes += self.crc.to_bytes(4, "little")
        for i in range(MCU_FW_OFFSET - 40):
            codes += b'\xff'
        return codes

class WlanFwHeader:
    def __init__(self):
        self.magic = "WLFW" # 4B
        self.length = 0 # 4B

    def toByteArray(self):
        codes = self.magic.encode(encoding="utf-8")
        codes += self.length.to_bytes(4, "little") 
        return codes

def die_usage(argv0):
    print("Usage: %s <option> <input_file> <output_img> [<load_addr>]" %argv0)
    print("  option:")
    print("    layout: Convert layout config file to layout flash image.")
    print("    mcufw:  Convert MCU image to MCU flash image. <load_addr> needed.")
    print("    wififw: Convert WiFi image to WiFi flash image")
    exit()

def soft_crc32(data, crc):
    crc = zlib.crc32(bytes(data), crc ^ 0xffffffff) ^ 0xffffffff
    return crc

def convert_layout(fin, fout):
    flash_parts = []

    while len(flash_parts) < MAX_FL_COMP:
        line = fin.readline()
        if not line:
            break

        # convert bytes to string
        line = line.decode('utf-8')

        if line[0] == '#' or line[0] == '\n':
            continue

        components = line.split()
        if len(components) != 5:
            print("Invalid record (should be 5 items per line):")
            print("  ", line)
            return

        comp = PartEntry()

        if not components[0] in g_comp_type:
            print("Wrong component type:", components[0])
            return
        comp.type = g_comp_type[components[0]]
        comp.start = int(components[1], 0)
        comp.size = int(components[2], 0)
        comp.device = int(components[3], 0)
        comp.name = components[4]
        comp.gen_level = 1

        flash_parts.append(comp)

    flash_table = PartTable()
    flash_table.partition_entries_no = len(flash_parts)

    byte_array = flash_table.toByteArray()
    crc = soft_crc32(byte_array, 0)
    fout.write(byte_array)
    fout.write(crc.to_bytes(4, "little"))

    crc = 0
    for comp in flash_parts:
        byte_array = comp.toByteArray()
        crc = soft_crc32(byte_array, crc)
        fout.write(byte_array)

    fout.write(crc.to_bytes(4, "little"))

def convert_mcufw(fin, fout, laddr):
    fin.seek(0, os.SEEK_END)
    size = fin.tell()
    fin.seek(4, os.SEEK_SET)

    if size < 8:
        print("mcufw file too small")
        return

    entry = int.from_bytes(fin.read(4), byteorder="little")
    fin.seek(0, os.SEEK_SET)

    ih = ImgHdr(entry)
    sh = SegHdr(laddr)

    if MCU_FW_OFFSET - ih.getSize() < SEG_CNT * sh.getSize():
        print("MCU_FW_OFFSET too small")
        return

    # padding firmware to 4 bytes aligned
    pad = 3 - ((size + 3) % 4)
    data = fin.read(size)
    for i in range(pad):
        data += b'\xff'
    size += pad

    sh.len = size
    sh.crc = soft_crc32(data, 0)

    fout.write(ih.toByteArray())
    fout.write(sh.toByteArray())
    fout.write(data)

def convert_wififw(fin, fout):
    fin.seek(0, os.SEEK_END)
    size = fin.tell()
    fin.seek(0, os.SEEK_SET)

    wf_header = WlanFwHeader()
    wf_header.length = size

    fout.write(wf_header.toByteArray())
    
    data = fin.read(size)
    fout.write(data)

if __name__ == "__main__":
    argc = len(sys.argv)
    if argc != 4 and argc != 5:
        die_usage(sys.argv[0])

    try:
        with open(sys.argv[2], 'rb') as fin:
            with open(sys.argv[3], 'wb') as fout:
                if sys.argv[1] == "layout":
                    convert_layout(fin, fout)
                elif sys.argv[1] == "mcufw":
                    if argc != 5:
                        die_usage(sys.argv[0])
                    laddr = int(sys.argv[4], 0)
                    print("Convert MCU firmware with load address 0x%x" %laddr)
                    convert_mcufw(fin, fout, laddr)
                elif sys.argv[1] == "wififw":
                    convert_wififw(fin, fout)
                else:
                    die_usage(sys.argv[0])
    except IOError:
        print("File IO error")
