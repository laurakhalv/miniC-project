func main() -> int32 {
    let int32 x = if (true) 10 else 20;
    let int32 y = if (x > 5) x + 1 else x - 1;
    print(y);
    print("\n");
    return 0;
}