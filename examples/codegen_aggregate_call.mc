struct Pair {
    int32 left,
    int32 right
}

func bump(Pair value) -> Pair {
    let Pair copy = value;
    copy.left = copy.left + 1;
    copy.right = copy.right + 1;
    return copy;
}

func sum_pair(Pair value) -> int32 {
    return value.left + value.right;
}

func main() -> int32 {
    let Pair pair = Pair { left: 10, right: 20 };
    let Pair bumped = bump(pair);
    return sum_pair(bumped);
}
