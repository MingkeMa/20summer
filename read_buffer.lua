print("doing read_buffer")
need_query=1
do
    local buffer = lunar.Buffer();
    buffer.setBuffer(buffer_pointer)
    request_type=buffer.getS(2)
    if(request_type==1)
    then
        query_num=2
        local upper_uid=buffer.getL(4)
        local lower_uid=buffer.getL(8)
        local month=buffer.getS(12)
        local uid= (upper_uid << 32) | lower_uid
        -- print("uid is "..uid..", month is "..month)
        local check_dur=client:ttl(tostring(uid)..':logindur:'..tostring(month))
        local check_cnt=client:ttl(tostring(uid)..':logincnt:'..tostring(month))
        if((check_dur==-1 or check_dur>10) and (check_cnt==-1 or check_cnt>10))
        then
            need_query=-1
        else
            padding = (month < 10) and "0" or ""
            date= "'2020-" .. padding .. month .. "-01'";
            query1 = "SELECT dtEventTime FROM PlayerLogin WHERE (UID = " .. uid .. ") AND (dtEventTime BETWEEN " .. date ..
                    " AND LAST_DAY(" .. date .. "))"  --use BETWEEN to allow mysql to use index
            query2 = "SELECT dtEventTime FROM PlayerLogout WHERE (UID = " .. uid .. ") AND (dtEventTime BETWEEN " .. date ..
                    " AND LAST_DAY(" .. date .. "))" 
        end
    elseif(request_type==2)
    then
        if(client:ttl("request2")==-1 or client:ttl("request2")>10)
        then
            need_query=-1
        else
            query_num=1
            query1="SELECT TeamID, dtEventTime, TeamKill, TeamRank FROM (GameRecord INNER JOIN MatchRegister USING (OpenID1,OpenID2,OpenID3,OpenID4))"
        end
    else

    end
end
