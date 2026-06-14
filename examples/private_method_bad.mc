struct SecretBox {
private:
    func hidden() -> int32 {
        return 42;
    }
}
func main() -> int32 {
    let SecretBox box = SecretBox {};
    print(box.hidden());
    return 0;
}