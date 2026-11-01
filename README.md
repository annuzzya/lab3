# cparse — транслятор підмножини C на flex/bison

Lab 3. Синтаксичний аналізатор для підмножини мови C, побудований на Lex/Yacc (flex + bison).
Програма розбирає вихідний код, будує AST, перевіряє типи та області видимості, виконує програму
(інтерпретатор) або транслює її в JavaScript. AST можна переглянути в браузері (d3.js).

```
 source.c ──► lexer.l ──► parser.y ──► AST ──► sema ──┬──► interp     (обчислення)
              (flex)      (bison,                     ├──► codegen    (JavaScript)
                          LALR(1))                    └──► JSON ──► web/ (d3.js)
```

## Збірка

Потрібні `flex`, `bison` (підходить і 2.3 з macOS, і 3.x), C-компілятор, `make`.
Для перевірки згенерованого JavaScript у тестах — `node` (необов'язково).

```bash
make            # збирає ./cparse
make test       # порівнює вивід cparse з виводом програм, зібраних справжнім компілятором
```

## Використання

```bash
./cparse examples/sort.c                      # виконати програму
./cparse --tokens examples/hello.c            # потік токенів від лексера
./cparse --ast examples/hello.c               # AST з типами
./cparse --js out.js examples/sort.c          # трансляція в JavaScript
node out.js
./cparse --json ast.json examples/sort.c      # AST у форматі JSON
./cparse --web web examples/sort.c            # дані для візуалізатора
open web/index.html
```

Приклад для `int x = 2 + 3 * 4; if (x > 10) x = x - 1;`:

```
|-- DeclStmt int  [line 3]
|   `-- VarDecl int x  [line 3]
|       `-- init: BinaryOperator + <int>  [line 3]
|           |-- lhs: IntegerLiteral 2 <int>  [line 3]
|           `-- rhs: BinaryOperator * <int>  [line 3]
|               |-- lhs: IntegerLiteral 3 <int>  [line 3]
|               `-- rhs: IntegerLiteral 4 <int>  [line 3]
|-- IfStmt  [line 4]
|   |-- cond: BinaryOperator > <int>  [line 4]
...
```

```js
function main() {
    let x = (2 + Math.imul(3, 4)) | 0;
    if (x > 10) {
        x = (x - 1) | 0;
    }
    return x;
}
```

## Підтримувана мова

Підмножина C99, достатня для реальних програм — усі приклади з `examples/` компілюються звичайним `cc` і дають той самий вивід.

* типи `int`, `double`, `void`, одновимірні масиви `int a[10]`, параметри-масиви `int a[]`;
* глобальні та локальні змінні, вкладені блоки й затінення імен, оголошення в `for (int i = 0; ...)`;
* функції, прототипи (оголошення до визначення), рекурсія;
* `if/else`, `while`, `do-while`, `for`, `break`, `continue`, `return`;
* усі арифметичні, порівняння, логічні (з коротким обчисленням), `?:`, `=`, `+= -= *= /= %=`, префіксні та постфіксні `++/--`, приведення `(int)`, `(double)`;
* літерали: цілі (десяткові, `0x..`), дійсні, символьні `'a'`, рядкові;
* `printf` з повноцінним форматуванням (`%d %5.2f %-8s %x %e %g`, ...), `sqrt`, `fabs`, `pow`;
* коментарі `//`, `/* */`, рядки препроцесора (`#include`) пропускаються.

## Структура

| файл | призначення |
|---|---|
| `src/lexer.l` | лексичний аналізатор (flex): ключові слова, ідентифікатори, числа, рядки, оператори, коментарі; номери рядків для кожного токена |
| `src/parser.y` | граматика (bison, LALR(1)), пріоритети операторів, побудова AST, відновлення після помилок |
| `src/ast.c/.h` | вузли AST, вивід дерева у вигляді тексту та JSON |
| `src/sema.c` | семантичний аналіз: таблиці символів з вкладеними областями видимості, типи виразів, перевірка викликів і формату `printf` |
| `src/interp.c` | інтерпретатор AST: кадри стеку, масиви, 32-бітна цілочисельна арифметика, помилки часу виконання |
| `src/codegen.c` | генерація JavaScript з урахуванням семантики C |
| `src/format.c` | розбір специфікаторів `printf` (спільний для перевірки та виконання) |
| `web/` | візуалізатор AST на d3.js |
| `grammars/` | маленькі граматики для демонстрації конфліктів |
| `examples/` | тестові програми, `examples/errors/` — програми з помилками |

## Граматика і конфлікти

Вирази описані неоднозначною граматикою `expr : expr '+' expr | ...`, а порядок розбору задають
декларації пріоритету й асоціативності (від найнижчого до найвищого):

```
%nonassoc THEN
%nonassoc ELSE
%right '=' ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN
%right '?' ':'
%left OR
%left AND
%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/' '%'
%right UNARY
%left INC DEC '['
```

Коли автомат у стані, де можна і зсунути (shift) наступний токен, і згорнути (reduce) правило,
bison порівнює пріоритет правила (пріоритет його останнього термінала або `%prec`) з пріоритетом
токена: вищий пріоритет перемагає, при рівному — вирішує асоціативність (`%left` → reduce,
`%right` → shift). Так `a - b - c` розбирається як `(a - b) - c`, а `a = b = c` — як `a = (b = c)`.

Проблема «висячого else» розв'язана через `%prec THEN`: правило `if` без `else` має нижчий
пріоритет, ніж токен `ELSE`, тому `else` завжди прив'язується до найближчого `if`.

Директива `%expect 0` гарантує, що в граматиці немає жодного нерозв'язаного конфлікту — інакше збірка
зупиниться. Таблиці LALR-автомата після збірки лежать у `build/parser.output`.

`make conflicts` показує конфлікти на навчальних граматиках:

```
== grammars/dangling_else.y      1 shift/reduce  (if-else без пріоритетів)
== grammars/expr_ambiguous.y     4 shift/reduce  (вирази без %left)
== grammars/expr_precedence.y    0               (та сама граматика з %left)
== grammars/reduce_reduce.y      1 reduce/reduce (ID може бути і типом, і змінною)
```

Відновлення після синтаксичних помилок — через спеціальний токен `error`:
`block_item : error ';'`, `external_decl : error '}'`, `IF '(' error ')' statement`.
Тому компілятор повідомляє про кілька помилок за один запуск:

```
examples/errors/syntax.c:4: error: syntax error, unexpected int, expecting ';' or ','
examples/errors/syntax.c:5: error: syntax error, unexpected ')'
```

## Семантичний аналіз

Після розбору AST обходиться ще раз. Кожне оголошення потрапляє в таблицю символів поточної
області видимості й отримує номер слота (глобальний або локальний у кадрі функції), а кожне
використання імені зв'язується з оголошенням. Для кожного виразу обчислюється тип за правилами C
(`int op double → double`, порівняння дають `int`, `%` лише для цілих).

```
examples/errors/semantic.c:11: error: void function 'report' should not return a value
examples/errors/semantic.c:18: error: function 'add' expects 2 arguments, got 1
examples/errors/semantic.c:19: error: expression is not assignable
examples/errors/semantic.c:20: error: invalid operands to '%' ('double' and 'int')
examples/errors/semantic.c:21: error: format '%d' expects 'int', but argument has type 'double'
examples/errors/semantic.c:22: error: use of undeclared identifier 'undefined_var'
examples/errors/semantic.c:23: error: 'break' statement not in loop
examples/errors/semantic.c:24: error: call to undeclared function 'missing'
```

## Обчислення

`src/interp.c` — інтерпретатор, що обходить AST. Цілі числа поводяться як 32-бітний `int`
(переповнення з перенесенням, ділення з відкиданням дробової частини), масиви передаються за
посиланням, вихід за межі масиву, ділення на нуль і надто глибока рекурсія дають помилку з номером рядка.

## Генерація JavaScript

| C | JavaScript |
|---|---|
| `int a[10]; double d[5];` | `new Int32Array(10)`, `new Float64Array(5)` |
| `a + b`, `a - b` (int) | `(a + b) \| 0` — повернення до 32 біт |
| `a * b` (int) | `Math.imul(a, b)` |
| `a / b`, `a % b` (int) | `__idiv(a, b)`, `__imod(a, b)` — ділення з відкиданням і перевіркою на нуль |
| `(int)x`, `int n = 2.9;` | `__toint(x)` |
| `a < b` як значення | `+(a < b)` — результат 0/1, як у C |
| `printf(...)` | `__printf(...)` — реалізація форматування C у рантаймі |

Допоміжні функції додаються в результат лише тоді, коли вони потрібні. Рядки `"use strict"` і
`let` зберігають блокову область видимості C. Згенерований код запускається і в Node.js, і в браузері.

## Візуалізація

`./cparse --web web file.c` записує `web/data.js` (код, AST, JavaScript), після чого
`web/index.html` відкривається просто подвійним кліком. Можна:

* розгортати й згортати вузли (клік по кружечку), масштабувати та перетягувати дерево;
* навести на вузол — підсвітиться відповідний рядок коду; клік — властивості вузла;
* подивитися згенерований JavaScript і запустити його прямо в браузері.

Також у вікно можна перетягнути файл, створений `--json`.

## Тести

`make test` збирає кожну програму з `examples/` системним компілятором і порівнює її вивід із
виводом інтерпретатора та згенерованого JavaScript; програми з `examples/errors/` мають завершитися помилкою.

```
expressions.c  interpreter ok   javascript ok
factorial.c    interpreter ok   javascript ok
newton.c       interpreter ok   javascript ok
sort.c         interpreter ok   javascript ok
...
passed: 19, failed: 0
```
