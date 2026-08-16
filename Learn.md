# Wast2 Learn

> Wast2를 처음부터 끝까지 배우는 공식 학습 문서입니다.

---

# 1. Wast2란?

Wast2는 간결한 문법을 사용하는 경량 프로그래밍 언어입니다.

Wast2의 현재 구현은 **Wast 소스 코드를 C++23 코드로 변환(transpile)** 합니다.

```text
Wast2 source
     ↓
   wastc
     ↓
C++23 source
     ↓
   C++ compiler
     ↓
 executable
```

Wast2 컴파일러는 현재 단일 C++ 파일로 제공되며 외부 라이브러리 없이 C++23 표준 라이브러리만 사용합니다.

---

# 2. 첫 번째 프로그램

가장 간단한 Wast2 프로그램입니다.

```wast
print("Hello, Wast2!")
```

실행하면:

```text
Hello, Wast2!
```

Wast2에서는 문장의 끝에 세미콜론을 반드시 넣을 필요가 없습니다.

세미콜론도 사용할 수 있습니다.

```wast
print("Hello");
print("World");
```

---

# 3. 주석

한 줄 주석은 `//`를 사용합니다.

```wast
// 이것은 주석입니다.

print("Hello") // 이것도 주석입니다.
```

주석은 컴파일 과정에서 무시됩니다.

---

# 4. 값과 기본 자료형

Wast2의 런타임 값은 다음 종류를 가집니다.

| 타입      | 설명    |
| ------- | ----- |
| `nil`   | 값이 없음 |
| `bool`  | 참/거짓  |
| `int`   | 정수    |
| `float` | 실수    |
| `str`   | 문자열   |
| `array` | 배열    |
| `map`   | 맵     |
| `func`  | 함수    |

---

## 4.1 정수

```wast
local age = 13
local score = 100
local big = 1000000
```

숫자 사이에 `_`를 넣을 수도 있습니다.

```wast
local million = 1_000_000
```

---

## 4.2 실수

```wast
local pi = 3.14159
local temperature = 36.5
```

---

## 4.3 문자열

문자열은 `" "`를 사용합니다.

```wast
local name = "Wast2"
local message = "Hello World"
```

---

## 4.4 Boolean

```wast
local yes = true
local no = false
```

---

## 4.5 Nil

```wast
local nothing = nil
```

`nil`은 값이 없음을 나타냅니다.

---

# 5. 변수

변수 선언에는 `local`을 사용합니다.

```wast
local x = 10
local name = "Wast2"
```

변수에 새로운 값을 대입할 수 있습니다.

```wast
local x = 10

x = 20
```

---

# 6. const

`const`를 사용하여 상수처럼 선언할 수 있습니다.

```wast
const PI = 3.14159
const NAME = "Wast2"
```

현재 Wast2 구현에서는 `const`가 문법적으로 지원되며 변수 선언과 같은 Value 기반 런타임을 사용합니다.

---

# 7. 타입 표기

변수와 함수 매개변수에는 타입 표기를 사용할 수 있습니다.

```wast
local x: i64 = 10
local y: f64 = 3.14
local ok: bool = true
local name: str = "Wast2"
```

현재 일반 Wast 변수의 타입 표기는 문법적으로 허용되며 런타임 값 자체는 동적 `Value`로 관리됩니다.

`library` 선언에서는 타입 표기가 C++ 타입 변환에 직접 사용됩니다.

---

# 8. 대입 연산자

기본 대입:

```wast
x = 10
```

복합 대입:

```wast
x += 5
x -= 2
x *= 3
x /= 4
x %= 2
```

예:

```wast
local x = 10

x += 5
print(x)

x *= 2
print(x)
```

---

# 9. 산술 연산

지원되는 기본 산술 연산:

```text
+   더하기
-   빼기
*   곱하기
/   나누기
%   나머지
```

예:

```wast
local a = 10
local b = 3

print(a + b)
print(a - b)
print(a * b)
print(a / b)
print(a % b)
```

---

# 10. 문자열 더하기

문자열에도 `+`를 사용할 수 있습니다.

```wast
local first = "Hello"
local second = " Wast2"

print(first + second)
```

결과:

```text
Hello Wast2
```

문자열과 다른 값도 `+`로 연결할 수 있습니다.

```wast
local score = 100

print("score: " + score)
```

---

# 11. 비교 연산

지원되는 비교 연산:

```text
==   같다
!=   다르다
<    작다
<=   작거나 같다
>    크다
>=   크거나 같다
```

예:

