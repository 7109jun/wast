// wastc_v6.cpp
// Single-file wast -> C++23 transpiler.
// No external dependencies beyond a C++23 standard library.
//
// Build:
//   g++ -std=c++23 -O3 -march=native -flto -funroll-loops wastc_v6.cpp -o wastc
//
// Usage:
//   ./wastc input.wast output.cpp
//
// New syntax:
//   library add(a: i64, b: i64) -> i64 """
//       return a + b;
//   """
//
// The C++23 code inside triple quotes is embedded into generated C++.

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_set>
#include <memory>
#include <stdexcept>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <print>

using namespace std;

// ============================================================
// Diagnostics
// ============================================================

static string formatError(int line, int col, const string& msg, const string& nearText = "") {
    ostringstream os;
    os << "[error] line " << line << ":" << col << ": " << msg;
    if (!nearText.empty()) os << " (near '" << nearText << "')";
    os << "\n";
    os << "  hint: wast syntax uses fn name(args) ( ... ), library name(args) -> ret \"\"\"C++23 code\"\"\"";
    return os.str();
}

struct CompileError : public runtime_error {
    CompileError(const string& msg) : runtime_error(msg) {}
};

[[noreturn]] static void fail(const string& msg) {
    throw CompileError(msg);
}

// ============================================================
// Token
// ============================================================

enum class TT {
    Int, Float, Str, Ident, RawCode,

    Fn, Local, Const, If, ElseIf, Else, While, For, In,
    Return, Break, Continue, True, False, Nil,
    And, Or, Not, Struct, Enum, Match, Library,

    Plus, Minus, Star, Slash, Percent,
    Eq, Ne, Lt, Le, Gt, Ge,
    Assign, PlusAssign, MinusAssign, StarAssign, SlashAssign, PercentAssign,
    Range, RangeIncl,
    BitAnd, BitOr, Pipe,

    LParen, RParen, LBracket, RBracket, LBrace, RBrace,
    Comma, Colon, Semi, Dot, Arrow,

    End, Error
};

struct Token {
    TT type;
    string text;
    int line;
    int col;
};

// ============================================================
// Lexer
// ============================================================

struct Lexer {
    string src;
    size_t pos = 0;
    int line = 1;
    int col = 1;

    Lexer(const string& source) : src(source) {}

    char cur() const {
        return pos < src.size() ? src[pos] : '\0';
    }

    char peek() const {
        return pos + 1 < src.size() ? src[pos + 1] : '\0';
    }

    char advance() {
        char c = cur();
        if (c == '\n') {
            line++;
            col = 1;
        } else {
            col++;
        }
        pos++;
        return c;
    }

    bool atEnd() const {
        return pos >= src.size();
    }

    void skipWhitespaceAndComments() {
        while (!atEnd()) {
            char c = cur();

            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                advance();
                continue;
            }

            if (c == '/' && peek() == '/') {
                while (!atEnd() && cur() != '\n') advance();
                continue;
            }

            break;
        }
    }

    Token makeToken(TT type, const string& text, int l, int c) {
        return Token{type, text, l, c};
    }

    bool isIdentStart(char c) const {
        return std::isalpha((unsigned char)c) || c == '_';
    }

    bool isIdentChar(char c) const {
        return std::isalnum((unsigned char)c) || c == '_';
    }

    vector<Token> tokenize() {
        vector<Token> tokens;

        while (true) {
            skipWhitespaceAndComments();
            if (atEnd()) break;

            char c = cur();
            int l = line;
            int co = col;

            if (std::isdigit((unsigned char)c) || (c == '.' && std::isdigit((unsigned char)peek()))) {
                string num;
                bool isFloat = false;

                while (!atEnd()) {
                    char ch = cur();

                    if (std::isdigit((unsigned char)ch)) {
                        num += advance();
                    } else if (ch == '.' && !isFloat && std::isdigit((unsigned char)peek())) {
                        isFloat = true;
                        num += advance();
                    } else if (ch == '_') {
                        advance();
                    } else {
                        break;
                    }
                }

                tokens.push_back(makeToken(isFloat ? TT::Float : TT::Int, num, l, co));
                continue;
            }

            if (c == '"') {
                // Triple-quoted raw C++23 code block:
                // """
                //   C++23 code
                // """
                if (pos + 2 < src.size() && src[pos + 1] == '"' && src[pos + 2] == '"') {
                    advance(); // first "
                    advance(); // second "
                    advance(); // third "

                    string code;
                    bool closed = false;

                    while (!atEnd()) {
                        if (cur() == '"' && pos + 2 < src.size() && src[pos + 1] == '"' && src[pos + 2] == '"') {
                            advance();
                            advance();
                            advance();
                            closed = true;
                            break;
                        }

                        code += advance();
                    }

                    if (!closed) {
                        fail(formatError(l, co, "unterminated raw C++23 library block; expected closing \"\"\""));
                    }

                    tokens.push_back(makeToken(TT::RawCode, code, l, co));
                    continue;
                }

                // Normal string
                advance();
                string s;

                while (!atEnd() && cur() != '"') {
                    if (cur() == '\\') {
                        advance();
                        if (atEnd()) fail(formatError(l, co, "unterminated string escape"));
                        char e = advance();
                        switch (e) {
                            case 'n': s += '\n'; break;
                            case 't': s += '\t'; break;
                            case 'r': s += '\r'; break;
                            case '\\': s += '\\'; break;
                            case '"': s += '"'; break;
                            case '0': s += '\0'; break;
                            default: s += e; break;
                        }
                    } else {
                        s += advance();
                    }
                }

                if (atEnd()) fail(formatError(l, co, "unterminated string literal; expected closing '\"'"));

                advance();
                tokens.push_back(makeToken(TT::Str, s, l, co));
                continue;
            }

            if (isIdentStart(c)) {
                string id;
                while (!atEnd() && isIdentChar(cur())) id += advance();

                TT type = TT::Ident;

                if (id == "fn") type = TT::Fn;
                else if (id == "local") type = TT::Local;
                else if (id == "const") type = TT::Const;
                else if (id == "if") type = TT::If;
                else if (id == "elseif") type = TT::ElseIf;
                else if (id == "else") type = TT::Else;
                else if (id == "while") type = TT::While;
                else if (id == "for") type = TT::For;
                else if (id == "in") type = TT::In;
                else if (id == "return") type = TT::Return;
                else if (id == "break") type = TT::Break;
                else if (id == "continue") type = TT::Continue;
                else if (id == "true") type = TT::True;
                else if (id == "false") type = TT::False;
                else if (id == "nil") type = TT::Nil;
                else if (id == "and") type = TT::And;
                else if (id == "or") type = TT::Or;
                else if (id == "not") type = TT::Not;
                else if (id == "struct") type = TT::Struct;
                else if (id == "enum") type = TT::Enum;
                else if (id == "match") type = TT::Match;
                else if (id == "library") type = TT::Library;

                tokens.push_back(makeToken(type, id, l, co));
                continue;
            }

            advance();

            switch (c) {
                case '+':
                    if (cur() == '=') {
                        advance();
                        tokens.push_back(makeToken(TT::PlusAssign, "+=", l, co));
                    } else {
                        tokens.push_back(makeToken(TT::Plus, "+", l, co));
                    }
                    break;

                case '-':
                    if (cur() == '>') {
                        advance();
                        tokens.push_back(makeToken(TT::Arrow, "->", l, co));
                    } else if (cur() == '=') {
                        advance();
                        tokens.push_back(makeToken(TT::MinusAssign, "-=", l, co));
                    } else {
                        tokens.push_back(makeToken(TT::Minus, "-", l, co));
                    }
                    break;

                case '*':
                    if (cur() == '=') {
                        advance();
                        tokens.push_back(makeToken(TT::StarAssign, "*=", l, co));
                    } else {
                        tokens.push_back(makeToken(TT::Star, "*", l, co));
                    }
                    break;

                case '/':
                    if (cur() == '=') {
                        advance();
                        tokens.push_back(makeToken(TT::SlashAssign, "/=", l, co));
                    } else {
                        tokens.push_back(makeToken(TT::Slash, "/", l, co));
                    }
                    break;

                case '%':
                    if (cur() == '=') {
                        advance();
                        tokens.push_back(makeToken(TT::PercentAssign, "%=", l, co));
                    } else {
                        tokens.push_back(makeToken(TT::Percent, "%", l, co));
                    }
                    break;

                case '=':
                    if (cur() == '=') {
                        advance();
                        tokens.push_back(makeToken(TT::Eq, "==", l, co));
                    } else {
                        tokens.push_back(makeToken(TT::Assign, "=", l, co));
                    }
                    break;

                case '!':
                    if (cur() == '=') {
                        advance();
                        tokens.push_back(makeToken(TT::Ne, "!=", l, co));
                    } else {
                        tokens.push_back(makeToken(TT::Not, "!", l, co));
                    }
                    break;

                case '<':
                    if (cur() == '=') {
                        advance();
                        tokens.push_back(makeToken(TT::Le, "<=", l, co));
                    } else {
                        tokens.push_back(makeToken(TT::Lt, "<", l, co));
                    }
                    break;

                case '>':
                    if (cur() == '=') {
                        advance();
                        tokens.push_back(makeToken(TT::Ge, ">=", l, co));
                    } else {
                        tokens.push_back(makeToken(TT::Gt, ">", l, co));
                    }
                    break;

                case '&':
                    if (cur() == '&') {
                        advance();
                        tokens.push_back(makeToken(TT::And, "&&", l, co));
                    } else {
                        tokens.push_back(makeToken(TT::BitAnd, "&", l, co));
                    }
                    break;

                case '|':
                    if (cur() == '|') {
                        advance();
                        tokens.push_back(makeToken(TT::Or, "||", l, co));
                    } else if (cur() == '>') {
                        advance();
                        tokens.push_back(makeToken(TT::Pipe, "|>", l, co));
                    } else {
                        tokens.push_back(makeToken(TT::BitOr, "|", l, co));
                    }
                    break;

                case '.':
                    if (cur() == '.') {
                        advance();
                        if (cur() == '=') {
                            advance();
                            tokens.push_back(makeToken(TT::RangeIncl, "..=", l, co));
                        } else {
                            tokens.push_back(makeToken(TT::Range, "..", l, co));
                        }
                    } else {
                        tokens.push_back(makeToken(TT::Dot, ".", l, co));
                    }
                    break;

                case '(': tokens.push_back(makeToken(TT::LParen, "(", l, co)); break;
                case ')': tokens.push_back(makeToken(TT::RParen, ")", l, co)); break;
                case '[': tokens.push_back(makeToken(TT::LBracket, "[", l, co)); break;
                case ']': tokens.push_back(makeToken(TT::RBracket, "]", l, co)); break;
                case '{': tokens.push_back(makeToken(TT::LBrace, "{", l, co)); break;
                case '}': tokens.push_back(makeToken(TT::RBrace, "}", l, co)); break;
                case ',': tokens.push_back(makeToken(TT::Comma, ",", l, co)); break;
                case ':': tokens.push_back(makeToken(TT::Colon, ":", l, co)); break;
                case ';': tokens.push_back(makeToken(TT::Semi, ";", l, co)); break;

                default:
                    tokens.push_back(makeToken(TT::Error, string(1, c), l, co));
                    break;
            }
        }

        tokens.push_back(makeToken(TT::End, "", line, col));
        return tokens;
    }
};

// ============================================================
// AST
// ============================================================

