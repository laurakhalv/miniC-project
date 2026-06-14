func printNumberTriangle(int32 n) -> void {
    let int32 current = 1;
    let int32 row = 1;
    while (row <= n) {
        let int32 count = 1;
        print(current);
        current = current + 1;
        while (count < row) {
            print(" ");
            print(current);
            current = current + 1;
            count = count + 1;
        }
        print("\n");
        row = row + 1;
    }
    return;
}
func main() -> int32 {
    printNumberTriangle(4);
    return 0;
}