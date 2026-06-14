func isPrime(int32 n) -> bool {
    if (n < 2) {
        return false;
    } else {
        if (n == 2) {
            return true;
        } else {
            if (n % 2 == 0) {
                return false;
            } else {
                let int32 divisor = 3;
                while (divisor * divisor <= n) {
                    if (n % divisor == 0) {
                        return false;
                    }
                    divisor = divisor + 2;
                }
                return true;
            }
        } 
    }      
} 
func classifyNumber(int32 n) -> void {
    if (n % 2 ==  0 && n > 100) {
        print("BigEven\n");
    } else {
        if (n % 2 == 0 && n >= 10 && n <= 100) {
            print("MediumEven\n");
        } else {
            if (n % 2 == 0 && n < 10) {
                print("SmallEven\n");
            } else {
                if (n % 2 != 0 && n >= 100 && n <= 999) {
                    print("OddThreeDigit\n");
                } else {
                    if (n % 2 != 0 && n < 0) {
                        print("NegativeOdd\n");
                    } else {
                        if (n % 2 != 0 && isPrime(n)) {
                            print("PrimeOdd\n");
                        } else {
                            print("Other\n");
                        }
                    }
                }
            }
        }
    }
    return;
}
func main() -> int32 {
    classifyNumber(208);
    classifyNumber(44);
    classifyNumber(6);
    classifyNumber(333);
    classifyNumber(-7);
    classifyNumber(17);
    classifyNumber(9);
    return 0;
}

