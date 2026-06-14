func sum(int32 a, int32 b = 1, int32 c = 2) -> int32 {
    return a + b + c;
}
func pick(int32 x) -> int32 {
    return x + 10;
}
func pick(bool x) -> int32 {
    if (x) {
        return 1;
    } else {
        return 0;
    }
}
func main() -> int32 {
    print(sum(5));
    print("\n");
    print(sum(5, c = 7));
    print("\n");
    print(sum(a = 5, c = 7));
    print("\n");
    print(pick(3));
    print("\n");
    print(pick(false));
    print("\n");
    return 0;
}