```wast
local x = 10

print(x == 10)
print(x != 5)
print(x > 3)
print(x <= 10)
```

비교 결과는 Boolean 값입니다.

---

# 12. 논리 연산

AND:

```wast
a && b
```

OR:

```wast
a || b
```

NOT:

```wast
!a
```

예:

```wast
local age = 20
local has_ticket = true

if age >= 18 && has_ticket (
    print("allowed")
)
```

---

# 13. Truthy / Falsy

조건문은 Boolean만 검사하지 않습니다.

Wast2에는 값의 `truthy` 규칙이 있습니다.

False로 취급되는 값:

```text
nil
false
0
0.0
""
[]
{}
```

그 외의 값은 일반적으로 true로 취급됩니다.

예:

```wast
local name = "Wast2"

if name (
    print("name exists")
)
```

---

# 14. if

기본 조건문:

```wast
if x > 10 (
    print("big")
)
```

Wast2의 블록은 `{}` 대신 `()`를 사용합니다.

---

# 15. elseif

```wast
if score >= 90 (
    print("A")
) elseif score >= 80 (
    print("B")
) elseif score >= 70 (
    print("C")
) else (
    print("D")
)
```

---

# 16. else

```wast
if x > 0 (
    print("positive")
) else (
    print("zero or negative")
)
```

---

# 17. while

조건이 true인 동안 반복합니다.

```wast
local x = 0

while x < 5 (
    print(x)
    x += 1
)
```

---

# 18. break

반복을 즉시 종료합니다.

```wast
local x = 0

while true (
    print(x)

    if x >= 5 (
        break
    )

    x += 1
)
```

---

# 19. continue

현재 반복을 건너뛰고 다음 반복으로 이동합니다.

```wast
for i = 0, 10 (
    if i == 5 (
        continue
    )

    print(i)
)
```

---

# 20. 숫자 for

Wast2는 숫자 범위 기반 `for`를 제공합니다.

```wast
for i = 0, 5 (
    print(i)
)
```

기본 step은 `1`입니다.

step을 직접 지정할 수도 있습니다.

```wast
for i = 0, 10, 2 (
    print(i)
)
```

결과:

```text
0
2
4
6
8
10
```

음수 step도 사용할 수 있습니다.

```wast
for i = 10, 0, -2 (
    print(i)
)
```

---

# 21. for-in

배열을 순회할 수 있습니다.

```wast
local numbers = [10, 20, 30, 40]

for x in numbers (
    print(x)
)
```

문자열도 iterable로 사용할 수 있습니다.

```wast
for x in "ABC" (
    print(x)
)
```

문자열 순회에서 얻는 값은 문자의 byte 값입니다.

---

# 22. 배열

배열은 `[]`를 사용합니다.

```wast
local numbers = [1, 2, 3, 4, 5]
```

여러 종류의 값도 넣을 수 있습니다.

```wast
local data = [
    10,
    "hello",
    true,
    nil
]
```

---

# 23. 배열 인덱싱

배열 인덱스는 `[]`로 접근합니다.

```wast
local numbers = [10, 20, 30]

print(numbers[0])
print(numbers[1])
print(numbers[2])
```

인덱스는 `0`부터 시작합니다.

---

# 24. 음수 인덱스

음수 인덱스를 사용할 수 있습니다.

```wast
local numbers = [10, 20, 30]

print(numbers[-1])
```

`-1`은 마지막 원소입니다.

```text
-1 → 마지막
-2 → 뒤에서 두 번째
```

---

# 25. 배열 수정

인덱스를 이용해 값을 변경할 수 있습니다.

```wast
local numbers = [10, 20, 30]

numbers[1] = 99

print(numbers)
```

배열의 마지막 위치에 값을 추가하는 방식도 가능합니다.

```wast
numbers[3] = 40
```

---

# 26. push

`push()`는 배열에 값을 추가합니다.

```wast
local numbers = [1, 2, 3]

numbers = push(numbers, 4)

print(numbers)
```

또는 pipe를 사용할 수 있습니다.

```wast
numbers |> push(4)
```

---

# 27. pop

`pop()`은 배열의 마지막 값을 제거하고 반환합니다.

```wast
local numbers = [1, 2, 3]

local x = pop(numbers)

print(x)
```

---

# 28. len

문자열, 배열, 맵의 길이를 가져옵니다.

```wast
print(len("Hello"))
print(len([1, 2, 3]))
```

---

# 29. Map

Map은 `{}`를 사용합니다.

