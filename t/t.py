import redis
import _thread
import time

SVR_IP = '172.16.185.133'
SVR_IP = '172.16.185.155'
SVR_IP = '127.0.0.1'

SVR_PORT = 6379

def pfunc():
    rd = redis.Redis(host=SVR_IP, port=SVR_PORT, db=0)
    f = open('./src-code.txt', 'r')

    # set
    i = 0;
    for line in f:
        rd.set('%d' % i, line.rstrip())
        i += 1

    # setnx
    f.seek(0)
    i = 0;
    for line in f:
        rd.setnx('%d' % i, line.rstrip())
        i += 1

    # exists
    i = 0
    for i in range(100):
        rd.exists('%d' % i)
        i += 1

    # get
    i = 0
    for i in range(100):
        rd.get('%d' % i)
        i += 1

    # mset
    f.seek(0)
    i = 0
    for line in f:
        k1, k2 = '%d' % i, '%d' % (i+1)
        v1, v2 = line.rstrip(), line.rstrip()
        rd.mset(**{k1:v1, k2:v2})
        i += 1

    # mget
    i = 0
    for i in range(100):
        k1, k2 = '%d' % i, '%d' % (i+1)
        rd.mget(k1, k2)
        i += 1

    f.seek(0)
    for line in f:
        # lpush
        rd.lpush('ltop', line.rstrip())
        # rpush
        rd.rpush('rtop', line.rstrip())

    # llen
    rd.llen('ltop')
    rd.llen('rtop')

    i = 0
    for i in range(100):
        # lpop
        rd.lpop('ltop')
        # rpop
        rd.rpop('rtop')

    # hset
    i = 0
    f.seek(0)
    for line in f:
        rd.hset('sub', '%d' % i, line.rstrip())
        i += 1

    # hget
    i = 0
    for i in range(100):
        rd.hget('sub', '%d' % i)
        i += 1

    # delete
    i = 0
    for i in range(100):
        rd.delete('%d' % i)

    # save
    rd.save()

    # select
    # ??? how to select db

    # flush
    rd.flushdb()

    # flushall
    rd.flushall()

    f.close()

start = time.time()
pfunc()
end = time.time()
print(end - start)

'''
for i in range(1, 2):
    thread.start_new_thread(pfunc, ())

while (1):
    time.sleep(1)
'''
