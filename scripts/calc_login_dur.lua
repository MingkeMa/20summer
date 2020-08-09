print("doing calc_login_dur")
-- print(res0)
-- print(res1)
local buffer = lunar.Buffer();
buffer.setBuffer(buffer_pointer)
local upper_uid=buffer.getL(4)
local lower_uid=buffer.getL(8)
local month=buffer.getS(12)
local uid= (upper_uid << 32) | lower_uid
local key_dur=tostring(uid)..':logindur:'..tostring(month)
local key_cnt=tostring(uid)..':logincnt:'..tostring(month)
if(client:exists(key_dur) and client:exists(key_cnt))
then
    buffer.setS(client:get(key_cnt),2)
    buffer.setL(client:get(key_dur),4)
else
    mylib=require("mylib")
    res0=mylib.split(res0,"|")
    res1=mylib.split(res1,"|")
    local i,j=1,1
    local cnt,dur=0,0
    while (i<=#res0 and j<=#res1)
    do
        if (mylib.time_gap(res0[i],res1[j])<=0)
        then
            j=j+1
        else
            dur=dur+mylib.time_gap(res0[i],res1[j])
            i=i+1
            j=j+1
            cnt=cnt+1
        end
    end
    buffer.setS(cnt,2)
    buffer.setL(dur,4)
    client:set(key_dur,dur)
    client:set(key_cnt,cnt)
    if(os.date("*t").month==month)
    then
        client:expire(key_dur, 1000)
        client:expire(key_cnt, 1000)
    end
    -- client:expire(key_cnt, -1)
end
msg_len=8