struct Node {
    enum Kind {
        Program,
        FuncDef,
        StructDecl,
        EnumDecl,
        LibraryFunc,
        RawCode,
        Param,
        TypeAnn,
        VarDecl,
        Block,
        If,
        While,
        ForNum,
        ForIn,
        Return,
        Break,
        Continue,
        ExprStmt,
        Assign,
        BinOp,
        UnaryOp,
        Call,
        Index,
        Field,
        Ident,
        IntLit,
        FloatLit,
        StrLit,
        BoolLit,
        NilLit,
        ArrayLit,
        MapLit,
        Pair,
        Lambda,
        Pipe,
        Match,
        MatchArm,
        Wildcard,
        PatternAlt
    };

    Kind kind;
    string sval;
    int64_t ival = 0;
    double fval = 0.0;
    bool bval = false;
    int line = 0;
    int col = 0;

    vector<Node*> children;

    Node(Kind k, int l, int c) : kind(k), line(l), col(c) {}

    ~Node() {
        clear();
    }

    void clear() {
        for (auto child : children) delete child;
        children.clear();
    }

    Node* add(Node* n) {
        children.push_back(n);
        return n;
    }
};

// ============================================================
// Parser
// ============================================================

struct Parser {
    vector<Token> tokens;
    size_t pos = 0;

    Parser(vector<Token> toks) : tokens(std::move(toks)) {}

    Token cur() const {
        return tokens[pos];
    }

    Token peek() const {
        return pos + 1 < tokens.size() ? tokens[pos + 1] : tokens.back();
    }

    Token advance() {
        return tokens[pos++];
    }

    bool check(TT type) const {
        return cur().type == type;
    }

    bool match(TT type) {
        if (check(type)) {
            advance();
            return true;
        }
        return false;
    }

    [[noreturn]] void error(const string& msg) {
        Token t = cur();
        fail(formatError(t.line, t.col, msg, t.text));
    }

    Token expect(TT type, const string& msg) {
        if (!check(type)) error(msg);
        return advance();
    }

    Node* parseProgram() {
        Node* program = new Node(Node::Program, 1, 1);

        while (!check(TT::End)) {
            while (match(TT::Semi)) {}
            if (check(TT::End)) break;
            program->add(parseStatement());
        }

        return program;
    }

    Node* parseStatement() {
        if (check(TT::Library)) return parseLibrary();

        if (check(TT::Fn)) {
            if (peek().type == TT::Ident) return parseFuncDef();
            return parseExprStatement();
        }

        if (check(TT::Struct)) return parseStructDecl();
        if (check(TT::Enum)) return parseEnumDecl();
        if (check(TT::Match)) return parseMatch();

        if (check(TT::Local) || check(TT::Const)) return parseVarDecl();
        if (check(TT::If)) return parseIf();
        if (check(TT::While)) return parseWhile();
        if (check(TT::For)) return parseFor();
        if (check(TT::Return)) return parseReturn();

        if (check(TT::Break)) {
            Token t = advance();
            return new Node(Node::Break, t.line, t.col);
        }

        if (check(TT::Continue)) {
            Token t = advance();
            return new Node(Node::Continue, t.line, t.col);
        }

        return parseExprStatement();
    }

    Node* parseLibrary() {
        Token libTok = expect(TT::Library, "expected 'library'");
        Node* lib = new Node(Node::LibraryFunc, libTok.line, libTok.col);

        Token nameTok = expect(TT::Ident, "expected library function name after 'library'");
        lib->sval = nameTok.text;

        expect(TT::LParen, "expected '(' after library function name");

        while (!check(TT::RParen)) {
            Token pTok = expect(TT::Ident, "expected parameter name in library function");
            Node* param = new Node(Node::Param, pTok.line, pTok.col);
            param->sval = pTok.text;

            if (match(TT::Colon)) {
                Token typeTok = expect(TT::Ident, "expected parameter type after ':'");
                Node* typeAnn = new Node(Node::TypeAnn, typeTok.line, typeTok.col);
                typeAnn->sval = typeTok.text;
                param->add(typeAnn);
            }

            lib->add(param);

            if (!check(TT::RParen)) {
                expect(TT::Comma, "expected ',' between library function parameters");
            }
        }

        expect(TT::RParen, "expected ')' after library function parameters");
        expect(TT::Arrow, "expected '->' after library function parameters");

        Token retTok = expect(TT::Ident, "expected return type after '->'");
        Node* retType = new Node(Node::TypeAnn, retTok.line, retTok.col);
        retType->sval = retTok.text;
        lib->add(retType);

        Token codeTok = expect(TT::RawCode,
            "expected C++23 code in triple quotes \"\"\" ... \"\"\" after library return type");

        Node* raw = new Node(Node::RawCode, codeTok.line, codeTok.col);
        raw->sval = codeTok.text;
        lib->add(raw);

        return lib;
    }

    Node* parseFuncDef() {
        Token fnTok = expect(TT::Fn, "expected 'fn' to start function definition");
        Node* fn = new Node(Node::FuncDef, fnTok.line, fnTok.col);

        Token nameTok = expect(TT::Ident, "expected function name after 'fn'");
        fn->sval = nameTok.text;

        expect(TT::LParen, "expected '(' after function name");

        while (!check(TT::RParen)) {
            Token pTok = expect(TT::Ident, "expected parameter name");
            Node* param = new Node(Node::Param, pTok.line, pTok.col);
            param->sval = pTok.text;

            if (match(TT::Colon)) {
                Token typeTok = expect(TT::Ident, "expected parameter type after ':'");
                Node* typeAnn = new Node(Node::TypeAnn, typeTok.line, typeTok.col);
                typeAnn->sval = typeTok.text;
                param->add(typeAnn);
            }

            fn->add(param);

            if (!check(TT::RParen)) {
                expect(TT::Comma, "expected ',' between function parameters");
            }
        }

        expect(TT::RParen, "expected ')' after function parameters");

        if (match(TT::Arrow)) {
            Token retTok = expect(TT::Ident, "expected return type after '->'");
            Node* retType = new Node(Node::TypeAnn, retTok.line, retTok.col);
            retType->sval = retTok.text;
            fn->add(retType);
        }

        expect(TT::LParen, "expected '(' to open function body; wast uses ( ... ) blocks");
        fn->add(parseBlock());
        expect(TT::RParen, "expected ')' to close function body");

        return fn;
    }

    Node* parseStructDecl() {
        Token stTok = expect(TT::Struct, "expected 'struct'");
        Node* st = new Node(Node::StructDecl, stTok.line, stTok.col);

        Token nameTok = expect(TT::Ident, "expected struct name after 'struct'");
        st->sval = nameTok.text;

        expect(TT::LParen, "expected '(' after struct name");

        while (!check(TT::RParen)) {
            Token fieldTok = expect(TT::Ident, "expected field name in struct");
            Node* field = new Node(Node::Param, fieldTok.line, fieldTok.col);
            field->sval = fieldTok.text;

            if (match(TT::Colon)) {
                Token typeTok = expect(TT::Ident, "expected field type after ':'");
                Node* typeAnn = new Node(Node::TypeAnn, typeTok.line, typeTok.col);
                typeAnn->sval = typeTok.text;
                field->add(typeAnn);
            }

            st->add(field);

            if (!check(TT::RParen)) {
                expect(TT::Comma, "expected ',' between struct fields");
            }
        }

        expect(TT::RParen, "expected ')' to close struct declaration");
        return st;
    }

    Node* parseEnumDecl() {
        Token enumTok = expect(TT::Enum, "expected 'enum'");
        Node* e = new Node(Node::EnumDecl, enumTok.line, enumTok.col);

        Token nameTok = expect(TT::Ident, "expected enum name after 'enum'");
        e->sval = nameTok.text;

        expect(TT::Assign, "expected '=' after enum name");

        while (true) {
            Token variantTok = expect(TT::Ident, "expected enum variant name");
            Node* variant = new Node(Node::StrLit, variantTok.line, variantTok.col);
            variant->sval = variantTok.text;
            e->add(variant);

            if (match(TT::BitOr)) continue;
            break;
        }

        return e;
    }

    Node* parseMatch() {
        Token matchTok = expect(TT::Match, "expected 'match'");
        Node* m = new Node(Node::Match, matchTok.line, matchTok.col);

        m->add(parseExpression());

        expect(TT::LParen, "expected '(' after match expression");

        while (!check(TT::RParen)) {
            while (match(TT::Semi)) {}
            if (check(TT::RParen)) break;

            Node* arm = new Node(Node::MatchArm, cur().line, cur().col);
            arm->add(parsePattern());

            expect(TT::Arrow, "expected '->' in match arm");

            if (check(TT::LParen)) {
                advance();
                arm->add(parseBlock());
                expect(TT::RParen, "expected ')' to close match arm block");
            } else {
                arm->add(parseExpression());
            }

            m->add(arm);

            while (match(TT::Comma) || match(TT::Semi)) {}
        }

        expect(TT::RParen, "expected ')' to close match statement");
        return m;
    }

    Node* parsePattern() {
        Node* first = parsePatternBasic();

        if (check(TT::BitOr)) {
            Node* alt = new Node(Node::PatternAlt, first->line, first->col);
            alt->add(first);

            while (match(TT::BitOr)) {
                alt->add(parsePatternBasic());
            }

            return alt;
        }

        return first;
    }

    Node* parsePatternBasic() {
        if (check(TT::Ident)) {
            Token t = cur();

            if (t.text == "_") {
                advance();
                return new Node(Node::Wildcard, t.line, t.col);
            }

            if (peek().type == TT::Dot || peek().type == TT::LBracket || peek().type == TT::LParen) {
                return parseExpression();
            }

            advance();
            Node* s = new Node(Node::StrLit, t.line, t.col);
            s->sval = t.text;
            return s;
        }

        if (check(TT::Int) || check(TT::Float) || check(TT::Str) ||
            check(TT::True) || check(TT::False) || check(TT::Nil)) {
            return parsePrimary();
        }

        return parseExpression();
    }

    Node* parseVarDecl() {
        bool isConst = check(TT::Const);
        advance();

        Token nameTok = expect(TT::Ident, "expected variable name after 'local'/'const'");
        Node* decl = new Node(Node::VarDecl, nameTok.line, nameTok.col);
        decl->sval = nameTok.text;
        decl->bval = isConst;

        if (match(TT::Colon)) {
            expect(TT::Ident, "expected type name after ':'");
        }

        if (match(TT::Assign)) {
            decl->add(parseExpression());
        }

        return decl;
    }

    Node* parseIf() {
        Token ifTok = expect(TT::If, "expected 'if'");
        Node* node = new Node(Node::If, ifTok.line, ifTok.col);

        node->add(parseExpression());

        expect(TT::LParen, "expected '(' after if condition");
        node->add(parseBlock());
        expect(TT::RParen, "expected ')' to close if block");

        while (check(TT::ElseIf)) {
            Token elifTok = advance();
            Node* elif = new Node(Node::If, elifTok.line, elifTok.col);

            elif->add(parseExpression());
            expect(TT::LParen, "expected '(' after elseif condition");
            elif->add(parseBlock());
            expect(TT::RParen, "expected ')' to close elseif block");

            node->add(elif);
        }

        if (match(TT::Else)) {
            expect(TT::LParen, "expected '(' after else");
            node->add(parseBlock());
            expect(TT::RParen, "expected ')' to close else block");
        }

        return node;
    }

    Node* parseWhile() {
        Token whileTok = expect(TT::While, "expected 'while'");
        Node* node = new Node(Node::While, whileTok.line, whileTok.col);

        node->add(parseExpression());

        expect(TT::LParen, "expected '(' after while condition");
        node->add(parseBlock());
        expect(TT::RParen, "expected ')' to close while block");

        return node;
    }

