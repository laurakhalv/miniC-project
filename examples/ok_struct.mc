struct Point {
    int32 x,
    int32 y
}

func main() -> int32 {
    let Point p = Point { x: 10, y: 20 };
    return p.x;
}