```wast
local user = {
    name: "Alice",
    age: 20
}
```

키는 문자열이나 표현식이 될 수 있습니다.

```wast
local key = "name"

local user = {
    key: "Alice"
}
```

---

# 30. Map 접근

인덱싱:

```wast
local user = {
    name: "Alice",
    age: 20
}

print(user["name"])
print(user["age"])
```

점 표기법도 사용할 수 있습니다.

```wast
print(user.name)
print(user.age)
```

---

# 31. Map 수정

```wast
local user = {
    name: "Alice"
}

user.name = "Bob"
```

또는:

```wast
user["name"] = "Bob"
```

존재하지 않는 Map 키도 대입으로 만들 수 있습니다.

```wast
user.score = 100
```

---

# 32. keys

Map의 모든 키를 배열로 가져옵니다.

```wast
local user = {
    name: "Alice",
    age: 20
}

local k = keys(user)

print(k)
```

---

# 33. values

Map의 모든 값을 배열로 가져옵니다.

```wast
local user = {
    name: "Alice",
    age: 20
}

local v = values(user)

print(v)
```

---

# 34. type

값의 타입 이름을 가져옵니다.

```wast
print(type(10))
print(type(3.14))
print(type("hello"))
print(type(true))
print(type([1, 2]))
```

결과는 각각 다음과 같은 문자열입니다.

```text
int
float
str
bool
array
```

---

# 35. 문자열 Escape

문자열에서는 다음 escape를 사용할 수 있습니다.

```text
\n   줄바꿈
\t   탭
\r   carriage return
\\   \
\"   "
\0   null
```

예:

```wast
print("Hello\nWorld")
print("A\tB")
```

---

# 36. 문자열 보간

문자열 안에서 간단한 값을 `{}`로 넣을 수 있습니다.

```wast
local name = "Wast2"

print("Hello {name}!")
```

숫자도 가능합니다.

```wast
local score = 100

print("Score: {score}")
```

숫자 리터럴도 가능합니다.

```wast
print("Value: {123}")
print("Pi: {3.14}")
```

현재 문자열 보간은 **간단한 변수 이름과 숫자**만 지원합니다.

다음과 같은 복잡한 표현식은 현재 구현에서 지원하지 않습니다.

```wast
// 지원하지 않음
print("Value: {x + 10}")
```

대신:

```wast
local result = x + 10

print("Value: {result}")
```

---

# 37. 함수

함수는 `fn`으로 선언합니다.

```wast
fn add(a, b) (
    return a + b
)
```

호출:

```wast
print(add(10, 20))
```

---

# 38. 반환값

`return`으로 값을 반환합니다.

```wast
fn square(x) (
    return x * x
)

local result = square(5)

print(result)
```

---

# 39. 암시적 반환

함수의 마지막 표현식은 자동으로 반환됩니다.

```wast
fn add(a, b) (
    a + b
)
```

다음과 사실상 같은 의미입니다.

```wast
fn add(a, b) (
    return a + b
)
```

따라서 짧은 함수는 매우 간결하게 작성할 수 있습니다.

---

# 40. 매개변수 타입

함수 매개변수에도 타입 표기를 사용할 수 있습니다.

```wast
fn add(a: i64, b: i64) (
    return a + b
)
```

---

# 41. 반환 타입

`->` 뒤에 반환 타입을 작성할 수 있습니다.

```wast
fn add(a: i64, b: i64) -> i64 (
    return a + b
)
```

현재 일반 함수의 타입 표기는 주로 문법 및 문서상의 타입 정보이며 런타임은 `Value` 기반입니다.

---

# 42. 재귀 함수

함수는 자기 자신을 호출할 수 있습니다.

```wast
fn factorial(n) (
    if n <= 1 (
        return 1
    )

    return n * factorial(n - 1)
)

print(factorial(5))
```

---

# 43. Lambda

Wast2에서는 익명 함수도 만들 수 있습니다.

```wast
local add = fn(a, b) (
    return a + b
)

print(add(10, 20))
```

짧은 함수는 암시적 반환과 함께 사용할 수 있습니다.

```wast
local square = fn(x) (
    x * x
)
```

---

# 44. 함수도 값이다

함수를 변수에 저장할 수 있습니다.

```wast
local operation = fn(x) (
    x * 2
)

print(operation(10))
```

함수를 다른 함수에 전달할 수도 있습니다.

```wast
fn apply(value, f) (
    return f(value)
)

local double = fn(x) (
    x * 2
)

print(apply(10, double))
```

---

# 45. map