    Node* parseFor() {
        Token forTok = expect(TT::For, "expected 'for'");
        Token varTok = expect(TT::Ident, "expected loop variable after 'for'");

        if (match(TT::Assign)) {
            Node* node = new Node(Node::ForNum, forTok.line, forTok.col);
            node->sval = varTok.text;

            node->add(parseExpression());
            expect(TT::Comma, "expected ',' in numeric for: for i = start, end");
            node->add(parseExpression());

            if (match(TT::Comma)) {
                node->add(parseExpression());
            } else {
                Node* step = new Node(Node::IntLit, forTok.line, forTok.col);
                step->ival = 1;
                node->add(step);
            }

            expect(TT::LParen, "expected '(' to open for body");
            node->add(parseBlock());
            expect(TT::RParen, "expected ')' to close for body");

            return node;
        }

        expect(TT::In, "expected 'in' or '=' after for variable");

        Node* node = new Node(Node::ForIn, forTok.line, forTok.col);
        node->sval = varTok.text;

        node->add(parseExpression());

        expect(TT::LParen, "expected '(' to open for-in body");
        node->add(parseBlock());
        expect(TT::RParen, "expected ')' to close for-in body");

        return node;
    }

    Node* parseReturn() {
        Token tok = expect(TT::Return, "expected 'return'");
        Node* node = new Node(Node::Return, tok.line, tok.col);

        if (!check(TT::RParen) && !check(TT::End) && !check(TT::Semi)) {
            node->add(parseExpression());
        }

        return node;
    }

    Node* parseExprStatement() {
        Node* node = new Node(Node::ExprStmt, cur().line, cur().col);
        node->add(parseExpression());
        return node;
    }

    Node* parseBlock() {
        Node* block = new Node(Node::Block, cur().line, cur().col);

        while (!check(TT::RParen) && !check(TT::End)) {
            while (match(TT::Semi)) {}
            if (check(TT::RParen) || check(TT::End)) break;
            block->add(parseStatement());
        }

        return block;
    }

    Node* parseExpression() {
        return parsePipe();
    }

    Node* parsePipe() {
        Node* left = parseAssignment();

        while (check(TT::Pipe)) {
            Token op = advance();
            Node* node = new Node(Node::Pipe, op.line, op.col);
            node->add(left);
            node->add(parseAssignment());
            left = node;
        }

        return left;
    }

    Node* parseAssignment() {
        Node* left = parseOr();

        if (check(TT::Assign) || check(TT::PlusAssign) || check(TT::MinusAssign) ||
            check(TT::StarAssign) || check(TT::SlashAssign) || check(TT::PercentAssign)) {
            Token op = advance();

            if (left->kind != Node::Ident && left->kind != Node::Index && left->kind != Node::Field) {
                fail(formatError(op.line, op.col,
                                 "invalid assignment target: only variable, index, or field can be assigned",
                                 op.text));
            }

            Node* assign = new Node(Node::Assign, op.line, op.col);
            assign->sval = op.text;
            assign->add(left);
            assign->add(parseAssignment());
            return assign;
        }

        return left;
    }

    Node* parseOr() {
        Node* left = parseAnd();

        while (check(TT::Or)) {
            Token op = advance();
            Node* node = new Node(Node::BinOp, op.line, op.col);
            node->sval = "||";
            node->add(left);
            node->add(parseAnd());
            left = node;
        }

        return left;
    }

    Node* parseAnd() {
        Node* left = parseEquality();

        while (check(TT::And)) {
            Token op = advance();
            Node* node = new Node(Node::BinOp, op.line, op.col);
            node->sval = "&&";
            node->add(left);
            node->add(parseEquality());
            left = node;
        }

        return left;
    }

    Node* parseEquality() {
        Node* left = parseComparison();

        while (check(TT::Eq) || check(TT::Ne)) {
            Token op = advance();
            Node* node = new Node(Node::BinOp, op.line, op.col);
            node->sval = op.text;
            node->add(left);
            node->add(parseComparison());
            left = node;
        }

        return left;
    }

    Node* parseComparison() {
        Node* left = parseRange();

        while (check(TT::Lt) || check(TT::Le) || check(TT::Gt) || check(TT::Ge)) {
            Token op = advance();
            Node* node = new Node(Node::BinOp, op.line, op.col);
            node->sval = op.text;
            node->add(left);
            node->add(parseRange());
            left = node;
        }

        return left;
    }

    Node* parseRange() {
        Node* left = parseAdditive();

        while (check(TT::Range) || check(TT::RangeIncl)) {
            Token op = advance();
            Node* node = new Node(Node::BinOp, op.line, op.col);
            node->sval = op.type == TT::Range ? ".." : "..=";
            node->add(left);
            node->add(parseAdditive());
            left = node;
        }

        return left;
    }

    Node* parseAdditive() {
        Node* left = parseMultiplicative();

        while (check(TT::Plus) || check(TT::Minus)) {
            Token op = advance();
            Node* node = new Node(Node::BinOp, op.line, op.col);
            node->sval = op.text;
            node->add(left);
            node->add(parseMultiplicative());
            left = node;
        }

        return left;
    }

    Node* parseMultiplicative() {
        Node* left = parseUnary();

        while (check(TT::Star) || check(TT::Slash) || check(TT::Percent)) {
            Token op = advance();
            Node* node = new Node(Node::BinOp, op.line, op.col);
            node->sval = op.text;
            node->add(left);
            node->add(parseUnary());
            left = node;
        }

        return left;
    }

    Node* parseUnary() {
        if (check(TT::Minus) || check(TT::Not)) {
            Token op = advance();
            Node* node = new Node(Node::UnaryOp, op.line, op.col);
            node->sval = op.type == TT::Minus ? "-" : "!";
            node->add(parseUnary());
            return node;
        }

        return parsePostfix();
    }

    Node* parsePostfix() {
        Node* expr = parsePrimary();

        while (true) {
            if (check(TT::LParen)) {
                Token l = advance();
                Node* call = new Node(Node::Call, l.line, l.col);
                call->add(expr);

                while (!check(TT::RParen)) {
                    call->add(parseExpression());

                    if (!check(TT::RParen)) {
                        expect(TT::Comma, "expected ',' between call arguments");
                    }
                }

                expect(TT::RParen, "expected ')' after call arguments");
                expr = call;
            } else if (check(TT::LBracket)) {
                Token l = advance();
                Node* index = new Node(Node::Index, l.line, l.col);
                index->add(expr);
                index->add(parseExpression());
                expect(TT::RBracket, "expected ']' after index expression");
                expr = index;
            } else if (check(TT::Dot)) {
                Token dot = advance();
                Token fieldTok = expect(TT::Ident, "expected field name after '.'");

                Node* field = new Node(Node::Field, dot.line, dot.col);
                field->sval = fieldTok.text;
                field->add(expr);
                expr = field;
            } else {
                break;
            }
        }

        return expr;
    }

    Node* parsePrimary() {
        Token t = cur();

        if (check(TT::Int)) {
            advance();
            Node* n = new Node(Node::IntLit, t.line, t.col);
            n->ival = stoll(t.text);
            return n;
        }

        if (check(TT::Float)) {
            advance();
            Node* n = new Node(Node::FloatLit, t.line, t.col);
            n->fval = stod(t.text);
            return n;
        }

        if (check(TT::Str)) {
            advance();
            Node* n = new Node(Node::StrLit, t.line, t.col);
            n->sval = t.text;
            return n;
        }

        if (check(TT::True)) {
            advance();
            Node* n = new Node(Node::BoolLit, t.line, t.col);
            n->bval = true;
            return n;
        }

        if (check(TT::False)) {
            advance();
            Node* n = new Node(Node::BoolLit, t.line, t.col);
            n->bval = false;
            return n;
        }

        if (check(TT::Nil)) {
            advance();
            return new Node(Node::NilLit, t.line, t.col);
        }

        if (check(TT::Ident)) {
            advance();
            Node* n = new Node(Node::Ident, t.line, t.col);
            n->sval = t.text;
            return n;
        }

        if (check(TT::LParen)) {
            advance();
            Node* expr = parseExpression();
            expect(TT::RParen, "expected ')' after grouped expression");
            return expr;
        }

        if (check(TT::LBracket)) {
            return parseArrayLiteral();
        }

        if (check(TT::LBrace)) {
            return parseMapLiteral();
        }

        if (check(TT::Fn)) {
            return parseLambda();
        }

        error("unexpected token in expression");
    }

    Node* parseLambda() {
        Token fnTok = expect(TT::Fn, "expected 'fn' for lambda");
        Node* lam = new Node(Node::Lambda, fnTok.line, fnTok.col);

        expect(TT::LParen, "expected '(' after lambda 'fn'");

        while (!check(TT::RParen)) {
            Token pTok = expect(TT::Ident, "expected lambda parameter name");
            Node* param = new Node(Node::Param, pTok.line, pTok.col);
            param->sval = pTok.text;

            if (match(TT::Colon)) {
                expect(TT::Ident, "expected lambda parameter type after ':'");
            }

            lam->add(param);

            if (!check(TT::RParen)) {
                expect(TT::Comma, "expected ',' between lambda parameters");
            }
        }

        expect(TT::RParen, "expected ')' after lambda parameters");
        expect(TT::LParen, "expected '(' to open lambda body");
        lam->add(parseBlock());
        expect(TT::RParen, "expected ')' to close lambda body");

        return lam;
    }

    Node* parseArrayLiteral() {
        Token l = expect(TT::LBracket, "expected '['");
        Node* arr = new Node(Node::ArrayLit, l.line, l.col);

        while (!check(TT::RBracket)) {
            arr->add(parseExpression());

            if (!check(TT::RBracket)) {
                expect(TT::Comma, "expected ',' between array elements");
            }
        }

        expect(TT::RBracket, "expected ']' to close array literal");
        return arr;
    }

    Node* parseMapLiteral() {
        Token l = expect(TT::LBrace, "expected '{'");
        Node* map = new Node(Node::MapLit, l.line, l.col);

        while (!check(TT::RBrace)) {
            Node* key = nullptr;

            if (check(TT::Ident) && peek().type == TT::Colon) {
                Token k = advance();
                key = new Node(Node::StrLit, k.line, k.col);
                key->sval = k.text;
            } else {
                key = parseExpression();
            }

            expect(TT::Colon, "expected ':' in map literal key/value pair");
            Node* value = parseExpression();

            Node* pair = new Node(Node::Pair, key->line, key->col);
            pair->add(key);
            pair->add(value);
            map->add(pair);

            if (!check(TT::RBrace)) {
                expect(TT::Comma, "expected ',' between map entries");
            }
        }

        expect(TT::RBrace, "expected '}' to close map literal");
        return map;
    }
};

// ============================================================
// Optimizer
// ============================================================

struct Optimizer {
    bool isNumericLit(Node* n) const {
        return n && (n->kind == Node::IntLit || n->kind == Node::FloatLit);
    }

    double numValue(Node* n) const {
        if (n->kind == Node::IntLit) return (double)n->ival;
        if (n->kind == Node::FloatLit) return n->fval;
        return 0.0;
    }

    void optimize(Node* n) {
        if (!n) return;

        for (auto child : n->children) {
            optimize(child);
        }

        if (n->kind == Node::BinOp) {
            foldBinary(n);
        } else if (n->kind == Node::UnaryOp) {
            foldUnary(n);
        }
    }

