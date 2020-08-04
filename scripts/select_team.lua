print("doing select_team")
-- print(res0)
mylib=require("mylib")
res0=mylib.split(res0,"|")
--use hashmap to store all games of each team
local teams={}
for i,n in ipairs(res0) 
do
    res0[i]=mylib.split(res0[i],"&")
    if teams[res0[i][1]]==null
    then
        teams[res0[i][1]]={}
    end
    table.insert(teams[res0[i][1]],{res0[i][2],res0[i][3],res0[i][4]})
end

--comparator, sort using timestamp, desc. reused when sort using score
local function my_comparator1(a, b)
    return a[1] > b[1]     
end
--scores for different ranks
local rank2score={}
for i = 1,25
do
    local score
    if (i==1) then score=20
    elseif (i==2) then score=15
    elseif (i==3) then score=12
    elseif (i==4) then score=10
    elseif (i==5) then score=8
    elseif (i==6) then score=6
    elseif (i==7) then score=5
    elseif (i==8) then score=4
    elseif (i<=11) then score=3
    elseif (i<=15) then score=2
    elseif (i<=20) then score=1
    else score=0 end
    rank2score[i]=score
    rank2score[tostring(i)]=score
end
 --comparator, sort using score, desc
local function my_comparator2(a, b)
    return a[1]+rank2score[a[2]] > b[1]+rank2score[b[2]]
end

local teamScore={}

mylib.pq_pthread(teams)
-- print_tree(teams)
for k,v in pairs(teams)
do
    --get the most recent 30 games for each team
    -- table.sort(v,my_comparator1)
    local games={}
    for i = 1,30
    do
        table.insert(games,{v[i][2],v[i][3]})
    end
    --get 20 games with the highest scores
    table.sort(games,my_comparator2)
    --calc avg score
    local score=0
    for i = 1,20
    do
        score=score+games[i][1]+rank2score[games[i][2]]
    end
    --store the avg score of each teams
    table.insert(teamScore,{score/20,k})
end
table.sort(teamScore,my_comparator1)
local buffer = lunar.Buffer()
buffer.setBuffer(buffer_pointer)
for i = 1,20
do
    buffer.setL(teamScore[i][2],2+4*(i-1))
end
msg_len=2+4*20
