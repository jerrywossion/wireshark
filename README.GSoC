# Add support for usbdump file-format to Wireshark

Student: **Jie Weng** (jerrywossion@gmail.com)

Mentor: **Hans Petter Selasky** (hps@selasky.org)

----

### Project description

This project will add support for usbdump file-format to Wireshark. usbdump is the userland program of FreeBSD USB subsystem, which allows capturing packets that go through each USB host. Wireshark is a widely used network packet analyzer which is capable of capturing network packets and displaying the packet data as detailed as possible. Currently Wireshark does not support usbdump file-format of FreeBSD. By diving into Wireshark’s wiretap library, which handles a number of formats it supports, we can add support for usbdump file-format.

## Stage 1: Add file open/read support for usbdump file format

### Code

- [wiretap/usbdump.h](https://github.com/jerrywossion/wireshark/blob/usbdump_dissector/wiretap/usbdump.h)
- [wiretap/usbdump.c](https://github.com/jerrywossion/wireshark/blob/usbdump_dissector/wiretap/usbdump.c)

### The wiretap library of Wireshark

The wiretap library of Wireshak is used to read and write capture files in libpcap, pcapng, and many other file formats. It is the place we should work with. To add the ability to read usbdump file format, some steps are needed as below:

- Add a new ```WTAP_FILE_``` value for the file type to "wiretap/wtap.h". The value ```WTAP_FILE_TYPE_SUBTYPE_USBDUMP``` is added to identify the file format of usbdump.

- Write an "open" routine that can read the beginning of the capture file and figure out if it's in that format or not. The routine ```usbdump_open``` is added by checking the magic number at the beginning of each usbdump dumped file to check if it is the format we handle. (The file format of usbdump dumped file is explained later.). In this routine, we also set the ```file_encap``` to ```WTAP_ENCAP_USB_FREEBSD```. This is very important since it is used to tell Wireshark which dissector(s) to use to dissect the packets. 

- Write a "read" routine that can read a packet form the file and supply the packet length, captured data length, time stamp, and packet pseudo-header and data. This is the sequence version of two reading modes:

  - The sequence read: read the whole file sequentially. During this reading stage, some useful informations are collected, one of those is the offset of each data packet. The offset is very important for the second reading mode.

  - The random read: randomly read a data packet, for example, when Wireshark done reading the capture and display the packet list, when the user click on a specific list item, the random read routine will be called for this specific data packet. The offset of each data packet is collected and stored when doing sequence read.

- Write a "seek and read" routine that can seek to a specified location in the file for a packet. This is the random read version. 

- Add a pointer to the "open" routine to the "open_routines_base[]" table in "wiretap/file_access.c". Since we use the magic number to figure out the usbdump file format, we need add the point in the first section of that list, the second section are those who use a heuristic to figure out the file format.

- Add an entry for that file type in the "dump_open_table_base[]" in "wiretap/file_access.c", giving a descriptive name, a short name that's convenient to type on a command line (no blanks or capital letters, please), common file extensions to open and save, a flag if it can be compressed with gzip (currently unused) and pointers to the "can_write_encap" and "dump_open" routines if writing that file is supported (see below), otherwise just null pointers.

### The format of usbdump dumped file

The overall file format of usbdump dumped file is indicated as below:

```

```


## Stage 2: Add dissection support for usbdump packets/frames

### Code

- [epan/dissectors/packet-usb.c](https://github.com/jerrywossion/wireshark/blob/usbdump_dissector/epan/dissectors/packet-usb.c#L4074)

### Method

----

**Reference**

- [Wireshark Developer's Guide](https://www.wireshark.org/docs/wsdg_html/#ChWorksOverview)
- [wiretap/README.developer](https://github.com/wireshark/wireshark/blob/master/wiretap/README.developer)
