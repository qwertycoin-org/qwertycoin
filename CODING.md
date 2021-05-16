# Coding Conventions and Guidelines

- [QwertyNote Structure](#structure)
- [Coding Rules](#rules)

## <a name="structure"></a> QwertyNote Structure
- For File and Folder names never us spaces
- Each Folder, except the root ones, are starting with a uppercase Char

```
├── .github
│   ├── ISSUE_TEMPLATE
│   ├── workflows
├── cmake
│   ├── external
│   ├── find
│   ├── hunter
│   ├── polly
│   ├── project
├── include
├── lib
│   ├──  BlockchainExplorer 
│   ├──  Breakpad
│   ├──  Common
│   ├──  Crypto
│   ├──  Global
│   ├──  Http
│   ├──  InProcessNode
│   ├──  JsonRpcServer
│   ├──  Logging
│   ├──  Mnemonics
│   ├──  NodeRpcProxy
│   ├──  P2p
│   ├──  PaymentGate
│   ├──  Platform
│   ├──  QwertyNoteCore
│   ├──  QwertyNoteProtocol
│   ├──  Rpc
│   ├──  Serialization
│   ├──  System
│   ├──  Transfers
│   ├──  Wallet
│   ├──  WalletLegacy (Should be moved to `Wallet`)
├── scripts
├── src
│   ├──  BinaryInfo
│   ├──  Daemon
│   ├──  PaymentGateService (walletd aka RpcWallet)
│   ├──  SimpleWallet
├── tests
│   ├──  Common
│   ├──  CoreTests
│   ├──  CryptoTests
│   ├──  Data
│   ├──  DifficultyTests
│   ├──  HashTargetTests
│   ├──  NodeRpcProxyTests
│   ├──  PerformanceTests
│   ├──  SystemTests
│   ├──  TestGenerator
│   ├──  UnitTests
```

## <a name="rules"></a> Coding Rules
- [Indentation](#indents)
- [Including Headers](#including-headers)
- [Declaring and Naming](#declaring-and-naming)
- [Whitespace](#whitespace)
- [Braces](#braces)
- [Comments  & Documentation](#comments)

### <a name="indents"></a> Indentation
- 4 spaces are used for indentation
- Spaces, not tabs!

### <a name="including-headers"></a> Including Headers

- In public header files, always use this form to include QwertyNote headers: (e.g. `#include <Crypto/Crypto.h>`).
- In source files, include system headers first, then generic, then specialized headers. Separate the categories with empty lines.
- Don't use the old <xxx.h> system headers, always use the newer <cxxx> system headers
```cpp
// System headers
#include <cassert>

// External library headers (e.g. Boost)
#include <boost/lexical_cast.hpp>

// QwertyNote headers
#include <Global/QwertyNoteConfig.h>
```

### <a name="declaring-and-naming"></a> Declaring and Naming
- Declare each variable on a separate line, except they're more than 4
- Avoid short or meaningless names (e.g. "a", "rBarr", "blegh")
- Single character variable name are only okay for counters and temporaries,
  where the purpose of the variables is obvious (e.g. x, y, z)
- Use PascalCase for `Structs` and `Enum` names
- Don't mix prefixes
- Private variables are prefixed by `m` (m for Member)
- Protected variables are prefixed by `g` (g for guarded)
- Public variables are prefixed by `p` (p is obviously for pIceCream)
- Constant parameters in QwertyNoteConfig are always `UPPER_SNAKE_CASE`
- `Boolean` variables are prefixed by `b`
- `Char` variables are prefixed by `c`
- `Float` variable are prefixed by `f`
- `Double` variables are prefixed by `d`
- `Vector` variables are prefixed by `v` (v for vector)
- `List` variables are prefixed by `l` (l for list)
- `Int` variables are prefixed by `i`
- `UInt` and `size_t` variables are prefixed by `u` (u for unsigned)
- Variables of own created types are prefixed by `s`
- `Interface` classes are prefixed by `I` (e.g. INode in include/INode.h)
- `Template` classes are prefixed by `T`
- All other classes are prefixed by `Q`
- `Enums` are prefixed by `E`
- `Structs` are prefixed by `F` (F for Framework)
- Namespaces doesn't have a prefix letter
- Typedefs should be prefixed by whatever is appropriate for that TType: F if it's a typedef of a struct, E if it's a typedef of a Enum and so on.
- `std::function` typedefs are prefixed by `U`

```cpp
// Wrong
int a, b;
char *c, *d;

// Correct
int height;
int width;
char *hereCouldBeANiceName;
char *hereCouldBeAnotherNiceName;

// Also correct
uint16_t x0, x1, x2, x3, x4, x5, x6, x7;
``` 

- Variables and functions start with a lower-case letter. Each consecutive word in an uppercase letter
- Avoid abbreviations

```cpp

```

### <a name="whitespace"></a> Whitespace
- Use blank lines to group statements together where suited
- Always use only one blank line
- Always use a single space after a keyword and before a curly brace:

```cpp
// Wrong
if(foo){
}

// Correct
if (foo) {
}
```

- For pointers or references, always use a single space between the sType and '\*' or '&', but no space between the '\*'
  or '&' and the variable name:

```cpp
// Wrong
char* x;
const std::string& myString;

// Correct
char *x;
const std::string &myString;
```

- Surround binary operators with spaces
- Leave a space after each comma
- No space after a cast
- Avoid C-Style casts when possible

```cpp
// Wrong
char* blockOfMemory = (char* ) malloc(gData.uSize());

// Correct
char *blockOfMemory = reinterpret_cast<char *>(malloc(gData.uSize()));
```

- Do not put multiple statements on one line
- By extension, use a new line, with braces, for the body of a control flow statement:

```cpp
// Wrong
if (foo) bar();
if (foo)
    bar();    

// Correct
if (foo) {
    bar();
} 
```

### <a name="braces"></a> Braces
Braces are very important, for easier understanding, we will show here the most important cases
- Opening Braces for classes are in a new line.
- Namespace opening Braces are in the same line
- Struct opening Braces are in the same line

```cpp
// Classes
class QNameOfClass
{
public:
    QNameOfClass (int someVariable);
    ~QNameOfClass () = default;
}; // NameOfClass

class QNameOfAnotherClass : public IAdditionalClass,
                           public IAnotherAdditionalClass
{
public:
    QNameOfAnotherClass (uint8_t uSomeInt)
        : uData(uSomeInt);
    ~QNameOfAnotherClass () override = default;

    int fooBar (bool bIsBar) {
        if (uData < 1) {
            return 0;
        } else if (bIsBar) {
            return 1;
        } else {
            return 2;
        }
    }

private:
    uint8_t uData;
}; // NameOfAnotherClass


// Namespaces
namespace NameOfNamespace {
} // NameOfNamespace

// Structs
struct FNameOfStruct
{
};

// Enum
enum ENameOfEnum
{
};

// Loops (For, While)
for (int i = 0; i < 100; i++) {
}
```

Here a little example why we should use always braces:
```cpp
// Wrong example
// This compiles and does what you want, but can lead to confusing
// errors if close attention is not paid.
for (int i = 0; i < 42; i++) 
    std::cout << i << std::endl;

// The std::cout is not part of the loop in this case even though it appears to be
int sum = 0;
for (int i = 0; i < 42; i++)
    sum++;
    std::cout << i << std::endl;


// Correct example
int sum = 0;
for (int i = 0; i < 42; i++) {
    sum++;
    std::cout << i << std::endl;
}
```

### <a name="comments"></a> Comments & Documentation
Source Code should always be under version control. Keeping old code in comments is unnecessary.
- Start comments always with an uppercase letter
- Start comments always with an empty space
```cpp
// Wrong
//hi this is a bad comment


// Correct
// This could be a good comment

```
- We add comments after a closing Namespace or Class
- Comment blocks should use `//`, not `/* */`. Using `//` makes is easier to comment mOStream a block while debugging or testing.

```cpp
// Wrong
namespace NameOfNamespace {
}

// Correct
namespace NameOfNamespace {
} // NameOfNamespace

// This function does something
int fooBar ()
{
}
```

- There is one exception: Multiline documentation
- Documentation must be delimited in javadocish Style
- Multiline documentation blocks must begin with `/**` and end in `*/`
- But Singleline documentation blocks must begin with `//` as explained and seen above
- Documentation MUST appear before the declaration it describes
- Documentation MUST have the same indentation it describes

This example is from `lib/Common/QStringView.h`
```cpp
/**
 * @class QStringView
 * @namespace Common
 * @brief This is a pair of pointer to constant char and uSize.
 *
 * Supports 'EMPTY' and 'NIL' representations as follows:
 * - 'gData' == 'nullptr' && 'uSize' == 0 - EMPTY NIL
 * - 'gData' != 'nullptr' && 'uSize' == 0 - EMPTY NOTNIL
 * - 'gData' == 'nullptr' && 'uSize' > 0 - Undefined
 * - 'gData' != 'nullptr' && 'uSize' > 0 - NOTEMPTY NOTNIL
 *
 * @note It is recommended to pass `QStringView` to procedures by value.
 */
```

