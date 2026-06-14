struct Date {
    int32 day,
    int32 month,
    int32 year
}

func isLeapYear(int32 year) -> bool {
    if (year % 400 == 0) {
        return true;
    } else {
        if (year % 100 == 0) {
            return false;
        } else {
            if (year % 4 == 0) {
                return true;
            } else {
                return false;
            }
        }
    }
}

func daysInMonth(int32 month, int32 year) -> int32 {
    if (month == 1 || month == 3 || month == 5 || month == 7 || month == 8 || month == 10 ||
        month == 12) {
        return 31;
    } else {
        if (month == 4 || month == 6 || month == 9 || month == 11) {
            return 30;
        } else {
            if (month == 2) {
                if (isLeapYear(year)) {
                    return 29;
                } else {
                    return 28;
                }
            } else {
                return 0;
            }
        }
    }
}

func isValidDate(int32 day, int32 month, int32 year) -> int32 {
    if (year < 1) {
        return 0;
    } else {
        if (month < 1 || month > 12) {
            return 0;
        } else {
            let int32 limit = daysInMonth(month, year);

            if (day < 1 || day > limit) {
                return 0;
            } else {
                return 1;
            }
        }
    }
}

func nextDay(int32 day, int32 month, int32 year) -> Date {
    if (day < daysInMonth(month, year)) {
        return Date { day: day + 1, month: month, year: year };
    } else {
        if (month < 12) {
            return Date { day: 1, month: month + 1, year: year };
        } else {
            return Date { day: 1, month: 1, year: year + 1 };
        }
    }
}

func printDate(Date value) -> void {
    print(value.day);
    print(".");
    print(value.month);
    print(".");
    print(value.year);
    print("\n");
    return;
}

func main() -> int32 {
    print(isValidDate(29, 2, 2024));
    print("\n");

    print(isValidDate(29, 2, 2023));
    print("\n");

    print(isValidDate(31, 4, 2025));
    print("\n");

    print(isValidDate(31, 12, 2025));
    print("\n");

    printDate(nextDay(28, 2, 2024));
    printDate(nextDay(28, 2, 2023));
    printDate(nextDay(31, 12, 2025));

    return 0;
}
