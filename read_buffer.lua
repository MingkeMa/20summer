print("doing read_buffer")
do
    local buffer = lunar.Buffer();
    buffer.setBuffer(buffer_pointer)
    type=buffer.getS(2)
    if(type==1)
    then
        query_num=2
        local upper_uid=buffer.getL(4)
        local lower_uid=buffer.getL(8)
        local month=buffer.getS(12)
        local uid= (upper_uid << 32) | lower_uid
        print("uid is "..uid..", month is "..month)
        padding = (month < 10) and "0" or ""
        date= "'2020-" .. padding .. month .. "-01'";
        query1 = "SELECT dtEventTime FROM PlayerLogin WHERE (UID = " .. uid .. ") AND (dtEventTime BETWEEN " .. date ..
                " AND LAST_DAY(" .. date .. "))"  --use BETWEEN to allow mysql to use index
        query2 = "SELECT dtEventTime FROM PlayerLogout WHERE (UID = " .. uid .. ") AND (dtEventTime BETWEEN " .. date ..
                " AND LAST_DAY(" .. date .. "))" 
    elseif(type==2)
    then
        
    else

    end
end
