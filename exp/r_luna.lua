do
    local car = Car();
    print_tree(car)
    print_metatable(car)
    print("")
    car.print();
    car.setLength(11);
    car.setWidth(22);
    car.print();
    print(car.getLength(), car.getWidth(), car.x, car.y);
end
collectgarbage("collect");
