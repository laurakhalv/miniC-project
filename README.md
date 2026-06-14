# MiniC

MiniC — небольшой учебный статически типизированный язык программирования и
компилятор для него, реализованный на C++23.

Компилятор организован как классический pipeline:

```text
Lexer -> Parser -> Semantic -> Codegen
```

Проект содержит:

- `src/` — исходный код компилятора
- `runtime/` — внешний runtime для служебных функций backend
- `specs/` — спецификация языка
- `examples/` — примеры программ на MiniC

## Сборка

Из корня проекта:

```bash
cmake -S . -B build
cmake --build build
```

После сборки получается:

```text
build/minic
```

## Использование

Базовый запуск:

```bash
./build/minic <source-file> [-o <output-file>] [--dump-tokens|--dump-ast|--emit-asm|--time-phases]
```

Поддерживаемые флаги:

- `--dump-tokens` — вывести поток токенов и завершиться
- `--dump-ast` — вывести AST и завершиться
- `--emit-asm` — остановиться на выводе `x86-64` assembly
- `--time-phases` — вывести время фаз компиляции

Примеры запуска:

```bash
./build/minic examples/semantic_ok.mc --dump-tokens
./build/minic examples/semantic_ok.mc --dump-ast
./build/minic examples/semantic_ok.mc
./build/minic examples/codegen_aggregate.mc
./build/minic examples/pipe_ok.mc --emit-asm --time-phases -o examples/pipe_ok.s
```

Если доступен `x86-64` toolchain, компилятор по умолчанию собирает готовый
`x86-64` executable. Если toolchain недоступен, он сохраняет `x86-64` assembly (`.s`).

## Примеры

- `examples/semantic_ok.mc` — корректный пример полного pipeline
- `examples/codegen_aggregate.mc` — массивы и структуры в codegen
- `examples/codegen_aggregate_call.mc` — aggregate-параметры и aggregate-return
- `examples/char_ok.mc` — тип `char`, символьные литералы и `cast<string>(char)`
- `examples/literals_ok.mc` — `0x`, `0b`, exponent float, `inf`, `NaN`
- `examples/assert_ok.mc` — builtin `assert`, `len(array)` и array equality
- `examples/aggregate_eq_ok.mc` — equality для структур и массивов
- `examples/float_ok.mc` — floating-point арифметика, печать и cast
- `examples/block_comment_ok.mc` — вложенные блочные комментарии
- `examples/bitwise_ok.mc` — побитовые операции и сдвиги
- `examples/pipe_ok.mc` — конвейерный оператор `|>`
- `examples/if_expr_ok.mc` — выражение `if`
- `examples/methods_visibility_ok.mc` — методы структур и `public/private`
- `examples/overload_named_default_ok.mc` — перегрузка функций, named args и default args
- `examples/operator_overload_ok.mc` — перегрузка операторов для структур
- `examples/module_import_ok.mc` — импорт модуля целиком
- `examples/module_specific_import_ok.mc` — импорт отдельного экспортированного объявления
- `examples/module_private_import_bad.mc` — ошибка импорта `private`-объявления модуля
- `examples/module_private_access_bad.mc` — скрытое `private`-объявление недоступно через импорт модуля
- `examples/graph_traversal_bfs_dfs.mc` — бонусная задача из пула: обход графа в ширину и глубину
- `examples/private_method_bad.mc` — ошибка доступа к `private`-методу
- `examples/lexer_bad.mc` — лексическая ошибка
- `examples/parser_bad.mc` — синтаксическая ошибка
- `examples/semantic_bad_if.mc` — семантическая ошибка типа
- `examples/semantic_bad_const.mc` — ошибка присваивания в `const`
- `examples/semantic_bad_return.mc` — ошибка control-flow

## Текущее состояние

Сейчас компилятор уже поддерживает:

- лексический анализ
- recursive descent parsing
- semantic analysis
- генерацию x86-64 assembly для integer/bool/string/float кода
- `char` и символьные литералы
- локальные массивы и структуры с value-copy semantics
- builtin `input`, `assert`
- aggregate-параметры функций и aggregate-return
- `len(array)` и equality для массивов/структур
- `cast<string>(numeric-or-bool-or-char)`
- вложенные блочные комментарии `/* ... */`
- расширенные диагностические сообщения с показом строки и `^`
- дополнительные CLI-флаги `--emit-asm` и `--time-phases`
- побитовые операции `& | ^ ~ << >>`
- конвейерный оператор `|>`
- выражение `if (cond) expr1 else expr2`
- методы структур
- секции видимости `public:` и `private:`
- перегрузку функций
- параметры по умолчанию и именованные аргументы
- перегрузку операторов для пользовательских типов
- модули с `module`/`import`
- контроль видимости модулей через `export` / `private`
- импорт отдельных экспортированных объявлений `import Module::name`

Главное оставшееся практическое ограничение:

- финальная сборка готового executable зависит от host toolchain; генерация assembly переносимее

## Спецификация

Спецификация языка находится в:

- `specs/grammar.md`
- `specs/semantics.md`
- `specs/types.md`
- `specs/codegen.md`
