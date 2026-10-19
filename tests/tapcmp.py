import sys
def recs(fn):
    b=open(fn,'rb').read(); i=0; out=[]
    while i+4<=len(b):
        n=int.from_bytes(b[i:i+4],'little'); i+=4
        if n==0: out.append('TM'); continue
        if n==0xffffffff: break
        n&=0xffffff; out.append(bytes(x&63 for x in b[i:i+n])); i+=n+(n&1)+4
    return out
sys.exit(0 if recs(sys.argv[1])==recs(sys.argv[2]) else 1)
