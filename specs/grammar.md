# Грамматика языка MiniC

MiniC — упрощённый статически типизированный язык программирования в стиле C.
Язык использует явные объявления, value semantics и компиляцию в x86-64.

---

## 1. Лексика

### 1.1 Алфавит

Язык использует кодировку ASCII.
Пробелы, табуляции и переводы строк являются разделителями и игнорируются вне литералов.

### 1.2 Ключевые слова

Следующие слова зарезервированы и не могут использоваться как идентификаторы:

```text
let      const     func      struct    type      namespace
if       else      while     break     continue  return
true     false     cast      public    private   operator
```

### 1.3 Встроенные типы

Следующие имена зарезервированы как встроенные типы:

```text
int8    int16    int32    int64
uint8   uint16   uint32   uint64
float32 float64
bool    char     string   void
```

### 1.4 Идентификаторы

```ebnf
IDENTIFIER ::= [a-zA-Z_] [a-zA-Z0-9_]*
```

Идентификатор не должен совпадать ни с ключевым словом, ни с именем встроенного типа.

### 1.5 Литералы

```ebnf
INT_LITERAL    ::= [0-9]+ | "0x" HEX_DIGIT+ | "0b" BINARY_DIGIT+
FLOAT_LITERAL  ::= ([0-9]+ "." [0-9]+ ([eE] [+-]? [0-9]+)?)
                 | ([0-9]+ [eE] [+-]? [0-9]+)
                 | "inf"
                 | "NaN"
BOOL_LITERAL   ::= "true" | "false"
CHAR_LITERAL   ::= "'" ( [^'\\] | '\\' CHAR_ESCAPE_CHAR ) "'"
STRING_LITERAL ::= '"' ( [^"\\] | '\\' ESCAPE_CHAR )* '"'
ESCAPE_CHAR    ::= '"' | '\\' | 'n' | 't'
CHAR_ESCAPE_CHAR ::= "'" | '"' | '\\' | 'n' | 't'
HEX_DIGIT      ::= [0-9a-fA-F]
BINARY_DIGIT   ::= "0" | "1"
```

Поддерживаемые escape-последовательности:

- `\\`
- `\"`
- `\'`
- `\n`
- `\t`

Любая другая escape-последовательность является лексической ошибкой.

### 1.6 Операторы и разделители

```text
+    -    *    /    %
=    ==   !=   <    >    <=   >=
&&   ||   !    ~
&    |    ^    <<   >>   |>
(    )    {    }    [    ]
;    ,    .    :    ::   ->
```

### 1.7 Комментарии

Поддерживаются два вида комментариев:

- однострочный комментарий `// ...` до конца строки;
- блочный комментарий `/* ... */`.

Блочные комментарии могут быть вложенными:

```minic
/* внешний
   /* внутренний */
*/
```

Комментарии не попадают в поток токенов и не влияют на семантику программы.

---

## 2. Синтаксис

### 2.1 Модуль и структура программы

Каждый исходный файл языка MiniC является отдельным модулем.

```ebnf
Program ::= Declaration* EOF
```

- На верхнем уровне модуля допускаются только объявления.
- Выражения и инструкции допускаются только внутри тел функций.
- В данной версии языка глобальные переменные не поддерживаются.
- Программа обязана содержать функцию `main` с сигнатурой `func main() -> int32`.

Пример минимальной программы:

```minic
func main() -> int32 {
    return 0;
}
```

### 2.2 Объявления верхнего уровня

```ebnf
RawDeclaration ::= FunctionDecl
                 | StructDecl
                 | TypeAliasDecl
                 | NamespaceDecl
Declaration ::= ModuleVisibility? RawDeclaration
```

До верхнеуровневых объявлений могут находиться заголовки модуля и импорта:

```ebnf
Program ::= ModuleDecl? ImportDecl* Declaration* EOF

ModuleDecl ::= "module" ModulePath ";"
ImportDecl ::= "import" ModulePath ("::" QualifiedImportPath)? ";"
ModulePath ::= IDENTIFIER ("." IDENTIFIER)*
QualifiedImportPath ::= IDENTIFIER ("::" IDENTIFIER)*
ModuleVisibility ::= "export" | "private"
```

Объявления переменных поддерживаются языком, но допускаются только внутри блоков функций и описаны в разделе `Statement`.

#### Функции

