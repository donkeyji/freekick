local k = KEYS[1]
local v = ARGV[1]
local r = redis.pcall('set', k, v)
local t = redis.pcall('get', k)
for m,n in pairs(t) do
	print('==KV:', m, n)
end
	
return {1, 2, 3, 4}
