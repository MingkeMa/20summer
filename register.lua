lua_scripts={}
dir="scripts/"

lua_scripts[1]=dir.."calc_login_dur.lua"
lua_scripts[2]=dir.."select_team.lua"

redis = require 'redis'
client = redis.connect('127.0.0.1', 6379)
response = client:ping()
if(not response)
then
    print("Error! Can't connect to Redis")
end