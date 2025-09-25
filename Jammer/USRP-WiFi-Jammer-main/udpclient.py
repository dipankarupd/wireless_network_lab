import argparse, socket, time, struct, os, sys
p=argparse.ArgumentParser()
p.add_argument("--ip", default="192.168.4.1")
p.add_argument("--port", type=int, default=5001)
p.add_argument("--size", type=int, default=400)
p.add_argument("--rate", type=int, default=200)  # packets per second
p.add_argument("--secs", type=int, default=30)
a=p.parse_args()

sock=socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.settimeout(0.05)
addr=(a.ip,a.port)
seq=0; sent=0; ack=0; lost=0
t0=time.time(); t_last=t0
interval=1.0/a.rate
payload=a.size-12
if payload<0: print("size must be >=12"); sys.exit(1)
print(f"Sending to {addr} size={a.size}B rate={a.rate}pps for {a.secs}s")
while time.time()-t0 < a.secs:
    t_send=time.time()
    pkt=struct.pack("!Iq",seq,int(t_send*1e6))+os.urandom(payload)
    sock.sendto(pkt,addr); sent+=1
    try:
        data,_=sock.recvfrom(2048)
        if len(data)>=12:
            rseq, ts = struct.unpack("!Iq", data[:12])
            rtt=(time.time()-t_send)*1000.0
            ack+=1
        else:
            lost+=1
    except socket.timeout:
        lost+=1
    # once per second, print summary
    now=time.time()
    if now-t_last>=1.0:
        dur=now-t0
        thr_kbps=(ack*a.size*8)/dur/1000.0
        loss=100.0*(lost)/(sent) if sent else 0.0
        print(f"t={dur:4.1f}s sent={sent} ack={ack} lost={lost} loss={loss:4.1f}% thr={thr_kbps:6.1f} kbps")
        t_last=now
    seq+=1
    # pacing
    sleep=max(0.0, interval-(time.time()-t_send))
    if sleep>0: time.sleep(sleep)
dur=time.time()-t0
thr_kbps=(ack*a.size*8)/dur/1000.0
loss=100.0*(lost)/(sent) if sent else 0.0
print(f"FINAL: dur={dur:.1f}s sent={sent} ack={ack} lost={lost} loss={loss:.1f}% thr={thr_kbps:.1f} kbps")