do
    print_tree(lunar)
    print("")
    local mobile = lunar.Mobile();
    print_tree(mobile)
    print_metatable(mobile)
    
    print("")
    print(mobile.version, mobile.price);
    mobile.version = 101
    mobile.price = 4888
    mobile.y = 100
    print(mobile.version, mobile.price)
    print(mobile.x, mobile.y);
    print(mobile.getVersion, mobile.getPrice)
    mobile.print();
    
    print("")
    local mobile1 = lunar.Mobile();
    mobile1.setVersion(201);
    mobile1.setPrice(5888);
    mobile1.print();
    print(mobile1.getVersion(), mobile1.getPrice(), mobile1.version, mobile1.price);
end
collectgarbage("collect");
