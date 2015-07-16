import redis
import thread
import time

SVR_IP = '172.16.185.133'
SVR_IP = '172.16.185.155'
SVR_IP = '127.0.0.1'

SVR_PORT = 6379

def pfunc():
	rd = redis.Redis(host=SVR_IP, port=SVR_PORT, db=0)
	f = file('./src-code.txt', 'r')

	# set
	i = 0;
	for line in f:
		rd.set('%d' % i, line.rstrip())
		i += 1

	# setnx
	f.seek(0)
	i = 0;
	for line in f:
		rd.set('%d' % i, line.rstrip())
		i += 1

	# get
	i = 0
	for i in range(100):
		rd.get('%d' % i)
		i += 1

	# lpush
	f.seek(0)
	for line in f:
		rd.lpush('ltop', line.rstrip())

	# lpop
	i = 0
	for i in range(100):
		rd.lpop('ltop')

	# rpush
	f.seek(0)
	for line in f:
		rd.rpush('rtop', line.rstrip())

	# rpop
	i = 0
	for i in range(100):
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

	f.close()

start = time.time()
pfunc()
end = time.time()
print end - start

'''
for i in range(1, 2):
	thread.start_new_thread(pfunc, ())

while (1):
	time.sleep(1)
'''
