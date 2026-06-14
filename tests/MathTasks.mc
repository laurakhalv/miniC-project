module MathTasks;

export func square(int32 x) -> int32 {
    return x * x;
}

export func sumTo(int32 n) -> int32 {
    let int32 total = 0;
    let int32 value = 1;

    while (value <= n) {
        total = total + value;
        value = value + 1;
    }

    return total;
}

private func hidden(int32 x) -> int32 {
    return x + 100;
}