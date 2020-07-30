print("doing calc_login_dur")
-- print(res0)
-- print(res1)
mylib=require("mylib")
res0=mylib.split(res0,"|")
res1=mylib.split(res1,"|")
i,j=1,1
cnt,dur=0,0
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
local buffer = lunar.Buffer();
buffer.setBuffer(buffer_pointer)
buffer.setS(cnt,2)
buffer.setL(dur,4)
msg_len=8