    void foldBinary(Node* n) {
        if (n->children.size() != 2) return;

        Node* l = n->children[0];
        Node* r = n->children[1];
        const string& op = n->sval;

        if (op == "+" && l->kind == Node::StrLit && r->kind == Node::StrLit) {
            string s = l->sval + r->sval;
            int line = n->line, col = n->col;
            n->clear();
            n->kind = Node::StrLit;
            n->sval = s;
            n->line = line;
            n->col = col;
            return;
        }

        if (l->kind == Node::StrLit && r->kind == Node::StrLit) {
            bool result = false;
            bool known = true;

            if (op == "==") result = (l->sval == r->sval);
            else if (op == "!=") result = (l->sval != r->sval);
            else if (op == "<") result = (l->sval < r->sval);
            else if (op == "<=") result = (l->sval <= r->sval);
            else if (op == ">") result = (l->sval > r->sval);
            else if (op == ">=") result = (l->sval >= r->sval);
            else known = false;

            if (known) {
                int line = n->line, col = n->col;
                n->clear();
                n->kind = Node::BoolLit;
                n->bval = result;
                n->line = line;
                n->col = col;
                return;
            }
        }

        if (l->kind == Node::BoolLit && r->kind == Node::BoolLit) {
            bool result = false;
            bool known = true;

            if (op == "&&") result = l->bval && r->bval;
            else if (op == "||") result = l->bval || r->bval;
            else known = false;

            if (known) {
                int line = n->line, col = n->col;
                n->clear();
                n->kind = Node::BoolLit;
                n->bval = result;
                n->line = line;
                n->col = col;
                return;
            }
        }

        if (!isNumericLit(l) || !isNumericLit(r)) return;

        bool bothInt = (l->kind == Node::IntLit && r->kind == Node::IntLit);
        double ld = numValue(l);
        double rd = numValue(r);

        if ((op == "/" || op == "%") && rd == 0.0) return;

        bool folded = true;
        bool asInt = bothInt;
        int64_t intResult = 0;
        double floatResult = 0.0;
        bool boolResult = false;
        bool isBool = false;

        if (op == "+") {
            if (asInt) intResult = l->ival + r->ival;
            else floatResult = ld + rd;
        } else if (op == "-") {
            if (asInt) intResult = l->ival - r->ival;
            else floatResult = ld - rd;
        } else if (op == "*") {
            if (asInt) intResult = l->ival * r->ival;
            else floatResult = ld * rd;
        } else if (op == "/") {
            if (asInt) intResult = l->ival / r->ival;
            else floatResult = ld / rd;
        } else if (op == "%") {
            if (asInt) intResult = l->ival % r->ival;
            else floatResult = fmod(ld, rd);
        } else if (op == "==") {
            boolResult = bothInt ? (l->ival == r->ival) : (ld == rd);
            isBool = true;
        } else if (op == "!=") {
            boolResult = bothInt ? (l->ival != r->ival) : (ld != rd);
            isBool = true;
        } else if (op == "<") {
            boolResult = bothInt ? (l->ival < r->ival) : (ld < rd);
            isBool = true;
        } else if (op == "<=") {
            boolResult = bothInt ? (l->ival <= r->ival) : (ld <= rd);
            isBool = true;
        } else if (op == ">") {
            boolResult = bothInt ? (l->ival > r->ival) : (ld > rd);
            isBool = true;
        } else if (op == ">=") {
            boolResult = bothInt ? (l->ival >= r->ival) : (ld >= rd);
            isBool = true;
        } else {
            folded = false;
        }

        if (!folded) return;

        int line = n->line;
        int col = n->col;
        n->clear();

        if (isBool) {
            n->kind = Node::BoolLit;
            n->bval = boolResult;
        } else if (asInt) {
            n->kind = Node::IntLit;
            n->ival = intResult;
        } else {
            n->kind = Node::FloatLit;
            n->fval = floatResult;
        }

        n->line = line;
        n->col = col;
    }

    void foldUnary(Node* n) {
        if (n->children.size() != 1) return;

        Node* child = n->children[0];

        if (n->sval == "-" && isNumericLit(child)) {
            int line = n->line, col = n->col;

            if (child->kind == Node::IntLit) {
                int64_t v = -child->ival;
                n->clear();
                n->kind = Node::IntLit;
                n->ival = v;
            } else {
                double v = -child->fval;
                n->clear();
                n->kind = Node::FloatLit;
                n->fval = v;
            }

            n->line = line;
            n->col = col;
            return;
        }

        if (n->sval == "!" && child->kind == Node::BoolLit) {
            bool v = !child->bval;
            int line = n->line, col = n->col;
            n->clear();
            n->kind = Node::BoolLit;
            n->bval = v;
            n->line = line;
            n->col = col;
        }
    }
};

// ============================================================
// Code Generator
// ============================================================

struct CodeGen {
    ostringstream out;
    int indent = 0;
    int uidCounter = 0;

    unordered_set<string> funcs;
    vector<Node*> funcNodes;
    vector<Node*> structNodes;
    vector<Node*> enumNodes;
    vector<Node*> libraryNodes;
    vector<Node*> topLevel;

    int newUid() {
        return ++uidCounter;
    }

    void emitIndent() {
        for (int i = 0; i < indent; i++) out << "    ";
    }

    [[noreturn]] void error(Node* n, const string& msg) {
        fail(formatError(n ? n->line : 0, n ? n->col : 0, msg));
    }

    string escapeCpp(const string& s) {
        string r;
        r.reserve(s.size());

        for (char c : s) {
            switch (c) {
                case '\\': r += "\\\\"; break;
                case '"': r += "\\\""; break;
                case '\n': r += "\\n"; break;
                case '\t': r += "\\t"; break;
                case '\r': r += "\\r"; break;
                default: r += c; break;
            }
        }

        return r;
    }

    string compile(Node* program) {
        collect(program);

        emitRuntime();
        out << "\nusing wast::Value;\n\n";

        for (Node* e : enumNodes) {
            out << "Value v_" << e->sval << ";\n";
        }

        for (const string& name : funcs) {
            out << "Value wastfn_" << name << "(std::vector<Value> args);\n";
        }
        out << "Value wast_entry(std::vector<Value> args);\n\n";

        for (Node* st : structNodes) {
            genStruct(st);
        }

        for (Node* lib : libraryNodes) {
            genLibrary(lib);
        }

        for (Node* fn : funcNodes) {
            genFunction(fn);
        }

        genEntry();

        out << "int main(int argc, char** argv) {\n";
        out << "    (void)argc; (void)argv;\n";
        out << "    try {\n";
        out << "        wast_entry(std::vector<Value>{});\n";
        out << "    } catch (const std::exception& e) {\n";
        out << "        std::cerr << \"wast runtime error: \" << e.what() << '\\n';\n";
        out << "        return 1;\n";
        out << "    }\n";
        out << "    return 0;\n";
        out << "}\n";

        return out.str();
    }

    void collect(Node* program) {
        for (Node* child : program->children) {
            if (child->kind == Node::FuncDef) {
                if (funcs.count(child->sval)) {
                    error(child, "duplicate function definition: '" + child->sval + "'");
                }
                funcs.insert(child->sval);
                funcNodes.push_back(child);
            } else if (child->kind == Node::StructDecl) {
                if (funcs.count(child->sval)) {
                    error(child, "duplicate struct/function name: '" + child->sval + "'");
                }
                funcs.insert(child->sval);
                structNodes.push_back(child);
            } else if (child->kind == Node::LibraryFunc) {
                if (funcs.count(child->sval)) {
                    error(child, "duplicate library/function name: '" + child->sval + "'");
                }
                funcs.insert(child->sval);
                libraryNodes.push_back(child);
            } else if (child->kind == Node::EnumDecl) {
                enumNodes.push_back(child);
            } else {
                topLevel.push_back(child);
            }
        }
    }

