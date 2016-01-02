local k = KEYS[1]
local v = ARGV[1]
local r = redis.pcall('set', k, v)
print(r)
local r = redis.pcall('get', k)
print(r)
for k in pairs(r) do
	print(k, r[k])
end
	
return {1, 2, 3, 4}