```ebnf
FunctionDecl ::= "func" FunctionName "(" ParamList? ")" "->" Type Block
FunctionName ::= IDENTIFIER | OperatorName
OperatorName ::= "operator" OverloadableOperator
OverloadableOperator ::= "+" | "-" | "*" | "/" | "%"
                       | "==" | "!=" | "<" | ">" | "<=" | ">="
                       | "&" | "|" | "^" | "<<" | ">>"
                       | "!" | "~"

ParamList    ::= Param ("," Param)*
Param        ::= Type IDENTIFIER ("=" Expression)?
```

Тип возврата указывается явно через `->`.
Для функций без возвращаемого значения тип возврата равен `void`.
Параметры могут иметь значения по умолчанию.
Имена функций могут быть как обычными идентификаторами, так и именами перегруженных операторов.

Примеры:

```minic
func add(int32 a, int32 b) -> int32 {
    return a + b;
}

func greet(string name) -> void {
    print(name);
    return;
}
```

#### Структуры

```ebnf
StructDecl       ::= "struct" IDENTIFIER "{" StructMember* "}"
StructMember     ::= VisibilitySection
                   | Field
                   | MethodDecl
VisibilitySection ::= "public" ":" | "private" ":"
Field            ::= Type IDENTIFIER ","
MethodDecl       ::= FunctionDecl
```

Пример:

```minic
struct Point {
    public:
    int32 x,
    int32 y

    func sum() -> int32 {
        return self.x + self.y;
    }
}
```

Внутри структуры допускаются как поля, так и методы.
Секция `public:` или `private:` меняет видимость следующих членов структуры.
Метод также может быть объявлен как перегруженный оператор.

#### Синонимы типов

```ebnf
TypeAliasDecl ::= "type" IDENTIFIER "=" Type ";"
```

Синоним не создаёт новый тип — это альтернативное имя для существующего.

Пример:

```minic
type Meters = int32;
type Matrix = float64[4];
```

#### Пространства имён

```ebnf
NamespaceDecl ::= "namespace" IDENTIFIER "{" Declaration* "}"
```

Внутри `namespace` допускаются только те же объявления, что и на верхнем уровне модуля.

Пример:

```minic
namespace Math {
    func square(float64 x) -> float64 {
        return x * x;
    }
}
```

#### Модули

Каждый файл может объявить себя модулем через `module`.
Другой файл может подключить его через `import`.
В полной модульной системе допускаются:

- импорт модуля целиком: `import Math;`
- импорт отдельного экспортированного объявления: `import Math::square;`
- явная пометка top-level объявлений как `export` или `private`

Пример:

```minic
module Math;

export func square(int32 x) -> int32 {
    return x * x;
}

private func hidden(int32 x) -> int32 {
    return x + 1;
}
```

```minic
import Math;

func main() -> int32 {
    return Math::square(5);
}
```

```minic
import Math::square;

func main() -> int32 {
    return square(5);
}
```

### 2.3 Типы

```ebnf
Type ::= BasicType
       | ArrayType
       | CustomType

BasicType ::= "int8" | "int16" | "int32" | "int64"
            | "uint8" | "uint16" | "uint32" | "uint64"
            | "float32" | "float64"
            | "bool" | "char" | "string" | "void"

ArrayType    ::= NonArrayType "[" INT_LITERAL "]"
NonArrayType ::= BasicType | CustomType
CustomType   ::= IDENTIFIER ("::" IDENTIFIER)*
```

- `ArrayType` строится поверх `NonArrayType`, чтобы избежать неоднозначности.
- Многомерные массивы в данной версии языка не поддерживаются.
- Размер массива является частью типа.

Примеры типов:

```minic
int32
int32[10]
Point
Math::Vector
```

### 2.4 Инструкции

```ebnf
Block ::= "{" Statement* "}"

Statement ::= Block
            | VariableDecl
            | IfStmt
            | WhileStmt
            | ReturnStmt
            | BreakStmt
            | ContinueStmt
            | ExprStmt
            | EmptyStmt
```

Блок является полноценной инструкцией и вводит новую область видимости.

#### Объявление переменной

```ebnf
VariableDecl ::= ("let" | "const") Type IDENTIFIER "=" Expression ";"
```

- `let` обозначает мутабельную переменную.
- `const` обозначает иммутабельную переменную.
- Тип указывается явно.
- Объявление без инициализатора не допускается.