`map()`은 iterable의 각 원소에 함수를 적용합니다.

```wast
local numbers = [1, 2, 3, 4]

local result = map(
    numbers,
    fn(x) (
        x * 2
    )
)

print(result)
```

결과:

```text
[2, 4, 6, 8]
```

---

# 46. filter

`filter()`는 조건을 만족하는 원소만 남깁니다.

```wast
local numbers = [1, 2, 3, 4, 5, 6]

local result = filter(
    numbers,
    fn(x) (
        x % 2 == 0
    )
)

print(result)
```

결과:

```text
[2, 4, 6]
```

---

# 47. reduce

`reduce()`는 여러 값을 하나의 값으로 합칩니다.

```wast
local numbers = [1, 2, 3, 4]

local result = reduce(
    numbers,
    0,
    fn(acc, x) (
        acc + x
    )
)

print(result)
```

결과:

```text
10
```

---

# 48. Pipe Operator

Wast2의 독특한 기능 중 하나가 `|>`입니다.

```wast
value |> function
```

예:

```wast
10 |> print
```

이는 대략 다음과 같은 의미입니다.

```wast
print(10)
```

---

# 49. Pipe + 함수

```wast
local numbers = [1, 2, 3, 4]

numbers |> len |> print
```

Pipe를 여러 번 연결할 수도 있습니다.

---

# 50. Pipe + 인자

```wast
local numbers = [1, 2, 3]

numbers |> push(4)
```

개념적으로:

```wast
push(numbers, 4)
```

와 같은 형태로 처리됩니다.

---

# 51. Pipe + Lambda

Lambda도 pipe의 오른쪽에 사용할 수 있습니다.

```wast
10 |> fn(x) (
    x * 2
) |> print
```

---

# 52. Range

Range 연산자는 `..`와 `..=`입니다.

```text
..    범위
..=   포함 범위
```

`range()` 함수와 함께 사용할 수도 있습니다.

```wast
local numbers = range(0, 5)

print(numbers)
```

세 번째 인자로 포함 여부를 지정할 수 있습니다.

```wast
local numbers = range(0, 5, true)
```

---

# 53. Range와 for

숫자 `for`는 범위 반복에 적합합니다.

```wast
for i = 0, 10 (
    print(i)
)
```

반복 간격도 지정할 수 있습니다.

```wast
for i = 0, 10, 2 (
    print(i)
)
```

---

# 54. Struct

`struct`는 여러 필드를 가진 객체를 쉽게 생성하기 위한 선언입니다.

```wast
struct User(
    name: str,
    age: i64
)
```

이제 `User()`를 호출할 수 있습니다.

```wast
local user = User("Alice", 20)

print(user.name)
print(user.age)
```

Struct는 런타임에서 Map 기반 객체로 생성됩니다.

---

# 55. Struct 필드

```wast
struct Point(
    x: i64,
    y: i64
)

local p = Point(10, 20)

print(p.x)
print(p.y)
```

필드도 수정할 수 있습니다.

```wast
p.x = 100
```

---

# 56. Enum

Enum은 `enum`을 사용합니다.

```wast
enum Color = RED | GREEN | BLUE
```

Enum은 이름을 가진 Map 형태로 생성됩니다.

```wast
print(Color.RED)
print(Color.GREEN)
print(Color.BLUE)
```

각 variant의 이름은 문자열 값으로 저장됩니다.

---

# 57. Match

`match`는 여러 패턴 중 하나를 선택합니다.

```wast
match value (
    1 -> print("one")
    2 -> print("two")
    3 -> print("three")
    _ -> print("other")
)
```

`_`는 wildcard입니다.

---

# 58. Match 여러 패턴

`|`를 사용해 여러 패턴을 하나의 arm으로 묶을 수 있습니다.

```wast
match value (
    1 | 2 | 3 -> print("small")
    _ -> print("other")
)
```

---

# 59. Match 블록

Match arm의 결과로 여러 문장을 실행할 수도 있습니다.

```wast
match value (
    1 -> (
        print("one")
        print("first")
    )

    _ -> (
        print("other")
    )
)
```

---

# 60. Match 표현식

Match arm은 표현식을 반환값처럼 사용할 수도 있습니다.

```wast
local name = match_value
```

현재 구현에서는 match 자체가 statement 형태로 설계되어 있으므로 복잡한 표현식 반환 용도보다는 분기 제어에 사용하는 것이 안전합니다.

---

# 61. Field Access

`.`으로 필드에 접근합니다.

