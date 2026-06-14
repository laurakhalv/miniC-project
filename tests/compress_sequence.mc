func compressSequence(int32[10] arr, int32 length) -> void {
    if (length == 0) {
        print("(empty)\n");
        return;
    } else {
        let int32 index = 0;

        while (index < length) {
            let int32 value = arr[index];
            let int32 count = 1;

            while (index + count < length && arr[index + count] == value) {
                count = count + 1;
            }

            print("(");
            print(value);
            print(",");
            print(count);
            print(")");

            index = index + count;
            if (index < length) {
                print(" ");
            }
        }

        print("\n");
        return;
    }
}

func main() -> int32 {
    let int32[10] sample = [1, 1, 1, 2, 3, 3, 4, 4, 4, 4];
    let int32[10] emptySample = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    let int32[10] singleSample = [7, 0, 0, 0, 0, 0, 0, 0, 0, 0];

    compressSequence(sample, 10);
    compressSequence(emptySample, 0);
    compressSequence(singleSample, 1);

    return 0;
}
