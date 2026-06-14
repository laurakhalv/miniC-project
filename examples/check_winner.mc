func lineWinner(char[9] board, int32 a, int32 b, int32 c) -> int32 {
    if (board[a] == 'X' && board[b] == 'X' && board[c] == 'X') {
        return 1;
    } else {
        if (board[a] == 'O' && board[b] == 'O' && board[c] == 'O') {
            return 2;
        } else {
            return 0;
        }
    }
}

func checkWinner(char[9] board) -> int32 {
    let int32 winner = lineWinner(board, 0, 1, 2);
    if (winner != 0) {
        return winner;
    }
    winner = lineWinner(board, 3, 4, 5);
    if (winner != 0) {
        return winner;
    }

    winner = lineWinner(board, 6, 7, 8);
    if (winner != 0) {
        return winner;
    }
    winner = lineWinner(board, 0, 3, 6);
    if (winner != 0) {
        return winner;
    }

    winner = lineWinner(board, 1, 4, 7);
    if (winner != 0) {
        return winner;
    }

    winner = lineWinner(board, 2, 5, 8);
    if (winner != 0) {
        return winner;
    }

    winner = lineWinner(board, 0, 4, 8);
    if (winner != 0) {
        return winner;
    }

    return lineWinner(board, 2, 4, 6);
}

func main() -> int32 {
    let char[9] xWins = ['X', 'X', 'X', '.', 'O', '.', 'O', '.', '.'];
    let char[9] oWins = ['X', 'O', '.', 'X', 'O', '.', '.', 'O', 'X'];
    let char[9] noWinner = ['X', 'O', 'X', 'X', 'O', 'O', 'O', 'X', 'X'];

    print(checkWinner(xWins));
    print("\n");

    print(checkWinner(oWins));
    print("\n");

    print(checkWinner(noWinner));
    print("\n");

    return 0;
}