Примеры:

```minic
let int32 x = 0;
let float32 ratio = 1.5;
const float64 pi = 3.14159265;
const string greeting = "hello";
```

#### Условие

```ebnf
IfStmt ::= "if" "(" Expression ")" Block ("else" Block)?
```

Ветка `else` необязательна.

Язык также поддерживает выражение `if`:

```ebnf
IfExpr ::= "if" "(" Expression ")" Expression "else" Expression
```

Оно используется там, где ожидается обычное выражение:

```minic
let int32 x = if (flag) 10 else 20;
```

#### Цикл

```ebnf
WhileStmt ::= "while" "(" Expression ")" Block
```

#### Управление циклом

```ebnf
BreakStmt    ::= "break" ";"
ContinueStmt ::= "continue" ";"
```

#### Возврат из функции

```ebnf
ReturnStmt ::= "return" Expression? ";"
```

#### Инструкция-выражение и пустая инструкция

```ebnf
ExprStmt  ::= Expression ";"
EmptyStmt ::= ";"
```

`ExprStmt` используется для вызовов функций с побочными эффектами и для присваивания.
`EmptyStmt` является нулевой инструкцией и не производит эффекта.

### 2.5 Выражения

Приоритет операторов задаётся структурой грамматики: от низшего к высшему.

```ebnf
Expression ::= AssignmentExpr
```

#### Присваивание

```ebnf
AssignmentExpr ::= Assignable "=" AssignmentExpr
                 | PipeExpr

Assignable ::= AssignableBase AssignSuffix*

AssignableBase ::= IDENTIFIER
                 | NamespaceAccess

AssignSuffix ::= "[" Expression "]"
               | "." IDENTIFIER
```

Присваивание право-ассоциативно: `a = b = c` разбирается как `a = (b = c)`.

Синтаксически слева от `=` допускаются только:

- идентификатор переменной;
- доступ к элементу массива;
- доступ к полю структуры;
- доступ через `NamespaceAccess` с последующей проверкой допустимости записи на этапе семантики.

В текущей версии языка простое `NamespaceAccess` не является допустимой записываемой целью по правилам семантики.

#### Логические операции

```ebnf
PipeExpr       ::= LogicalOrExpr ("|>" LogicalOrExpr)*
LogicalOrExpr  ::= LogicalAndExpr ("||" LogicalAndExpr)*
LogicalAndExpr ::= BitwiseOrExpr  ("&&" BitwiseOrExpr)*
BitwiseOrExpr  ::= BitwiseXorExpr ("|" BitwiseXorExpr)*
BitwiseXorExpr ::= BitwiseAndExpr ("^" BitwiseAndExpr)*
BitwiseAndExpr ::= EqualityExpr   ("&" EqualityExpr)*
```

Оператор `|>` является синтаксическим сахаром:

- `x |> f` десугарится в `f(x)`;
- `x |> f(a, b)` десугарится в `f(x, a, b)`.

#### Сравнения

```ebnf
EqualityExpr   ::= RelationalExpr (("==" | "!=") RelationalExpr)*
RelationalExpr ::= ShiftExpr      (("<" | ">" | "<=" | ">=") ShiftExpr)*
ShiftExpr      ::= AdditiveExpr   (("<<" | ">>") AdditiveExpr)*
```

#### Арифметика

```ebnf
AdditiveExpr       ::= MultiplicativeExpr (("+" | "-") MultiplicativeExpr)*
MultiplicativeExpr ::= UnaryExpr (("*" | "/" | "%") UnaryExpr)*
```

#### Унарные операции

```ebnf
UnaryExpr ::= ("-" | "!" | "~") UnaryExpr
            | CastExpr
```

#### Приведение типов

```ebnf
CastExpr ::= "cast" "<" Type ">" "(" Expression ")"
           | PostfixExpr
```

Неявные приведения типов запрещены.
Любое преобразование должно быть записано явно через `cast`.

Примеры:

```minic
cast<float32>(x)
cast<int32>(3.14)
cast<string>(42)
```

#### Постфиксные выражения

```ebnf
PostfixExpr ::= Atom PostfixOp*

PostfixOp ::= "(" ArgList? ")"
            | "[" Expression "]"
            | "." IDENTIFIER

ArgList ::= Argument ("," Argument)*
Argument ::= IDENTIFIER "=" Expression
           | Expression
```

