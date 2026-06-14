func hasEdge(int32[36] graph, int32 nodeCount, int32 from, int32 to) -> bool {
    return graph[from * nodeCount + to] != 0;
}

func bfs(int32[36] graph, int32 nodeCount, int32 start) -> void {
    let int32[6] visited = [0, 0, 0, 0, 0, 0];
    let int32[6] queue = [0, 0, 0, 0, 0, 0];
    let int32 head = 0;
    let int32 tail = 0;

    visited[start] = 1;
    queue[tail] = start;
    tail = tail + 1;

    print("BFS:\n");

    while (head < tail) {
        let int32 current = queue[head];
        head = head + 1;

        print(current);
        print("\n");

        let int32 next = 0;
        while (next < nodeCount) {
            if (hasEdge(graph, nodeCount, current, next) && visited[next] == 0) {
                visited[next] = 1;
                queue[tail] = next;
                tail = tail + 1;
            }
            next = next + 1;
        }
    }

    return;
}

func dfs(int32[36] graph, int32 nodeCount, int32 start) -> void {
    let int32[6] visited = [0, 0, 0, 0, 0, 0];
    let int32[6] stack = [0, 0, 0, 0, 0, 0];
    let int32 size = 0;

    stack[size] = start;
    size = size + 1;

    print("DFS:\n");

    while (size > 0) {
        let int32 current = stack[size - 1];
        size = size - 1;

        if (visited[current] == 0) {
            visited[current] = 1;

            print(current);
            print("\n");

            let int32 next = nodeCount - 1;
            while (next >= 0) {
                if (hasEdge(graph, nodeCount, current, next) && visited[next] == 0) {
                    stack[size] = next;
                    size = size + 1;
                }
                next = next - 1;
            }
        }
    }

    return;
}

func main() -> int32 {
    let int32[36] graph = [
        0, 1, 1, 0, 0, 0,
        1, 0, 0, 1, 1, 0,
        1, 0, 0, 0, 1, 0,
        0, 1, 0, 0, 0, 1,
        0, 1, 1, 0, 0, 1,
        0, 0, 0, 1, 1, 0
    ];

    bfs(graph, 6, 0);
    print("---\n");
    dfs(graph, 6, 0);

    return 0;
}
