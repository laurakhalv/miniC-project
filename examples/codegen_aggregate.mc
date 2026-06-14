struct Pair {
    int32 left,
    int32 right
}

func main() -> int32 {
    let Pair pair = Pair { left: 1, right: 2 };
    let Pair copy = pair;
    copy.left = copy.left + pair.right;

    let Pair[2] pairs = [
        Pair { left: 10, right: 20 },
        Pair { left: 30, right: 40 }
    ];

    pairs[1].left = pairs[0].right + copy.left;

    print("result=");
    print(pairs[1].left);
    return pairs[1].left;
}