```wast
local user = {
    name: "Alice"
}

print(user.name)
```

이는 Map의 문자열 키 접근과 연결됩니다.

---

# 62. Index Access

`[]`를 사용합니다.

```wast
local data = [10, 20, 30]

print(data[0])
```

Map:

```wast
local data = {
    value: 123
}

print(data["value"])
```

String:

```wast
local text = "ABC"

print(text[0])
```

문자열 인덱스는 해당 문자의 byte 값을 반환합니다.

---

# 63. 중첩 데이터

배열과 Map은 서로 중첩할 수 있습니다.

```wast
local users = [
    {
        name: "Alice",
        age: 20
    },
    {
        name: "Bob",
        age: 25
    }
]

print(users[0].name)
print(users[1].age)
```

---

# 64. 문자열 처리

문자열은 기본적인 연결, 비교, 길이 측정, 인덱싱을 지원합니다.

```wast
local text = "Wast2"

print(len(text))
print(text[0])
print(text == "Wast2")
```

---

# 65. 타입 변환

`int()`:

```wast
local x = int("123")
```

`float()`:

```wast
local x = float("3.14")
```

`str()`:

```wast
local x = str(123)
```

Boolean과 숫자 사이의 변환도 가능합니다.

```wast
print(int(true))
print(float(false))
```

---

# 66. Input

`input()`은 표준 입력에서 한 줄을 읽습니다.

```wast
print("Name?")
local name = input()

print("Hello {name}")
```

---

# 67. Sleep

`sleep()`은 밀리초 단위로 대기합니다.

```wast
print("Start")

sleep(1000)

print("One second later")
```

---

# 68. Shell

`shell()`을 사용하면 호스트 시스템의 명령을 실행할 수 있습니다.

```wast
local result = shell("echo Hello")

print(result)
```

반환값은 시스템 명령의 상태 코드입니다.

> `shell()`은 Wast2 프로그램에서 운영체제 명령을 실행할 수 있게 하므로 신뢰할 수 있는 코드에서만 사용하세요.

---

# 69. main 함수

Wast2 프로그램에 `main` 함수가 존재하면 프로그램 시작 시 자동으로 호출됩니다.

```wast
fn main() (
    print("Program started")
)
```

실행 흐름:

```text
program start
    ↓
wast_entry()
    ↓
main()
```

`main`이 없어도 top-level 코드는 실행됩니다.

---

# 70. Top-Level Code

함수 밖에서도 코드를 작성할 수 있습니다.

```wast
local x = 10

print(x)
```

컴파일된 프로그램의 entry에서 실행됩니다.

---

# 71. 실행 구조

Wast2 소스:

```wast
local x = 10

fn add(a, b) (
    a + b
)

print(add(x, 20))
```

대략 다음과 같은 구조로 변환됩니다.

```text
Wast2
 ↓
Lexer
 ↓
Parser
 ↓
AST
 ↓
Optimizer
 ↓
C++23 Code Generator
 ↓
Generated C++23
 ↓
g++
 ↓
Executable
```

---

# 72. 컴파일러 빌드

현재 Wast2 컴파일러는 C++23이 필요합니다.

예:

```bash
g++ -std=c++23 -O3 -march=native -flto -funroll-loops wast2.cpp -o wastc
```

---

# 73. Wast 프로그램 컴파일

예를 들어 `hello.wast`가 있다면:

```bash
wastc hello.wast hello.cpp
```

출력 파일을 생략할 수도 있습니다.

```bash
wastc hello.wast
```

이 경우:

```text
hello.wast.cpp
```

가 생성됩니다.

---

# 74. 생성된 C++ 컴파일

생성된 C++는 C++23 컴파일러로 컴파일합니다.

```bash
g++ -std=c++23 -O3 hello.cpp -o hello
```

실행:

```bash
./hello
```

Windows에서는:

```powershell
hello.exe
```

---

# 75. 한 번에 빌드하기

예:

```bash
wastc hello.wast hello.cpp
g++ -std=c++23 -O3 hello.cpp -o hello
```

Wast2 자체는 C++23으로 변환되므로 최종 프로그램의 성능은 생성된 C++와 C++ 컴파일러의 최적화에 크게 영향을 받습니다.

---

# 76. Compile-time Optimization

Wast2에는 간단한 AST 최적화가 포함되어 있습니다.

상수끼리의 연산은 컴파일 단계에서 계산될 수 있습니다.

```wast
local x = 10 + 20 * 3
```

문자열 상수 연결도 최적화됩니다.

```wast
local text = "Hello " + "Wast2"
```

