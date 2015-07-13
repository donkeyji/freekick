import redis
import thread
import time

SVR_IP = '172.16.185.133'
SVR_IP = '172.16.185.155'
SVR_IP = '127.0.0.1'

SVR_PORT = 6379

def pfunc():
	rd = redis.Redis(host=SVR_IP, port=SVR_PORT, db=0)

	# set
	i = 0;
	f = file('./src-code.txt', 'r')
	for l in f:
		rd.set('%d' % i, l.rstrip())
		i += 1

	# get
	i = 0
	for i in range(1000):
		rd.get('%d' % i)
		i += 1

	# lpush
	f.seek(0)
	for l in f:
		rd.lpush('top', l.rstrip())

	# rpop
	for i in range(100):
		rd.rpop('top')

	# hset
	i = 0
	f.seek(0)
	for l in f:
		rd.hset('sub', '%d' % i, l.rstrip())
		i += 1

	# hget
	i = 0
	for i in range(1000):
		rd.hget('sub', '%d' % i)
		i += 1

	# delete
	i = 0
	for i in range(1000):
		rd.delete('%d' % i)

	# save
	rd.save()

	f.close()

s = time.time()
pfunc()
e = time.time()
print e - s

'''
for i in range(1, 2):
	thread.start_new_thread(pfunc, ())

while (1):
	time.sleep(1)
'''