    void emitRuntime() {
        out << R"WASTRUNTIME(
// Generated by wastc v6.
// Embedded wast runtime: single-file, portable C++23.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <format>
#include <functional>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace wast {

struct Value;
using Array = std::vector<Value>;
using Map = std::vector<std::pair<Value, Value>>;

struct Value {
    enum class Type {
        Nil,
        Bool,
        Int,
        Float,
        Str,
        Array,
        Map,
        Func
    };

    Type type = Type::Nil;
    bool b = false;
    int64_t i = 0;
    double f = 0.0;
    std::string s;
    std::shared_ptr<Array> a;
    std::shared_ptr<Map> m;
    std::shared_ptr<void> c;

    Value() {}
    Value(bool bb) : type(Type::Bool), b(bb) {}
    Value(int64_t ii) : type(Type::Int), i(ii) {}
    Value(double ff) : type(Type::Float), f(ff) {}
    Value(const char* ss) : type(Type::Str), s(ss ? ss : "") {}
    Value(std::string ss) : type(Type::Str), s(std::move(ss)) {}
    Value(std::shared_ptr<Array> aa) : type(Type::Array), a(std::move(aa)) {}
    Value(std::shared_ptr<Map> mm) : type(Type::Map), m(std::move(mm)) {}
    Value(std::shared_ptr<void> cc) : type(Type::Func), c(std::move(cc)) {}

    bool truthy() const {
        switch (type) {
            case Type::Nil: return false;
            case Type::Bool: return b;
            case Type::Int: return i != 0;
            case Type::Float: return f != 0.0;
            case Type::Str: return !s.empty();
            case Type::Array: return a && !a->empty();
            case Type::Map: return m && !m->empty();
            case Type::Func: return c != nullptr;
        }
        return false;
    }

    std::string to_string() const {
        switch (type) {
            case Type::Nil:
                return "nil";
            case Type::Bool:
                return b ? "true" : "false";
            case Type::Int: {
                std::ostringstream os;
                os << i;
                return os.str();
            }
            case Type::Float: {
                std::ostringstream os;
                os << f;
                return os.str();
            }
            case Type::Str:
                return s;
            case Type::Array: {
                std::string r = "[";
                if (a) {
                    for (size_t idx = 0; idx < a->size(); ++idx) {
                        if (idx) r += ", ";
                        r += (*a)[idx].to_string();
                    }
                }
                r += "]";
                return r;
            }
            case Type::Map: {
                std::string r = "{";
                if (m) {
                    for (size_t idx = 0; idx < m->size(); ++idx) {
                        if (idx) r += ", ";
                        r += (*m)[idx].first.to_string();
                        r += ": ";
                        r += (*m)[idx].second.to_string();
                    }
                }
                r += "}";
                return r;
            }
            case Type::Func:
                return "<function>";
        }
        return "";
    }
};

struct Closure {
    virtual ~Closure() {}
    virtual Value call(std::vector<Value> args) const = 0;
};

template<class F>
Value make_closure(F f) {
    struct Impl : Closure {
        F f;
        Impl(F ff) : f(std::move(ff)) {}
        Value call(std::vector<Value> args) const override {
            return f(std::move(args));
        }
    };

    return Value(std::static_pointer_cast<void>(std::make_shared<Impl>(std::move(f))));
}

inline Value call(const Value& fn, std::vector<Value> args) {
    if (fn.type == Value::Type::Func && fn.c) {
        Closure* cl = static_cast<Closure*>(fn.c.get());
        return cl->call(std::move(args));
    }

    throw std::runtime_error("value is not callable: " + fn.to_string());
}

inline bool isNumeric(const Value& v) {
    return v.type == Value::Type::Int || v.type == Value::Type::Float;
}

inline double asDouble(const Value& v) {
    if (v.type == Value::Type::Int) return (double)v.i;
    if (v.type == Value::Type::Float) return v.f;
    throw std::runtime_error("expected numeric value, got: " + v.to_string());
}

inline int64_t asInt(const Value& v) {
    if (v.type == Value::Type::Int) return v.i;
    if (v.type == Value::Type::Float) return (int64_t)v.f;
    if (v.type == Value::Type::Bool) return v.b ? 1 : 0;
    throw std::runtime_error("expected integer value, got: " + v.to_string());
}

inline bool truthy(const Value& v) {
    return v.truthy();
}

inline Value make_array(std::vector<Value> v) {
    return Value(std::make_shared<Array>(std::move(v)));
}

inline Value make_map(std::vector<std::pair<Value, Value>> v) {
    return Value(std::make_shared<Map>(std::move(v)));
}

inline std::string concat(const std::vector<Value>& args) {
    std::string r;
    for (const auto& x : args) r += x.to_string();
    return r;
}

inline Value add(const Value& x, const Value& y) {
    if (x.type == Value::Type::Str || y.type == Value::Type::Str) {
        return Value(x.to_string() + y.to_string());
    }

    if (x.type == Value::Type::Array && y.type == Value::Type::Array) {
        auto arr = std::make_shared<Array>();
        if (x.a) arr->insert(arr->end(), x.a->begin(), x.a->end());
        if (y.a) arr->insert(arr->end(), y.a->begin(), y.a->end());
        return Value(arr);
    }

    if (!isNumeric(x) || !isNumeric(y)) {
        throw std::runtime_error("cannot add values: " + x.to_string() + " + " + y.to_string());
    }

    if (x.type == Value::Type::Int && y.type == Value::Type::Int) {
        return Value(x.i + y.i);
    }

    return Value(asDouble(x) + asDouble(y));
}

inline Value sub(const Value& x, const Value& y) {
    if (!isNumeric(x) || !isNumeric(y)) {
        throw std::runtime_error("cannot subtract values: " + x.to_string() + " - " + y.to_string());
    }
    if (x.type == Value::Type::Int && y.type == Value::Type::Int) {
        return Value(x.i - y.i);
    }
    return Value(asDouble(x) - asDouble(y));
}

inline Value mul(const Value& x, const Value& y) {
    if (!isNumeric(x) || !isNumeric(y)) {
        throw std::runtime_error("cannot multiply values: " + x.to_string() + " * " + y.to_string());
    }
    if (x.type == Value::Type::Int && y.type == Value::Type::Int) {
        return Value(x.i * y.i);
    }
    return Value(asDouble(x) * asDouble(y));
}

inline Value divide(const Value& x, const Value& y) {
    if (!isNumeric(x) || !isNumeric(y)) {
        throw std::runtime_error("cannot divide values: " + x.to_string() + " / " + y.to_string());
    }

    if (x.type == Value::Type::Int && y.type == Value::Type::Int) {
        if (y.i == 0) throw std::runtime_error("integer division by zero");
        return Value(x.i / y.i);
    }

    double yy = asDouble(y);
    if (yy == 0.0) throw std::runtime_error("division by zero");
    return Value(asDouble(x) / yy);
}

inline Value mod(const Value& x, const Value& y) {
    if (!isNumeric(x) || !isNumeric(y)) {
        throw std::runtime_error("cannot modulo values: " + x.to_string() + " % " + y.to_string());
    }

    if (x.type == Value::Type::Int && y.type == Value::Type::Int) {
        if (y.i == 0) throw std::runtime_error("modulo by zero");
        return Value(x.i % y.i);
    }

    double yy = asDouble(y);
    if (yy == 0.0) throw std::runtime_error("modulo by zero");
    return Value(fmod(asDouble(x), yy));
}

inline Value neg(const Value& x) {
    if (x.type == Value::Type::Int) return Value(-x.i);
    if (x.type == Value::Type::Float) return Value(-x.f);
    throw std::runtime_error("cannot negate value: " + x.to_string());
}

inline Value eq(const Value& x, const Value& y) {
    if (isNumeric(x) && isNumeric(y)) {
        if (x.type == Value::Type::Int && y.type == Value::Type::Int) return Value(x.i == y.i);
        return Value(asDouble(x) == asDouble(y));
    }

    if (x.type != y.type) return Value(false);

    switch (x.type) {
        case Value::Type::Nil: return Value(true);
        case Value::Type::Bool: return Value(x.b == y.b);
        case Value::Type::Str: return Value(x.s == y.s);
        case Value::Type::Array: return Value(x.a == y.a);
        case Value::Type::Map: return Value(x.m == y.m);
        case Value::Type::Func: return Value(x.c == y.c);
    }

    return Value(false);
}

inline Value ne(const Value& x, const Value& y) {
    return Value(!truthy(eq(x, y)));
}

inline Value lt(const Value& x, const Value& y) {
    if (x.type == Value::Type::Str && y.type == Value::Type::Str) return Value(x.s < y.s);
    if (!isNumeric(x) || !isNumeric(y)) {
        throw std::runtime_error("cannot compare values with <: " + x.to_string() + " < " + y.to_string());
    }
    return Value(asDouble(x) < asDouble(y));
}

inline Value le(const Value& x, const Value& y) {
    if (x.type == Value::Type::Str && y.type == Value::Type::Str) return Value(x.s <= y.s);
    if (!isNumeric(x) || !isNumeric(y)) {
        throw std::runtime_error("cannot compare values with <=: " + x.to_string() + " <= " + y.to_string());
    }
    return Value(asDouble(x) <= asDouble(y));
}

inline Value gt(const Value& x, const Value& y) {
    if (x.type == Value::Type::Str && y.type == Value::Type::Str) return Value(x.s > y.s);
    if (!isNumeric(x) || !isNumeric(y)) {
        throw std::runtime_error("cannot compare values with >: " + x.to_string() + " > " + y.to_string());
    }
    return Value(asDouble(x) > asDouble(y));
}

inline Value ge(const Value& x, const Value& y) {
    if (x.type == Value::Type::Str && y.type == Value::Type::Str) return Value(x.s >= y.s);
    if (!isNumeric(x) || !isNumeric(y)) {
        throw std::runtime_error("cannot compare values with >=: " + x.to_string() + " >= " + y.to_string());
    }
    return Value(asDouble(x) >= asDouble(y));
}

inline Value index(const Value& obj, const Value& key) {
    if (obj.type == Value::Type::Array) {
        if (!obj.a) return Value();
        int64_t idx = asInt(key);
        int64_t sz = (int64_t)obj.a->size();

        if (idx < 0) idx += sz;
        if (idx < 0 || idx >= sz) {
            throw std::runtime_error("array index out of range: index=" + std::to_string(idx) +
                                     ", size=" + std::to_string(sz));
        }

        return (*obj.a)[idx];
    }

    if (obj.type == Value::Type::Map) {
        if (!obj.m) return Value();
        for (const auto& kv : *obj.m) {
            if (truthy(eq(kv.first, key))) return kv.second;
        }
        return Value();
    }

    if (obj.type == Value::Type::Str) {
        int64_t idx = asInt(key);
        int64_t sz = (int64_t)obj.s.size();

        if (idx < 0) idx += sz;
        if (idx < 0 || idx >= sz) {
            throw std::runtime_error("string index out of range: index=" + std::to_string(idx) +
                                     ", size=" + std::to_string(sz));
        }

        return Value((int64_t)(unsigned char)obj.s[idx]);
    }

    throw std::runtime_error("cannot index value: " + obj.to_string());
}

inline void set_index(Value& obj, const Value& key, const Value& value) {
    if (obj.type == Value::Type::Array) {
        if (!obj.a) obj.a = std::make_shared<Array>();
        int64_t idx = asInt(key);
        int64_t sz = (int64_t)obj.a->size();

        if (idx < 0) idx += sz;

        if (idx == sz) {
            obj.a->push_back(value);
            return;
        }

        if (idx < 0 || idx >= sz) {
            throw std::runtime_error("array assignment index out of range: index=" + std::to_string(idx) +
                                     ", size=" + std::to_string(sz));
        }

        (*obj.a)[idx] = value;
        return;
    }

    if (obj.type == Value::Type::Map) {
        if (!obj.m) obj.m = std::make_shared<Map>();

        for (auto& kv : *obj.m) {
            if (truthy(eq(kv.first, key))) {
                kv.second = value;
                return;
            }
        }

        obj.m->push_back({key, value});
        return;
    }

    throw std::runtime_error("cannot assign to index of non-array/map value");
}

inline Value field(const Value& obj, const std::string& name) {
    if (obj.type == Value::Type::Map) {
        if (!obj.m) return Value();

        Value key(name);
        for (const auto& kv : *obj.m) {
            if (truthy(eq(kv.first, key))) return kv.second;
        }
        return Value();
    }

    if (obj.type == Value::Type::Array && name == "len") {
        return Value((int64_t)(obj.a ? obj.a->size() : 0));
    }

    if (obj.type == Value::Type::Str && name == "len") {
        return Value((int64_t)obj.s.size());
    }

    throw std::runtime_error("cannot access field '" + name + "' of value: " + obj.to_string());
}

inline void set_field(Value& obj, const std::string& name, const Value& value) {
    if (obj.type == Value::Type::Map) {
        if (!obj.m) obj.m = std::make_shared<Map>();

        Value key(name);
        for (auto& kv : *obj.m) {
            if (truthy(eq(kv.first, key))) {
                kv.second = value;
                return;
            }
        }

        obj.m->push_back({key, value});
        return;
    }

    throw std::runtime_error("cannot assign field '" + name + "' of non-map value");
}

inline Value range(const Value& start, const Value& end, bool inclusive) {
    auto arr = std::make_shared<Array>();

    if (start.type == Value::Type::Int && end.type == Value::Type::Int) {
        int64_t a = start.i;
        int64_t b = end.i;
        int64_t step = (a <= b) ? 1 : -1;

        if (step > 0) {
            while (inclusive ? (a <= b) : (a < b)) {
                arr->push_back(Value(a));
                a += step;
            }
        } else {
            while (inclusive ? (a >= b) : (a > b)) {
                arr->push_back(Value(a));
                a += step;
            }
        }

        return Value(arr);
    }

    double a = asDouble(start);
    double b = asDouble(end);
    double step = (a <= b) ? 1.0 : -1.0;

    if (step > 0.0) {
        while (inclusive ? (a <= b) : (a < b)) {
            arr->push_back(Value(a));
            a += step;
        }
    } else {
        while (inclusive ? (a >= b) : (a > b)) {
            arr->push_back(Value(a));
            a += step;
        }
    }

    return Value(arr);
}

inline std::vector<Value> to_iterable(const Value& v) {
    if (v.type == Value::Type::Array) {
        return v.a ? *v.a : std::vector<Value>{};
    }

    if (v.type == Value::Type::Map) {
        std::vector<Value> keys;
        if (v.m) {
            for (const auto& kv : *v.m) keys.push_back(kv.first);
        }
        return keys;
    }

    if (v.type == Value::Type::Str) {
        std::vector<Value> chars;
        for (char c : v.s) chars.push_back(Value(std::string(1, c)));
        return chars;
    }

    if (v.type == Value::Type::Int) {
        std::vector<Value> nums;
        int64_t n = v.i;
        for (int64_t idx = 0; idx < n; ++idx) nums.push_back(Value(idx));
        return nums;
    }

    if (v.type == Value::Type::Nil) {
        return {};
    }

    throw std::runtime_error("value is not iterable: " + v.to_string());
}

inline Value len(const Value& v) {
    if (v.type == Value::Type::Str) return Value((int64_t)v.s.size());
    if (v.type == Value::Type::Array) return Value((int64_t)(v.a ? v.a->size() : 0));
    if (v.type == Value::Type::Map) return Value((int64_t)(v.m ? v.m->size() : 0));
    throw std::runtime_error("len() expects string/array/map, got: " + v.to_string());
}

inline Value push(Value arr, const Value& x) {
    if (arr.type != Value::Type::Array) {
        throw std::runtime_error("push() expects array, got: " + arr.to_string());
    }

    if (!arr.a) arr.a = std::make_shared<Array>();

    arr.a->push_back(x);
    return arr;
}

inline Value pop(Value arr) {
    if (arr.type != Value::Type::Array || !arr.a || arr.a->empty()) {
        throw std::runtime_error("pop() expects non-empty array");
    }

    Value x = arr.a->back();
    arr.a->pop_back();
    return x;
}

inline Value keys(const Value& mapValue) {
    if (mapValue.type != Value::Type::Map) {
        throw std::runtime_error("keys() expects map, got: " + mapValue.to_string());
    }

    auto arr = std::make_shared<Array>();
    if (mapValue.m) {
        for (const auto& kv : *mapValue.m) arr->push_back(kv.first);
    }
    return Value(arr);
}

inline Value values(const Value& mapValue) {
    if (mapValue.type != Value::Type::Map) {
        throw std::runtime_error("values() expects map, got: " + mapValue.to_string());
    }

    auto arr = std::make_shared<Array>();
    if (mapValue.m) {
        for (const auto& kv : *mapValue.m) arr->push_back(kv.second);
    }
    return Value(arr);
}

inline Value type(const Value& v) {
    switch (v.type) {
        case Value::Type::Nil: return Value("nil");
        case Value::Type::Bool: return Value("bool");
        case Value::Type::Int: return Value("int");
        case Value::Type::Float: return Value("float");
        case Value::Type::Str: return Value("str");
        case Value::Type::Array: return Value("array");
        case Value::Type::Map: return Value("map");
        case Value::Type::Func: return Value("func");
    }
    return Value("unknown");
}

inline Value shell(const Value& cmd) {
    if (cmd.type != Value::Type::Str) {
        throw std::runtime_error("shell() expects string command");
    }

    int status = std::system(cmd.s.c_str());

    if (status == -1) {
        throw std::runtime_error("shell() failed to execute command: " + cmd.s);
    }

    return Value((int64_t)status);
}

inline Value input() {
    std::string line;
    std::getline(std::cin, line);
    return Value(line);
}

inline Value sleep_ms(const Value& ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(asInt(ms)));
    return Value();
}

inline Value to_int(const Value& v) {
    if (v.type == Value::Type::Int) return v;
    if (v.type == Value::Type::Float) return Value((int64_t)v.f);
    if (v.type == Value::Type::Bool) return Value(v.b ? 1 : 0);
    if (v.type == Value::Type::Str) {
        try {
            return Value((int64_t)std::stoll(v.s));
        } catch (...) {
            throw std::runtime_error("int() cannot parse: '" + v.s + "'");
        }
    }
    throw std::runtime_error("int() expects number/bool/string, got: " + v.to_string());
}

inline Value to_float(const Value& v) {
    if (v.type == Value::Type::Int) return Value((double)v.i);
    if (v.type == Value::Type::Float) return v;
    if (v.type == Value::Type::Bool) return Value(v.b ? 1.0 : 0.0);
    if (v.type == Value::Type::Str) {
        try {
            return Value(std::stod(v.s));
        } catch (...) {
            throw std::runtime_error("float() cannot parse: '" + v.s + "'");
        }
    }
    throw std::runtime_error("float() expects number/bool/string, got: " + v.to_string());
}

inline Value to_str(const Value& v) {
    return Value(v.to_string());
}

inline Value print(const std::vector<Value>& args) {
    for (const auto& x : args) {
        std::print("{}", x.to_string());
    }
    std::println("");
    return Value();
}

inline Value map(const Value& iterable, const Value& f) {
    auto res = std::make_shared<Array>();

    for (auto x : to_iterable(iterable)) {
        res->push_back(call(f, std::vector<Value>{x}));
    }

    return Value(res);
}

inline Value filter(const Value& iterable, const Value& f) {
    auto res = std::make_shared<Array>();

    for (auto x : to_iterable(iterable)) {
        if (truthy(call(f, std::vector<Value>{x}))) {
            res->push_back(x);
        }
    }

    return Value(res);
}

inline Value reduce(const Value& iterable, const Value& init, const Value& f) {
    Value acc = init;

    for (auto x : to_iterable(iterable)) {
        acc = call(f, std::vector<Value>{acc, x});
    }

    return acc;
}

} // namespace wast
)WASTRUNTIME";
    }

    void genStruct(Node* st) {
        out << "Value wastfn_" << st->sval << "(std::vector<Value> args) {\n";
        out << "    Value obj = wast::make_map(std::vector<std::pair<Value, Value>>{});\n";

        for (size_t i = 0; i < st->children.size(); i++) {
            Node* fieldNode = st->children[i];
            out << "    wast::set_field(obj, \"" << fieldNode->sval << "\", args.size() > "
                << i << " ? args[" << i << "] : Value());\n";
        }

        out << "    return obj;\n";
        out << "}\n\n";
    }

    void genLibrary(Node* lib) {
        vector<Node*> params;
        string retType = "nil";
        string code;

        for (Node* child : lib->children) {
            if (child->kind == Node::Param) {
                params.push_back(child);
            } else if (child->kind == Node::TypeAnn) {
                retType = child->sval;
            } else if (child->kind == Node::RawCode) {
                code = child->sval;
            }
        }

        out << "Value wastfn_" << lib->sval << "(std::vector<Value> args) {\n";

        for (size_t i = 0; i < params.size(); i++) {
            Node* param = params[i];
            string pname = param->sval;
            string ptype = "value";

            if (!param->children.empty() && param->children[0]->kind == Node::TypeAnn) {
                ptype = param->children[0]->sval;
            }

            out << "    ";

            if (ptype == "i64") {
                out << "std::int64_t " << pname
                    << " = args.size() > " << i << " ? wast::asInt(args[" << i << "]) : 0;\n";
            } else if (ptype == "f64") {
                out << "double " << pname
                    << " = args.size() > " << i << " ? wast::asDouble(args[" << i << "]) : 0.0;\n";
            } else if (ptype == "bool") {
                out << "bool " << pname
                    << " = args.size() > " << i << " ? wast::truthy(args[" << i << "]) : false;\n";
            } else if (ptype == "str") {
                out << "std::string " << pname
                    << " = args.size() > " << i << " ? args[" << i << "].to_string() : std::string();\n";
            } else {
                out << "wast::Value " << pname
                    << " = args.size() > " << i << " ? args[" << i << "] : Value();\n";
            }
        }

        if (retType == "nil") {
            out << "    [&]() {\n";
            out << code << "\n";
            out << "    }();\n";
            out << "    return Value();\n";
        } else if (retType == "value") {
            out << "    Value wast_cpp_result = [&]() -> Value {\n";
            out << code << "\n";
            out << "    }();\n";
            out << "    return wast_cpp_result;\n";
        } else {
            string cppType;

            if (retType == "i64") cppType = "std::int64_t";
            else if (retType == "f64") cppType = "double";
            else if (retType == "bool") cppType = "bool";
            else if (retType == "str") cppType = "std::string";
            else error(lib, "unsupported library return type: " + retType +
                              " (supported: i64, f64, bool, str, value, nil)");

            out << "    " << cppType << " wast_cpp_result = [&]() -> " << cppType << " {\n";
            out << code << "\n";
            out << "    }();\n";

            out << "    return Value(";
            if (retType == "i64") out << "(std::int64_t)wast_cpp_result";
            else if (retType == "f64") out << "(double)wast_cpp_result";
            else if (retType == "bool") out << "(bool)wast_cpp_result";
            else if (retType == "str") out << "std::string(wast_cpp_result)";
            out << ");\n";
        }

        out << "}\n\n";
    }

    void genFunction(Node* fn) {
        vector<Node*> params;
        Node* body = nullptr;

        for (Node* child : fn->children) {
            if (child->kind == Node::Param) {
                params.push_back(child);
            } else if (child->kind == Node::Block) {
                body = child;
            }
        }

        out << "Value wastfn_" << fn->sval << "(std::vector<Value> args) {\n";

        for (size_t i = 0; i < params.size(); i++) {
            out << "    Value v_" << params[i]->sval
                << " = args.size() > " << i << " ? args[" << i << "] : Value();\n";
        }

        indent = 1;
        genBodyWithImplicitReturn(body);

        out << "    return Value();\n";
        out << "}\n\n";
    }

    void genBodyWithImplicitReturn(Node* body) {
        if (!body) return;
        if (body->children.empty()) return;

        for (size_t i = 0; i + 1 < body->children.size(); i++) {
            genStmt(body->children[i]);
        }

        Node* last = body->children.back();

        if (last->kind == Node::ExprStmt && !last->children.empty() &&
            last->children[0]->kind != Node::Assign) {
            emitIndent();
            out << "return ";
            genExpr(last->children[0]);
            out << ";\n";
        } else {
            genStmt(last);
        }
    }

    void genEntry() {
        out << "Value wast_entry(std::vector<Value> args) {\n";
        indent = 1;

        for (Node* e : enumNodes) {
            emitIndent();
            out << "v_" << e->sval << " = wast::make_map(std::vector<std::pair<Value, Value>>{";

            for (size_t i = 0; i < e->children.size(); i++) {
                if (i) out << ", ";
                Node* variant = e->children[i];
                out << "std::pair<Value, Value>{Value(\"" << escapeCpp(variant->sval)
                    << "\"), Value(\"" << escapeCpp(variant->sval) << "\")}";
            }

            out << "});\n";
        }

        for (Node* stmt : topLevel) {
            genStmt(stmt);
        }

        if (funcs.count("main")) {
            out << "    return wastfn_main(std::vector<Value>{});\n";
        } else {
            out << "    return Value();\n";
        }

        out << "}\n\n";
    }

    void genBlockChildren(Node* block) {
        if (!block) return;
        for (Node* stmt : block->children) {
            genStmt(stmt);
        }
    }

    void genStmt(Node* n) {
        if (!n) return;

        switch (n->kind) {
            case Node::VarDecl: {
                emitIndent();
                out << "Value v_" << n->sval;

                if (!n->children.empty()) {
                    out << " = ";
                    genExpr(n->children[0]);
                } else {
                    out << " = Value()";
                }

                out << ";\n";
                return;
            }

            case Node::Block: {
                emitIndent();
                out << "{\n";
                indent++;
                genBlockChildren(n);
                indent--;
                emitIndent();
                out << "}\n";
                return;
            }

            case Node::If: {
                if (n->children.size() < 2) error(n, "if statement is malformed");

                emitIndent();
                out << "if (wast::truthy(";
                genExpr(n->children[0]);
                out << ")) {\n";

                indent++;
                genBlockChildren(n->children[1]);
                indent--;

                emitIndent();
                out << "}";

                for (size_t i = 2; i < n->children.size(); i++) {
                    Node* child = n->children[i];

                    if (child->kind == Node::If) {
                        if (child->children.size() < 2) error(child, "elseif statement is malformed");

                        out << " else if (wast::truthy(";
                        genExpr(child->children[0]);
                        out << ")) {\n";

                        indent++;
                        genBlockChildren(child->children[1]);
                        indent--;

                        emitIndent();
                        out << "}";
                    } else if (child->kind == Node::Block) {
                        out << " else {\n";
                        indent++;
                        genBlockChildren(child);
                        indent--;
                        emitIndent();
                        out << "}";
                    }
                }

                out << "\n";
                return;
            }

            case Node::While: {
                if (n->children.size() != 2) error(n, "while statement is malformed");

                emitIndent();
                out << "while (wast::truthy(";
                genExpr(n->children[0]);
                out << ")) {\n";

                indent++;
                genBlockChildren(n->children[1]);
                indent--;

                emitIndent();
                out << "}\n";
                return;
            }

            case Node::ForNum: {
                if (n->children.size() != 4) error(n, "numeric for statement is malformed");

                int uid = newUid();
                string endVar = "end_" + to_string(uid);
                string stepVar = "step_" + to_string(uid);
                string posVar = "pos_" + to_string(uid);

                emitIndent();
                out << "{\n";
                indent++;

                emitIndent();
                out << "Value v_" << n->sval << " = ";
                genExpr(n->children[0]);
                out << ";\n";

                emitIndent();
                out << "Value " << endVar << " = ";
                genExpr(n->children[1]);
                out << ";\n";

                emitIndent();
                out << "Value " << stepVar << " = ";
                genExpr(n->children[2]);
                out << ";\n";

                emitIndent();
                out << "if (!wast::truthy(" << stepVar << ")) {\n";
                indent++;
                emitIndent();
                out << "throw std::runtime_error(\"for loop step cannot be zero\");\n";
                indent--;
                emitIndent();
                out << "}\n";

                emitIndent();
                out << "bool " << posVar << " = wast::truthy(wast::gt(" << stepVar
                    << ", Value((int64_t)0)));\n";

                emitIndent();
                out << "while (" << posVar << " ? wast::truthy(wast::le(v_" << n->sval
                    << ", " << endVar << ")) : wast::truthy(wast::ge(v_" << n->sval
                    << ", " << endVar << "))) {\n";

                indent++;
                genBlockChildren(n->children[3]);

                emitIndent();
                out << "v_" << n->sval << " = wast::add(v_" << n->sval << ", " << stepVar << ");\n";

                indent--;
                emitIndent();
                out << "}\n";

                indent--;
                emitIndent();
                out << "}\n";
                return;
            }

            case Node::ForIn: {
                if (n->children.size() != 2) error(n, "for-in statement is malformed");

                int uid = newUid();
                string seqVar = "seq_" + to_string(uid);

                emitIndent();
                out << "{\n";
                indent++;

                emitIndent();
                out << "std::vector<Value> " << seqVar << " = wast::to_iterable(";
                genExpr(n->children[0]);
                out << ");\n";

                emitIndent();
                out << "for (Value v_" << n->sval << " : " << seqVar << ") {\n";

                indent++;
                genBlockChildren(n->children[1]);
                indent--;

                emitIndent();
                out << "}\n";

                indent--;
                emitIndent();
                out << "}\n";
                return;
            }

            case Node::Return: {
                emitIndent();
                if (!n->children.empty()) {
                    out << "return ";
                    genExpr(n->children[0]);
                    out << ";\n";
                } else {
                    out << "return Value();\n";
                }
                return;
            }

            case Node::Break: {
                emitIndent();
                out << "break;\n";
                return;
            }

            case Node::Continue: {
                emitIndent();
                out << "continue;\n";
                return;
            }

            case Node::ExprStmt: {
                if (!n->children.empty()) {
                    if (n->children[0]->kind == Node::Assign) {
                        genAssign(n->children[0]);
                    } else {
                        emitIndent();
                        genExpr(n->children[0]);
                        out << ";\n";
                    }
                }
                return;
            }

            case Node::Match: {
                genMatch(n);
                return;
            }

            default:
                error(n, "unsupported statement in codegen");
        }
    }

    void genMatch(Node* n) {
        if (n->children.empty()) error(n, "match statement is malformed");

        int uid = newUid();
        string scrut = "scrut_" + to_string(uid);
        string matched = "matched_" + to_string(uid);

        emitIndent();
        out << "{\n";
        indent++;

        emitIndent();
        out << "Value " << scrut << " = ";
        genExpr(n->children[0]);
        out << ";\n";

        emitIndent();
        out << "bool " << matched << " = false;\n";

        for (size_t i = 1; i < n->children.size(); i++) {
            Node* arm = n->children[i];

            if (arm->kind != Node::MatchArm || arm->children.size() != 2) {
                error(arm, "match arm is malformed");
            }

            Node* pattern = arm->children[0];
            Node* body = arm->children[1];

            if (pattern->kind == Node::Wildcard) {
                emitIndent();
                out << "if (!" << matched << ") {\n";
                indent++;
                genMatchBody(body);
                indent--;
                emitIndent();
                out << "}\n";
            } else {
                emitIndent();
                out << "if (!" << matched << " && (";
                genPatternCond(pattern, scrut);
                out << ")) {\n";
                indent++;

                emitIndent();
                out << matched << " = true;\n";

                genMatchBody(body);

                indent--;
                emitIndent();
                out << "}\n";
            }
        }

        indent--;
        emitIndent();
        out << "}\n";
    }

    void genMatchBody(Node* body) {
        if (body->kind == Node::Block) {
            genBlockChildren(body);
        } else {
            emitIndent();
            genExpr(body);
            out << ";\n";
        }
    }

    void genPatternCond(Node* pattern, const string& scrut) {
        if (pattern->kind == Node::PatternAlt) {
            out << "(";
            for (size_t i = 0; i < pattern->children.size(); i++) {
                if (i) out << " || ";
                genPatternCond(pattern->children[i], scrut);
            }
            out << ")";
            return;
        }

        if (pattern->kind == Node::Wildcard) {
            out << "true";
            return;
        }

        out << "wast::truthy(wast::eq(" << scrut << ", ";
        genExpr(pattern);
        out << "))";
    }

    string helperForCompound(const string& op) {
        if (op == "+") return "wast::add";
        if (op == "-") return "wast::sub";
        if (op == "*") return "wast::mul";
        if (op == "/") return "wast::divide";
        if (op == "%") return "wast::mod";
        return "";
    }

    void genAssign(Node* n) {
        if (n->children.size() != 2) error(n, "assignment is malformed");

        Node* left = n->children[0];
        Node* right = n->children[1];
        string op = n->sval;

        if (op == "=") {
            if (left->kind == Node::Ident) {
                emitIndent();
                out << "v_" << left->sval << " = ";
                genExpr(right);
                out << ";\n";
                return;
            }

            if (left->kind == Node::Index) {
                if (left->children.size() != 2) error(left, "index assignment is malformed");

                emitIndent();
                out << "{ Value tmp_obj = ";
                genExpr(left->children[0]);
                out << "; Value tmp_idx = ";
                genExpr(left->children[1]);
                out << "; Value tmp_val = ";
                genExpr(right);
                out << "; wast::set_index(tmp_obj, tmp_idx, tmp_val); }\n";
                return;
            }

            if (left->kind == Node::Field) {
                if (left->children.size() != 1) error(left, "field assignment is malformed");

                emitIndent();
                out << "{ Value tmp_obj = ";
                genExpr(left->children[0]);
                out << "; Value tmp_val = ";
                genExpr(right);
                out << "; wast::set_field(tmp_obj, \"" << left->sval << "\", tmp_val); }\n";
                return;
            }

            error(n, "invalid assignment target");
        }

        if (op.size() < 1) error(n, "invalid compound assignment");
        string baseOp = op.substr(0, 1);
        string helper = helperForCompound(baseOp);
        if (helper.empty()) error(n, "unsupported compound assignment: " + op);

        if (left->kind == Node::Ident) {
            emitIndent();
            out << "v_" << left->sval << " = " << helper << "(v_" << left->sval << ", ";
            genExpr(right);
            out << ");\n";
            return;
        }

        if (left->kind == Node::Index) {
            if (left->children.size() != 2) error(left, "index assignment is malformed");

            emitIndent();
            out << "{ Value tmp_obj = ";
            genExpr(left->children[0]);
            out << "; Value tmp_idx = ";
            genExpr(left->children[1]);
            out << "; Value tmp_old = wast::index(tmp_obj, tmp_idx); Value tmp_new = "
                << helper << "(tmp_old, ";
            genExpr(right);
            out << "); wast::set_index(tmp_obj, tmp_idx, tmp_new); }\n";
            return;
        }

        if (left->kind == Node::Field) {
            if (left->children.size() != 1) error(left, "field assignment is malformed");

            emitIndent();
            out << "{ Value tmp_obj = ";
            genExpr(left->children[0]);
            out << "; Value tmp_old = wast::field(tmp_obj, \"" << left->sval << "\"); Value tmp_new = "
                << helper << "(tmp_old, ";
            genExpr(right);
            out << "); wast::set_field(tmp_obj, \"" << left->sval << "\", tmp_new); }\n";
            return;
        }

        error(n, "invalid compound assignment target");
    }

    void genExpr(Node* n) {
        if (!n) {
            out << "Value()";
            return;
        }

        switch (n->kind) {
            case Node::IntLit:
                out << "Value((int64_t)" << n->ival << ")";
                return;

            case Node::FloatLit: {
                ostringstream os;
                os << n->fval;
                string s = os.str();
                if (s.find('.') == string::npos && s.find('e') == string::npos &&
                    s.find("inf") == string::npos && s.find("nan") == string::npos) {
                    s += ".0";
                }
                out << "Value(" << s << ")";
                return;
            }

            case Node::StrLit:
                emitStringExpr(n->sval, n);
                return;

            case Node::BoolLit:
                out << "Value(" << (n->bval ? "true" : "false") << ")";
                return;

            case Node::NilLit:
                out << "Value()";
                return;

            case Node::Ident:
                out << "v_" << n->sval;
                return;

            case Node::BinOp: {
                if (n->children.size() != 2) error(n, "binary operation is malformed");

                const string& op = n->sval;

                if (op == "&&" || op == "||") {
                    out << "Value(wast::truthy(";
                    genExpr(n->children[0]);
                    out << ") " << op << " wast::truthy(";
                    genExpr(n->children[1]);
                    out << "))";
                    return;
                }

                if (op == "..") {
                    out << "wast::range(";
                    genExpr(n->children[0]);
                    out << ", ";
                    genExpr(n->children[1]);
                    out << ", false)";
                    return;
                }

                if (op == "..=") {
                    out << "wast::range(";
                    genExpr(n->children[0]);
                    out << ", ";
                    genExpr(n->children[1]);
                    out << ", true)";
                    return;
                }

                string helper;
                if (op == "+") helper = "wast::add";
                else if (op == "-") helper = "wast::sub";
                else if (op == "*") helper = "wast::mul";
                else if (op == "/") helper = "wast::divide";
                else if (op == "%") helper = "wast::mod";
                else if (op == "==") helper = "wast::eq";
                else if (op == "!=") helper = "wast::ne";
                else if (op == "<") helper = "wast::lt";
                else if (op == "<=") helper = "wast::le";
                else if (op == ">") helper = "wast::gt";
                else if (op == ">=") helper = "wast::ge";
                else error(n, "unsupported binary operator: " + op);

                out << helper << "(";
                genExpr(n->children[0]);
                out << ", ";
                genExpr(n->children[1]);
                out << ")";
                return;
            }

            case Node::UnaryOp: {
                if (n->children.size() != 1) error(n, "unary operation is malformed");

                if (n->sval == "-") {
                    out << "wast::neg(";
                    genExpr(n->children[0]);
                    out << ")";
                    return;
                }

                if (n->sval == "!") {
                    out << "Value(!wast::truthy(";
                    genExpr(n->children[0]);
                    out << "))";
                    return;
                }

                error(n, "unsupported unary operator: " + n->sval);
            }

            case Node::Call:
                genCall(n);
                return;

            case Node::Index: {
                if (n->children.size() != 2) error(n, "index expression is malformed");
                out << "wast::index(";
                genExpr(n->children[0]);
                out << ", ";
                genExpr(n->children[1]);
                out << ")";
                return;
            }

            case Node::Field: {
                if (n->children.size() != 1) error(n, "field expression is malformed");
                out << "wast::field(";
                genExpr(n->children[0]);
                out << ", \"" << escapeCpp(n->sval) << "\")";
                return;
            }

            case Node::ArrayLit: {
                out << "wast::make_array(std::vector<Value>{";
                for (size_t i = 0; i < n->children.size(); i++) {
                    if (i) out << ", ";
                    genExpr(n->children[i]);
                }
                out << "})";
                return;
            }

            case Node::MapLit: {
                out << "wast::make_map(std::vector<std::pair<Value, Value>>{";
                for (size_t i = 0; i < n->children.size(); i++) {
                    Node* pair = n->children[i];
                    if (pair->kind != Node::Pair || pair->children.size() != 2) {
                        error(pair, "map literal entry is malformed");
                    }

                    if (i) out << ", ";
                    out << "std::pair<Value, Value>{";
                    genExpr(pair->children[0]);
                    out << ", ";
                    genExpr(pair->children[1]);
                    out << "}";
                }
                out << "})";
                return;
            }

            case Node::Lambda:
                genLambda(n);
                return;

            case Node::Pipe:
                genPipe(n);
                return;

            case Node::Assign:
                error(n, "assignment cannot be used as an expression inside another expression");
                return;

            default:
                error(n, "unsupported expression in codegen");
        }
    }

    void genLambda(Node* lam) {
        vector<Node*> params;
        Node* body = nullptr;

        for (Node* child : lam->children) {
            if (child->kind == Node::Param) params.push_back(child);
            else if (child->kind == Node::Block) body = child;
        }

        int oldIndent = indent;

        out << "wast::make_closure([=](std::vector<Value> args) -> Value {\n";

        indent = oldIndent + 1;

        for (size_t i = 0; i < params.size(); i++) {
            emitIndent();
            out << "Value v_" << params[i]->sval
                << " = args.size() > " << i << " ? args[" << i << "] : Value();\n";
        }

        genBodyWithImplicitReturn(body);

        emitIndent();
        out << "return Value();\n";

        indent = oldIndent;
        emitIndent();
        out << "})";
    }

    void genPipe(Node* n) {
        if (n->children.size() != 2) error(n, "pipe expression is malformed");

        Node* left = n->children[0];
        Node* right = n->children[1];

        if (right->kind == Node::Lambda) {
            out << "wast::call(";
            genExpr(right);
            out << ", std::vector<Value>{";
            genExpr(left);
            out << "})";
            return;
        }

        if (right->kind == Node::Ident) {
            string name = right->sval;

            if (name == "print") {
                out << "wast::print(std::vector<Value>{";
                genExpr(left);
                out << "})";
                return;
            }

            if (name == "len" || name == "str" || name == "int" || name == "float" ||
                name == "keys" || name == "values" || name == "type" || name == "pop") {
                string helper = "wast::";
                if (name == "str") helper += "to_str";
                else if (name == "int") helper += "to_int";
                else if (name == "float") helper += "to_float";
                else helper += name;

                out << helper << "(";
                genExpr(left);
                out << ")";
                return;
            }

            if (funcs.count(name)) {
                out << "wastfn_" << name << "(std::vector<Value>{";
                genExpr(left);
                out << "})";
                return;
            }

            out << "wast::call(v_" << name << ", std::vector<Value>{";
            genExpr(left);
            out << "})";
            return;
        }

        error(n, "pipe target must be a function name or lambda in this build");
    }

    void genCall(Node* n) {
        if (n->children.empty()) error(n, "call expression is malformed");

        Node* callee = n->children[0];

        if (callee->kind != Node::Ident) {
            out << "wast::call(";
            genExpr(callee);
            out << ", std::vector<Value>{";

            for (size_t i = 1; i < n->children.size(); i++) {
                if (i > 1) out << ", ";
                genExpr(n->children[i]);
            }

            out << "})";
            return;
        }

        string name = callee->sval;

        auto argList = [&]() {
            out << "std::vector<Value>{";
            for (size_t i = 1; i < n->children.size(); i++) {
                if (i > 1) out << ", ";
                genExpr(n->children[i]);
            }
            out << "}";
        };

        size_t argc = n->children.size() - 1;

        auto expectArgs = [&](size_t count) {
            if (argc != count) {
                error(n, name + "() expects " + to_string(count) + " argument(s), got " + to_string(argc));
            }
        };

        if (name == "print") {
            out << "wast::print(";
            argList();
            out << ")";
            return;
        }

        if (name == "len") {
            expectArgs(1);
            out << "wast::len(";
            genExpr(n->children[1]);
            out << ")";
            return;
        }

        if (name == "str") {
            expectArgs(1);
            out << "wast::to_str(";
            genExpr(n->children[1]);
            out << ")";
            return;
        }

        if (name == "int") {
            expectArgs(1);
            out << "wast::to_int(";
            genExpr(n->children[1]);
            out << ")";
            return;
        }

        if (name == "float") {
            expectArgs(1);
            out << "wast::to_float(";
            genExpr(n->children[1]);
            out << ")";
            return;
        }

        if (name == "push") {
            expectArgs(2);
            out << "wast::push(";
            genExpr(n->children[1]);
            out << ", ";
            genExpr(n->children[2]);
            out << ")";
            return;
        }

        if (name == "pop") {
            expectArgs(1);
            out << "wast::pop(";
            genExpr(n->children[1]);
            out << ")";
            return;
        }

        if (name == "keys") {
            expectArgs(1);
            out << "wast::keys(";
            genExpr(n->children[1]);
            out << ")";
            return;
        }

        if (name == "values") {
            expectArgs(1);
            out << "wast::values(";
            genExpr(n->children[1]);
            out << ")";
            return;
        }

        if (name == "type") {
            expectArgs(1);
            out << "wast::type(";
            genExpr(n->children[1]);
            out << ")";
            return;
        }

        if (name == "range") {
            if (argc == 2) {
                out << "wast::range(";
                genExpr(n->children[1]);
                out << ", ";
                genExpr(n->children[2]);
                out << ", false)";
                return;
            }

            if (argc == 3) {
                out << "wast::range(";
                genExpr(n->children[1]);
                out << ", ";
                genExpr(n->children[2]);
                out << ", wast::truthy(";
                genExpr(n->children[3]);
                out << "))";
                return;
            }

            error(n, "range() expects 2 or 3 arguments");
        }

        if (name == "input") {
            expectArgs(0);
            out << "wast::input()";
            return;
        }

        if (name == "shell") {
            expectArgs(1);
            out << "wast::shell(";
            genExpr(n->children[1]);
            out << ")";
            return;
        }

        if (name == "sleep") {
            expectArgs(1);
            out << "wast::sleep_ms(";
            genExpr(n->children[1]);
            out << ")";
            return;
        }

        if (name == "map") {
            expectArgs(2);
            out << "wast::map(";
            genExpr(n->children[1]);
            out << ", ";
            genExpr(n->children[2]);
            out << ")";
            return;
        }

        if (name == "filter") {
            expectArgs(2);
            out << "wast::filter(";
            genExpr(n->children[1]);
            out << ", ";
            genExpr(n->children[2]);
            out << ")";
            return;
        }

        if (name == "reduce") {
            expectArgs(3);
            out << "wast::reduce(";
            genExpr(n->children[1]);
            out << ", ";
            genExpr(n->children[2]);
            out << ", ";
            genExpr(n->children[3]);
            out << ")";
            return;
        }

        if (funcs.count(name)) {
            out << "wastfn_" << name << "(";
            argList();
            out << ")";
            return;
        }

        out << "wast::call(v_" << name << ", ";
        argList();
        out << ")";
    }

    bool isSimpleInterpolIdent(const string& s) {
        if (s.empty()) return false;
        if (!(std::isalpha((unsigned char)s[0]) || s[0] == '_')) return false;

        for (char c : s) {
            if (!(std::isalnum((unsigned char)c) || c == '_')) return false;
        }

        return true;
    }

    bool isSimpleInterpolNumber(const string& s, bool& isFloat) {
        if (s.empty()) return false;

        size_t i = 0;
        if (s[i] == '-' || s[i] == '+') i++;

        bool digit = false;
        bool dot = false;

        for (; i < s.size(); i++) {
            if (std::isdigit((unsigned char)s[i])) {
                digit = true;
            } else if (s[i] == '.') {
                if (dot) return false;
                dot = true;
            } else {
                return false;
            }
        }

        isFloat = dot;
        return digit;
    }

    void emitStringExpr(const string& raw, Node* n) {
        bool hasInterpolation = raw.find('{') != string::npos;

        if (!hasInterpolation) {
            out << "Value(\"" << escapeCpp(raw) << "\")";
            return;
        }

        vector<string> parts;
        string text;

        for (size_t i = 0; i < raw.size(); ) {
            if (raw[i] == '{') {
                size_t close = raw.find('}', i + 1);
                if (close == string::npos) {
                    error(n, "unterminated string interpolation: expected '}'");
                }

                if (!text.empty()) {
                    parts.push_back("Value(\"" + escapeCpp(text) + "\")");
                    text.clear();
                }

                string inside = raw.substr(i + 1, close - i - 1);

                while (!inside.empty() && std::isspace((unsigned char)inside.front())) inside.erase(inside.begin());
                while (!inside.empty() && std::isspace((unsigned char)inside.back())) inside.pop_back();

                if (inside.empty()) {
                    error(n, "empty string interpolation '{}' is not allowed");
                }

                bool isFloat = false;
                if (isSimpleInterpolIdent(inside)) {
                    parts.push_back("Value(v_" + inside + ")");
                } else if (isSimpleInterpolNumber(inside, isFloat)) {
                    if (isFloat) {
                        parts.push_back("Value(" + inside + ")");
                    } else {
                        parts.push_back("Value((int64_t)" + inside + ")");
                    }
                } else {
                    error(n, "string interpolation currently supports only simple variable names or numbers: {" + inside + "}");
                }

                i = close + 1;
            } else {
                text += raw[i++];
            }
        }

        if (!text.empty()) {
            parts.push_back("Value(\"" + escapeCpp(text) + "\")");
        }

        if (parts.empty()) {
            out << "Value(\"\")";
            return;
        }

        out << "Value(wast::concat(std::vector<Value>{";
        for (size_t i = 0; i < parts.size(); i++) {
            if (i) out << ", ";
            out << parts[i];
        }
        out << "}))";
    }
};

