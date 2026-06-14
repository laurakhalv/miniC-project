func main() -> int32 {
    let uint32 left = 0xF0;
    let uint32 right = 0xCC;
    let uint32 and_value = left & right;
    let uint32 or_value = left | right;
    let uint32 xor_value = left ^ right;
    let uint32 shift_left = and_value << 2;
    let uint32 shift_right = or_value >> 3;
    let uint32 inverted = ~xor_value;
    print(and_value);
    print("\n");
    print(or_value);
    print("\n");
    print(xor_value);
    print("\n");
    print(shift_left);
    print("\n");
    print(shift_right);
    print("\n");
    print(inverted);
    print("\n");
    return 0;
}