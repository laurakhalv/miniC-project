func scale(float64 x, float64 factor) -> float64 {
    return x * factor;
}

func main() -> int32 {
    let float32 small = 1.5;
    let float64 wide = cast<float64>(small);
    let float64 scaled = scale(wide + 0.5, 2.0);
    let float64 rem = scaled % 2.5;

    print(scaled);
    print("\n");
    print(rem);
    print("\n");

    let string text = cast<string>(scaled);
    print(text);
    print("\n");
    print(cast<string>(true));
    print("\n");

    if (scaled > 3.0) {
        return 0;
    }

    return 1;
}