Boolean 상수 연산도 최적화됩니다.

```wast
local x = true && false
```

따라서 Wast2는 단순히 문자를 C++로 치환하는 방식이 아니라 **Lexer → Parser → AST → Optimization → Code Generation** 단계를 거칩니다.

---

# 77. 연산자 우선순위

Wast2의 표현식은 다음과 같은 우선순위를 가집니다.

높은 우선순위부터:

```text
함수 호출 / [] / .
단항 - !
* / %
+ -
.. ..=
< <= > >=
== !=
&&
||
대입
|>
```

예:

```wast
local result = 10 + 20 * 3
```

곱셈이 먼저 계산됩니다.

명확하게 만들고 싶다면 괄호를 사용하세요.

```wast
local result = (10 + 20) * 3
```

---

# 78. 함수 호출

일반적인 호출:

```wast
add(10, 20)
```

함수도 값이므로 변수에 저장된 함수를 호출할 수 있습니다.

```wast
local f = fn(x) (
    x * 2
)

print(f(10))
```

---

# 79. 고차 함수

Wast2에서는 함수를 다른 함수에 전달할 수 있습니다.

```wast
fn execute(value, operation) (
    operation(value)
)

local double = fn(x) (
    x * 2
)

print(execute(10, double))
```

이 기능은 AI, 데이터 처리, 자동화 등의 코드에서 유용합니다.

---

# 80. AI / 수치 계산

Wast2의 배열, 함수, 반복문, 고차 함수, 수치 연산을 조합하면 AI 알고리즘을 직접 구현할 수 있습니다.

예를 들어 간단한 선형 계산:

```wast
fn predict(x, weight, bias) (
    x * weight + bias
)

local x = 5.0
local weight = 2.0
local bias = 1.0

print(predict(x, weight, bias))
```

배열 기반 데이터 처리:

```wast
local data = [1, 2, 3, 4, 5]

local doubled = map(
    data,
    fn(x) (
        x * 2
    )
)

print(doubled)
```

Wast2는 AI 전용 언어가 아니라 **일반적인 계산과 데이터 처리도 가능한 언어**이므로 AI 알고리즘을 언어 자체로 작성할 수 있습니다.

---

# 81. AI 예제: 선형 모델

아주 간단한 선형 모델을 만들어 봅니다.

```wast
fn predict(x, w, b) (
    x * w + b
)

local weight = 2.0
local bias = 1.0

local data = [1.0, 2.0, 3.0, 4.0]

for x in data (
    print(predict(x, weight, bias))
)
```

---

# 82. AI 예제: 평균

```wast
fn mean(values) (
    local total = reduce(
        values,
        0.0,
        fn(acc, x) (
            acc + x
        )
    )

    return total / len(values)
)

print(mean([1.0, 2.0, 3.0, 4.0]))
```

---

# 83. AI 예제: 필터링

```wast
local data = [1, 2, 3, 4, 5, 6, 7, 8]

local even = filter(
    data,
    fn(x) (
        x % 2 == 0
    )
)

print(even)
```

이런 식으로 데이터 전처리 파이프라인을 구성할 수 있습니다.

---

# 84. Library

Wast2의 가장 강력한 확장 기능 중 하나입니다.

`library`를 사용하면 Wast2 코드 안에 **C++23 코드를 직접 삽입**할 수 있습니다.

기본 형태:

```wast
library add(a: i64, b: i64) -> i64 """
    return a + b;
"""
```

사용:

```wast
print(add(10, 20))
```

---

# 85. Library 매개변수 타입

지원되는 주요 library 매개변수 타입:

```text
i64
f64
bool
str
value
```

예:

```wast
library multiply(a: i64, b: i64) -> i64 """
    return a * b;
"""
```

---

# 86. Library 반환 타입

지원되는 반환 타입:

```text
i64
f64
bool
str
value
nil
```

예:

```wast
library pi() -> f64 """
    return 3.141592653589793;
"""
```

---

# 87. Library에서 Value 사용

`value`는 Wast2 런타임의 `Value`를 직접 사용할 수 있게 합니다.

```wast
library identity(x: value) -> value """
    return x;
"""
```

이 기능을 사용하면 Wast2와 C++ 런타임 사이에서 더 일반적인 값을 전달할 수 있습니다.

---

# 88. Library에서 문자열 사용

```wast
library hello(name: str) -> str """
    return "Hello " + name;
"""
```

사용:

```wast
print(hello("Wast2"))
```

---

# 89. Library에서 직접 C++ 사용

