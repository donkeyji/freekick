function print_tbl(tbl)
    for k, v in pairs(tbl) do
        print('key: ', k, 'value: ', v)
    end
end

local k1 = KEYS[1]
local k2 = KEYS[2]
local v1 = ARGV[1]
local v2 = ARGV[2]

local s1 = redis.pcall('set', k1, v1)
local s2 = redis.pcall('set', k2, v2)
print(type(s1), s1)
print_tbl(s1)
print(type(s2), s2)
print_tbl(s2)

local g1 = redis.pcall('get', k1)
local g2 = redis.pcall('get', k2)
print('g1 g2', g1, g2)
return {g1, g2}

--local r = redis.pcall('set', k, v)
--print('set return', type(r), r)

--local t = redis.pcall('mget', k1, k2)
--print('get return', type(t), t)

--for m,n in pairs(t) do
    --print('==KV:', m, n)
--end
	
--values returned to C
--return {1, 2, 3, 4}