Вызовы, индексирование и доступ к полям можно комбинировать в цепочки:

```minic
obj.items[0].name()
```

#### Атомарные выражения

```ebnf
Atom ::= IDENTIFIER
       | Literal
       | "(" Expression ")"
       | NamespaceAccess
       | StructLiteral
       | ArrayLiteral
       | IfExpr
```

#### Доступ к пространству имён

```ebnf
NamespaceAccess ::= IDENTIFIER ("::" IDENTIFIER)+
```

Примеры:

```minic
Math::pi
Std::IO::print
```

#### Литерал структуры

```ebnf
StructLiteral ::= CustomType "{" (FieldInit ("," FieldInit)* ","?)? "}"

FieldInit ::= IDENTIFIER ":" Expression
```

Все поля структуры должны быть инициализированы, иначе это семантическая ошибка.

Примеры:

```minic
let Point p = Point { x: 1, y: 2 };
let Math::Point q = Math::Point { x: 3, y: 4 };
```

#### Литерал массива

```ebnf
ArrayLiteral ::= "[" (Expression ("," Expression)* ","?)? "]"
```

Количество элементов должно совпадать с размером в типе массива, иначе это семантическая ошибка.

Примеры:

```minic
let int32[3] arr = [1, 2, 3];
let float32[2] v = [0.0, 1.0];
```

#### Литералы

```ebnf
Literal ::= INT_LITERAL
          | FLOAT_LITERAL
          | BOOL_LITERAL
          | CHAR_LITERAL
          | STRING_LITERAL
```

---

## 3. Встроенные функции

Имена встроенных функций не являются ключевыми словами: лексически это обычные идентификаторы,
но семантически они разрешаются как встроенные объявления.

| Сигнатура | Описание |
|-----------|----------|
| `print(T) -> void` | вывод значения в стандартный поток вывода |
| `input() -> string` | чтение строки из стандартного потока ввода |
| `len(string) -> int32` | длина строки |
| `len(T[N]) -> int32` | длина массива фиксированного размера |
| `exit(int32) -> void` | завершение программы с кодом возврата |
| `panic(string) -> void` | аварийное завершение с сообщением |
| `assert(bool) -> void` | проверка условия; при ложном условии аварийное завершение |

Для `print(T)` допустимы следующие типы `T`:

- любой целочисленный тип;
- `float32`, `float64`;
- `bool`;
- `char`;
- `string`.

Примеры:

```minic
print("hello\n");
print(42);
print(true);

let string s = input();
let int32 n = len(s);

exit(0);
panic("unreachable");
```

---

## 4. Приоритет и ассоциативность операторов

| Приоритет | Операторы | Ассоциативность |
|-----------|-----------|-----------------|
| 1 | `=` | право-ассоциативно |
| 2 | `|>` | лево-ассоциативно |
| 3 | `||` | лево-ассоциативно |
| 4 | `&&` | лево-ассоциативно |
| 5 | `|` | лево-ассоциативно |
| 6 | `^` | лево-ассоциативно |
| 7 | `&` | лево-ассоциативно |
| 8 | `==`, `!=` | лево-ассоциативно |
| 9 | `<`, `>`, `<=`, `>=` | лево-ассоциативно |
| 10 | `<<`, `>>` | лево-ассоциативно |
| 11 | `+`, `-` | лево-ассоциативно |
| 12 | `*`, `/`, `%` | лево-ассоциативно |
| 13 | унарные `-`, `!`, `~` | право-ассоциативно |
| 14 | `cast<T>(...)` | — |
| 10 | `()`, `[]`, `.` | лево-ассоциативно |

---

## 5. Порядок вычисления

Операнды бинарных операторов и аргументы при вызове функции вычисляются слева направо.

Пример:

```minic
func f() -> int32 { return 1; }
func g() -> int32 { return 2; }

let int32 x = f() + g();
foo(f(), g());
```

В обоих случаях сначала вычисляется `f()`, затем `g()`.

---

## 6. Операции со строками

Тип `string` поддерживает:

| Операция | Синтаксис | Результат |
|----------|-----------|-----------|
| Конкатенация | `s1 + s2` | `string` |
| Равенство | `s1 == s2` | `bool` |
| Неравенство | `s1 != s2` | `bool` |
| Длина | `len(s)` | `int32` |

Строки не поддерживают операторы `<`, `>`, `<=`, `>=`.