Triple quote 안의 내용은 생성된 C++에 삽입됩니다.

```wast
library square(x: i64) -> i64 """
    return x * x;
"""
```

따라서 Wast2가 제공하지 않는 기능을 C++23 코드로 확장할 수 있습니다.

---

# 90. Library의 의미

Wast2는 다음 구조를 가집니다.

```text
Wast2
 ├── 간단한 고수준 문법
 │
 ├── 기본 런타임
 │
 └── library
       ↓
     C++23
```

즉 Wast2의 간단한 문법을 유지하면서도 필요하면 C++ 수준까지 내려갈 수 있습니다.

---

# 91. 완전한 프로그램 예제

다음은 여러 기능을 한꺼번에 사용하는 예제입니다.

```wast
// Wast2 example

struct User(
    name: str,
    score: i64
)

fn average(values) (
    local total = reduce(
        values,
        0,
        fn(acc, x) (
            acc + x
        )
    )

    total / len(values)
)

fn main() (
    local scores = [90, 80, 100, 70]

    local avg = average(scores)

    print("Average: {avg}")

    if avg >= 90 (
        print("Excellent")
    ) elseif avg >= 70 (
        print("Good")
    ) else (
        print("Needs improvement")
    )

    local user = User("Player", int(avg))

    print("Name: {user.name}")
    print("Score: {user.score}")
)
```

---

# 92. 좋은 Wast2 코드 작성법

## 짧게 유지하기

Wast2의 장점은 간결함입니다.

복잡한 코드를 불필요하게 길게 작성하지 마세요.

```wast
local doubled = map(
    numbers,
    fn(x) (
        x * 2
    )
)
```

---

## 함수로 분리하기

큰 코드는 함수로 나눕니다.

```wast
fn calculate_score(a, b, c) (
    (a + b + c) / 3
)
```

---

## Struct 활용

관련된 데이터를 하나로 묶습니다.

```wast
struct Player(
    name: str,
    score: i64
)
```

---

## Pipe 활용

데이터 처리 과정이 길어질 경우 pipe를 사용하면 읽기 쉬워질 수 있습니다.

```wast
numbers |> map(fn(x) (
    x * 2
)) |> print
```

---

# 93. 자주 사용하는 패턴

## 카운터

```wast
local count = 0

while count < 10 (
    print(count)
    count += 1
)
```

## 배열 처리

```wast
local result = map(
    [1, 2, 3, 4],
    fn(x) (
        x * 10
    )
)
```

## 조건 필터

```wast
local result = filter(
    [1, 2, 3, 4, 5],
    fn(x) (
        x > 2
    )
)
```

## 합계

```wast
local total = reduce(
    [1, 2, 3, 4, 5],
    0,
    fn(acc, x) (
        acc + x
    )
)
```

---

# 94. 전체 키워드

현재 Wast2가 사용하는 주요 키워드:

```text
fn
local
const

if
elseif
else

while
for
in

return
break
continue

true
false
nil

and
or
not

struct
enum
match
library
```

---

# 95. 연산자 전체

```text
+
-
*
/
%

==
!=
<
<=
>
>=

=
+=
-=
*=
/=
%=

&&
||
!

&
|

..
..=

|>

.
->
```

---

# 96. 괄호와 자료구조 문법

함수 호출:

```wast
foo()
```

함수 블록:

```wast
fn foo() (
    print("hello")
)
```

조건 블록:

```wast
if x > 0 (
    print("positive")
)
```

배열:

```wast
[1, 2, 3]
```

Map:

```wast
{
    name: "Alice",
    age: 20
}
```

인덱스:

```wast
array[0]
```

필드:

```wast
object.name
```

---

# 97. 에러 이해하기

Wast2는 오류가 발생하면 대략 다음 형태의 정보를 출력합니다.

```text
[error] line 3:5: ...
  hint: ...
```

즉 다음 정보를 확인하면 됩니다.

```text
line → 몇 번째 줄인가?
column → 몇 번째 위치인가?
message → 무엇이 잘못되었는가?
hint → 어떤 문법을 기대했는가?
```

---

# 98. 흔한 오류

## 함수 블록을 닫지 않음

잘못된 코드:

```wast
fn test() (
    print("hello")
```

올바른 코드:

```wast
fn test() (
    print("hello")
)
```

---

## 배열 괄호 오류

잘못된 코드:

```wast
local x = [1, 2, 3
```

올바른 코드:

```wast
local x = [1, 2, 3]
```

---

## Map의 `:` 누락

