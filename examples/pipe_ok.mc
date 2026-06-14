func square(int32 value) -> int32 {
    return value * value;
}
func add(int32 left, int32 right) -> int32 {
    return left + right;
}
func main() -> int32 {
    let int32 result = 3 |> square |> add(4);
    print(result);
    print("\n");
    return result;
}