// ============================================================
// Main Driver
// ============================================================

static string readFile(const string& path) {
    ifstream f(path, ios::binary);
    if (!f.is_open()) {
        fail("[error] cannot open input file: '" + path + "'\n  hint: check file path and permissions");
    }

    ostringstream os;
    os << f.rdbuf();
    return os.str();
}

static void writeFile(const string& path, const string& content) {
    ofstream f(path, ios::binary);
    if (!f.is_open()) {
        fail("[error] cannot open output file: '" + path + "'\n  hint: check file path and permissions");
    }

    f << content;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "wastc v6 - single-file wast -> C++23 transpiler\n";
        cerr << "usage: wastc <input.wast> [output.cpp]\n";
        return 1;
    }

    string input = argv[1];
    string output = argc >= 3 ? argv[2] : input + ".cpp";

    try {
        string source = readFile(input);

        Lexer lexer(source);
        vector<Token> tokens = lexer.tokenize();

        Parser parser(tokens);
        Node* ast = parser.parseProgram();

        Optimizer optimizer;
        optimizer.optimize(ast);

        CodeGen codegen;
        string cpp = codegen.compile(ast);

        writeFile(output, cpp);

        delete ast;

        std::println("wastc: {} -> {}", input, output);
        return 0;
    } catch (const CompileError& e) {
        cerr << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        cerr << "[fatal] " << e.what() << "\n";
        return 1;
    }
}
