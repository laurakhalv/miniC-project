func square(int32 x) -> int32 {
    return x * x;
}
func main() -> int32 {
    let int32 x = 4;
    let int32 y = square(x);
    if (y > 0) {
        print("ok\n");
    }
    return y;
}