잘못된 코드:

```wast
local user = {
    name "Alice"
}
```

올바른 코드:

```wast
local user = {
    name: "Alice"
}
```

---

## 함수 매개변수 사이의 comma 누락

잘못된 코드:

```wast
fn add(a b) (
    a + b
)
```

올바른 코드:

```wast
fn add(a, b) (
    a + b
)
```

---

# 99. Wast2로 만들 수 있는 것

Wast2는 단순한 계산 스크립트뿐 아니라 다음과 같은 프로그램에 사용할 수 있습니다.

```text
자동화
데이터 처리
게임 스크립트
AI 알고리즘
수치 계산
프로토타이핑
CLI 프로그램
개발 도구
스크립팅
C++ 기반 프로그램 확장
```

특히 `library`를 이용하면 Wast2 자체에 없는 저수준 기능을 C++23으로 확장할 수 있습니다.

---

# 100. Wast2를 제대로 배우는 순서

처음 배우는 경우 다음 순서를 추천합니다.

```text
1. print()
2. 변수
3. 기본 타입
4. 연산자
5. if / elseif / else
6. while
7. for
8. array
9. map
10. 함수
11. return
12. lambda
13. map / filter / reduce
14. pipe
15. struct
16. enum
17. match
18. 문자열 보간
19. library
20. AI / 데이터 처리
```

---

# 101. 첫 번째 프로젝트

간단한 평균 계산기를 만들어 봅니다.

```wast
fn average(values) (
    local total = reduce(
        values,
        0.0,
        fn(acc, x) (
            acc + x
        )
    )

    return total / len(values)
)

fn main() (
    local scores = [80.0, 90.0, 100.0]

    local result = average(scores)

    print("Average: {result}")
)
```

---

# 102. 두 번째 프로젝트

짝수만 골라 제곱합니다.

```wast
fn square(x) (
    x * x
)

fn main() (
    local numbers = [1, 2, 3, 4, 5, 6]

    local even = filter(
        numbers,
        fn(x) (
            x % 2 == 0
        )
    )

    local squared = map(
        even,
        square
    )

    print(squared)
)
```

결과:

```text
[4, 16, 36]
```

---

# 103. 세 번째 프로젝트

간단한 데이터 모델을 만듭니다.

```wast
struct Player(
    name: str,
    score: i64
)

fn main() (
    local players = [
        Player("Alice", 100),
        Player("Bob", 80),
        Player("Charlie", 120)
    ]

    for player in players (
        print("{player.name}: {player.score}")
    )
)
```

---

# 104. 네 번째 프로젝트 — 간단한 AI 계산

```wast
fn predict(x, weight, bias) (
    x * weight + bias
)

fn main() (
    local weight = 2.5
    local bias = 1.0

    local inputs = [1.0, 2.0, 3.0, 4.0]

    local outputs = map(
        inputs,
        fn(x) (
            predict(x, weight, bias)
        )
    )

    print(outputs)
)
```

이 구조를 확장하면 더 복잡한 수치 모델과 데이터 처리 알고리즘을 작성할 수 있습니다.

---

# 105. Wast2의 핵심 철학

Wast2를 사용할 때 가장 중요한 것은 문법을 많이 외우는 것이 아닙니다.

핵심은 다음 네 가지입니다.

```text
값
 ↓
함수
 ↓
데이터
 ↓
조합
```

예를 들어:

```wast
data
    |> filter(...)
    |> map(...)
    |> reduce(...)
```

처럼 데이터를 함수 사이에서 이동시키면서 프로그램을 구성할 수 있습니다.

---

# 106. 마지막으로

Wast2는 작은 문법으로 시작할 수 있지만 단순한 장난감 언어에 머무르는 것을 목표로 하지 않습니다.

```text
간단한 스크립트
       ↓
일반 프로그램
       ↓
데이터 처리
       ↓
고차 함수
       ↓
AI 알고리즘
       ↓
C++23 확장
```

이 모든 단계를 하나의 언어 안에서 연결하는 것이 Wast2의 핵심입니다.

**Wast2를 배우는 가장 좋은 방법은 직접 코드를 작성하는 것입니다.**

작은 프로그램부터 시작하세요.

```wast
print("Hello, Wast2!")
```

그리고 변수, 함수, 배열, Map, 반복문, Lambda, Pipe를 하나씩 추가하세요.

```text
작게 시작한다.
기능을 조합한다.
복잡한 프로그램을 만든다.
```

# Wast2

**Small syntax. Powerful programs.**
