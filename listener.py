import struct
import serial
import sys
import os
import subprocess
import signal
import datetime
import platform
import textwrap


def create_pcap(filename):
    print("Creating capture file: pcap/%s" % filename)
    folder_path = f"{os.path.dirname(os.path.abspath(__file__))}/pcap"
    if not os.path.exists(folder_path):
        os.makedirs(folder_path)
        print(f"Folder '{folder_path}' created.")
    else:
        print(f"Folder '{folder_path}' already exists.")
    path = f"{folder_path}/{filename}"
    f = open(path, 'wb')
    global_header = struct.pack(
        '<IHHIIII',
        0xa1b2c3d4,  # magic number
        2,           # version major
        4,           # version minor
        0,           # thiszone
        0,           # sigfigs
        65535,       # snaplen
        105          # network (Ethernet)
    )
    f.write(global_header)
    return (f, path)


def add_packet_unix(packet_bytes, f):
    raw_packet = bytes.fromhex(packet_bytes)
    # Packet Header (16 bytes)
    packet_header = struct.pack(
        '<IIII',
        0,                  # ts_sec
        0,                  # ts_usec
        len(raw_packet),    # incl_len
        len(raw_packet)     # orig_len
    )

    # Write to file
    f.write(packet_header)
    f.write(raw_packet)

def add_packet_windows(packet_bytes, p):
    import win32file
    raw_packet = bytes.fromhex(packet_bytes)
    # Packet Header (16 bytes)
    packet_header = struct.pack(
        '<IIII',
        0,                  # ts_sec
        0,                  # ts_usec
        len(raw_packet),    # incl_len
        len(raw_packet)     # orig_len
    )

    # Write to file
    win32file.WriteFile(p, packet_header + raw_packet)


def unix_complient_main():
    if len(sys.argv) < 2:
        print(textwrap.dedent(f"""
            Missing argument port_path
            python listener.py <port_path>

            Example:
            python listener.py /dev/ttyACM0
        """))
        sys.exit(1)
    filename = "capture_%s.pcap" % datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    f, path = create_pcap(filename)
    port = f"{sys.argv[1]}"
    ser = serial.Serial(port, 921600)
    cmd = "tail -f -c +0 " + path + " | wireshark -k -i -"
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True, preexec_fn=os.setsid)

    try:
        while True:
            line = ser.readline()
            line = line.decode().strip()
            try:
                if "Data" in line:
                    data = line.split("Data: ")[1]
                    add_packet_unix(data, f)
                    f.flush()
                    #write_hex(f, data)
            except:
                pass

    except KeyboardInterrupt:
        print("Stopping Wireshark...")
        os.killpg(os.getpgid(p.pid), signal.SIGTERM)
        pass
    except Exception as e:
        print(e)
        os.killpg(os.getpgid(p.pid), signal.SIGTERM)
        pass

def bad_os_main():
    if len(sys.argv) < 2:
        print(textwrap.dedent(f"""
            Missing argument port
            python listener.py <port>

            Example:
            python listener.py COM10
        """))
        sys.exit(1)
    import win32pipe, win32file, pywintypes
    pipe_name = r'\\.\pipe\mypipe'
    bufsize = 65535

    p = subprocess.Popen(['wireshark', '-k', '-i', pipe_name])

    pipe = win32pipe.CreateNamedPipe(
        pipe_name,
        win32pipe.PIPE_ACCESS_OUTBOUND,
        win32pipe.PIPE_TYPE_BYTE | win32pipe.PIPE_WAIT,
        1, bufsize, bufsize, 0, None
    )

    print("Waiting for Wireshark to connect to named pipe...")
    win32pipe.ConnectNamedPipe(pipe, None)
    print("Wireshark connected!")

    # Write global pcap header
    global_header = struct.pack('<IHHIIII', 0xa1b2c3d4, 2, 4, 0, 0, 65535, 105)
    win32file.WriteFile(pipe, global_header)
    port = f"{sys.argv[1]}"
    ser = serial.Serial(port, 921600)
    try:
        while True:
            line = ser.readline()
            line = line.decode().strip()
            try:
                if "Data" in line:
                    data = line.split("Data: ")[1]
                    add_packet_windows(data, pipe)
                    #write_hex(f, data)
            except:
                pass

    except KeyboardInterrupt:
        print("Stopping Wireshark...")
        p.terminate()
        pass
    except Exception as e:
        print(e)
        p.terminate()
        pass



if __name__ == "__main__":
    match platform.system():
        case "Windows":
            bad_os_main()
        case "Linux":
            unix_complient_main()
        case "Darwin":
            unix_complient_main()
        case _ :
            raise Exception("System not suported")
