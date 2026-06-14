struct Vec {
    public:
    int32 x,
    int32 y
    func operator +(Vec other) -> Vec {
        return Vec { x: self.x + other.x, y: self.y + other.y };
    }
    func operator -() -> Vec {
        return Vec { x: -self.x, y: -self.y };
    }
    func operator ==(Vec other) -> bool {
        return self.x == other.x && self.y == other.y;
    }
}
func main() -> int32 {
    let Vec a = Vec { x: 1, y: 2 };
    let Vec b = Vec { x: 10, y: 20 };
    let Vec c = a + b;
    let Vec d = -a;
    print(c.x);
    print("\n");
    print(c.y);
    print("\n");
    print(d.x);
    print("\n");
    print(d.y);
    print("\n");
    print(a == a);
    print("\n");
    return 0;
}