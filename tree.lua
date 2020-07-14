function print_tree(var, depth)
    print(var)
    local bitmap = {}
    function print_tree_i(var, depth)
        if type(var) ~= "table" then
            print("not a table");
            return
        end 
    
        local depth = depth or 0
        local tab = string.rep("        ", depth);
        depth = depth  + 1 

        if depth >= 4 then
            return
        end 

        bitmap[var] = true
        for k,v in pairs(var) do
            if type(v) ~= "table" then
                print(string.format("%s%-7s %s", tab, tostring(k), tostring(v)))
            else
                if not bitmap[v] then
                    print(string.format("%s%s(%s)", tab, tostring(k), v)) 
                    print_tree_i(v, depth)
                else
                    print(string.format("%s%-7s %s+", tab, tostring(k), v)) 
                end 
            end 
        end 
        return
    end 

    print_tree_i(var, depth)
end

function print_metatable(tab)
    if type(getmetatable(tab)) ~= "table" then
        print("has no metatable");
        return
    end 

    print_tree(getmetatable(tab));
end
