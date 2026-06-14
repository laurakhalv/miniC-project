struct Box {
public:
    int32 value,
    func add(int32 delta) -> int32 {
        return self.value + delta;
    }
private:
    func hidden() -> int32 {
        return self.value + 1;
    }
public:
    func useHidden() -> int32 {
        return self.hidden();
    }
}
func main() -> int32 {
    let Box box = Box { value: 10 };
    print(box.add(5));
    print("\n");
    print(box.useHidden());
    print("\n");
    return 0;
}