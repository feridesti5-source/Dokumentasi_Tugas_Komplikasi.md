/*
 * ============================================================
 *  Simulasi Tahapan Kompilasi untuk Konstruksi IF-ELSE
 * ============================================================
 *
 *  Tahapan yang direpresentasikan:
 *    1. Analisis Leksikal   -> memecah source code menjadi token
 *    2. Analisis Sintaksis  -> membentuk Abstract Syntax Tree (AST)
 *    3. Analisis Semantik   -> validasi variabel & pembagian dengan nol
 *    4. Generasi Kode Antara-> Three-Address Code (TAC)
 *
 *  Grammar (BNF) yang diimplementasikan:
 *
 *  <if_stmt>    ::= "if" "(" <condition> ")" "{" <stmt_list> "}"
 *                   "else" "{" <stmt_list> "}"
 *  <condition>  ::= <expr> <relop> <expr>
 *  <relop>      ::= "<" | ">" | "<=" | ">=" | "==" | "!="
 *  <stmt_list>  ::= <assign_stmt>*
 *  <assign_stmt>::= <identifier> "=" <expr> ";"
 *  <expr>       ::= <term> (("+" | "-") <term>)*
 *  <term>       ::= <factor> (("*" | "/") <factor>)*
 *  <factor>     ::= <identifier> | <number> | "(" <expr> ")"
 *
 *  Kompilasi : g++ -std=c++17 ifelse_compiler.cpp -o ifelse_compiler
 *  Jalankan  : ./ifelse_compiler
 * ============================================================
 */

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <map>
#include <stdexcept>
#include <cctype>
#include <sstream>

// =========================================================
// 1. ANALISIS LEKSIKAL (LEXER)
// =========================================================

enum class TokenType {
    IF, ELSE, IDENT, NUMBER, RELOP, ASSIGN,
    PLUS, MINUS, STAR, SLASH,
    LPAREN, RPAREN, LBRACE, RBRACE, SEMICOLON,
    END
};

struct Token {
    TokenType type;
    std::string text;
};

static std::string tokenTypeName(TokenType t) {
    switch (t) {
        case TokenType::IF: return "IF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::IDENT: return "IDENT";
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::RELOP: return "RELOP";
        case TokenType::ASSIGN: return "ASSIGN";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::END: return "END";
    }
    return "?";
}

class Lexer {
public:
    explicit Lexer(const std::string& src) : src_(src), pos_(0) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (pos_ < src_.size()) {
            char c = src_[pos_];

            if (std::isspace((unsigned char)c)) { pos_++; continue; }

            if (std::isalpha((unsigned char)c)) {
                std::string word;
                while (pos_ < src_.size() && std::isalnum((unsigned char)src_[pos_])) {
                    word += src_[pos_++];
                }
                if (word == "if") tokens.push_back({TokenType::IF, word});
                else if (word == "else") tokens.push_back({TokenType::ELSE, word});
                else tokens.push_back({TokenType::IDENT, word});
                continue;
            }

            if (std::isdigit((unsigned char)c)) {
                std::string num;
                while (pos_ < src_.size() && std::isdigit((unsigned char)src_[pos_])) {
                    num += src_[pos_++];
                }
                tokens.push_back({TokenType::NUMBER, num});
                continue;
            }

            switch (c) {
                case '(': tokens.push_back({TokenType::LPAREN, "("}); pos_++; continue;
                case ')': tokens.push_back({TokenType::RPAREN, ")"}); pos_++; continue;
                case '{': tokens.push_back({TokenType::LBRACE, "{"}); pos_++; continue;
                case '}': tokens.push_back({TokenType::RBRACE, "}"}); pos_++; continue;
                case ';': tokens.push_back({TokenType::SEMICOLON, ";"}); pos_++; continue;
                case '+': tokens.push_back({TokenType::PLUS, "+"}); pos_++; continue;
                case '-': tokens.push_back({TokenType::MINUS, "-"}); pos_++; continue;
                case '*': tokens.push_back({TokenType::STAR, "*"}); pos_++; continue;
                case '/': tokens.push_back({TokenType::SLASH, "/"}); pos_++; continue;
            }

            // Operator relasional & assignment (bisa 1 atau 2 karakter)
            if (c == '=' || c == '<' || c == '>' || c == '!') {
                std::string op(1, c);
                pos_++;
                if (pos_ < src_.size() && src_[pos_] == '=') {
                    op += '=';
                    pos_++;
                }
                if (op == "=") tokens.push_back({TokenType::ASSIGN, op});
                else tokens.push_back({TokenType::RELOP, op});
                continue;
            }

            throw std::runtime_error(std::string("Karakter tidak dikenal pada lexer: ") + c);
        }
        tokens.push_back({TokenType::END, ""});
        return tokens;
    }

private:
    std::string src_;
    size_t pos_;
};

// =========================================================
// 2. ANALISIS SINTAKSIS (PARSER) -> Abstract Syntax Tree
// =========================================================

// Node ekspresi: leaf (identifier/number) atau binary operation
struct ExprNode {
    enum class Kind { IDENT, NUMBER, BINOP } kind;
    std::string value;                 // untuk IDENT / NUMBER / operator (+ - * /)
    std::shared_ptr<ExprNode> left;
    std::shared_ptr<ExprNode> right;
};

using ExprPtr = std::shared_ptr<ExprNode>;

struct ConditionNode {
    ExprPtr left;
    std::string op;   // relop
    ExprPtr right;
};

struct AssignStmt {
    std::string var;
    ExprPtr expr;
};

struct IfElseNode {
    ConditionNode condition;
    std::vector<AssignStmt> thenBlock;
    std::vector<AssignStmt> elseBlock;
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)), pos_(0) {}

    IfElseNode parseIfElse() {
        expect(TokenType::IF, "Diharapkan kata kunci 'if'");
        expect(TokenType::LPAREN, "Diharapkan '(' setelah 'if'");
        ConditionNode cond = parseCondition();
        expect(TokenType::RPAREN, "Diharapkan ')' setelah kondisi");

        expect(TokenType::LBRACE, "Diharapkan '{' untuk blok if");
        std::vector<AssignStmt> thenBlock = parseStmtList();
        expect(TokenType::RBRACE, "Diharapkan '}' penutup blok if");

        expect(TokenType::ELSE, "Diharapkan kata kunci 'else'");
        expect(TokenType::LBRACE, "Diharapkan '{' untuk blok else");
        std::vector<AssignStmt> elseBlock = parseStmtList();
        expect(TokenType::RBRACE, "Diharapkan '}' penutup blok else");

        return IfElseNode{cond, thenBlock, elseBlock};
    }

private:
    std::vector<Token> tokens_;
    size_t pos_;

    const Token& current() { return tokens_[pos_]; }

    const Token& advance() { return tokens_[pos_++]; }

    void expect(TokenType t, const std::string& msg) {
        if (current().type != t) {
            throw std::runtime_error("Syntax Error: " + msg +
                " (ditemukan token '" + current().text + "' [" + tokenTypeName(current().type) + "])");
        }
        advance();
    }

    ConditionNode parseCondition() {
        ExprPtr left = parseExpr();
        if (current().type != TokenType::RELOP) {
            throw std::runtime_error("Syntax Error: diharapkan operator relasional pada kondisi");
        }
        std::string op = advance().text;
        ExprPtr right = parseExpr();
        return ConditionNode{left, op, right};
    }

    std::vector<AssignStmt> parseStmtList() {
        std::vector<AssignStmt> stmts;
        while (current().type == TokenType::IDENT) {
            std::string var = advance().text;
            expect(TokenType::ASSIGN, "Diharapkan '=' pada statement assignment");
            ExprPtr expr = parseExpr();
            expect(TokenType::SEMICOLON, "Diharapkan ';' penutup statement");
            stmts.push_back(AssignStmt{var, expr});
        }
        return stmts;
    }

    // expr := term (( '+' | '-' ) term)*
    ExprPtr parseExpr() {
        ExprPtr node = parseTerm();
        while (current().type == TokenType::PLUS || current().type == TokenType::MINUS) {
            std::string op = advance().text;
            ExprPtr right = parseTerm();
            auto bin = std::make_shared<ExprNode>();
            bin->kind = ExprNode::Kind::BINOP;
            bin->value = op;
            bin->left = node;
            bin->right = right;
            node = bin;
        }
        return node;
    }

    // term := factor (( '*' | '/' ) factor)*
    ExprPtr parseTerm() {
        ExprPtr node = parseFactor();
        while (current().type == TokenType::STAR || current().type == TokenType::SLASH) {
            std::string op = advance().text;
            ExprPtr right = parseFactor();
            auto bin = std::make_shared<ExprNode>();
            bin->kind = ExprNode::Kind::BINOP;
            bin->value = op;
            bin->left = node;
            bin->right = right;
            node = bin;
        }
        return node;
    }

    // factor := IDENT | NUMBER | '(' expr ')'
    ExprPtr parseFactor() {
        if (current().type == TokenType::IDENT) {
            auto n = std::make_shared<ExprNode>();
            n->kind = ExprNode::Kind::IDENT;
            n->value = advance().text;
            return n;
        }
        if (current().type == TokenType::NUMBER) {
            auto n = std::make_shared<ExprNode>();
            n->kind = ExprNode::Kind::NUMBER;
            n->value = advance().text;
            return n;
        }
        if (current().type == TokenType::LPAREN) {
            advance();
            ExprPtr n = parseExpr();
            expect(TokenType::RPAREN, "Diharapkan ')' penutup ekspresi");
            return n;
        }
        throw std::runtime_error("Syntax Error: ekspresi tidak valid pada token '" + current().text + "'");
    }
};

// Fungsi bantu untuk mencetak AST secara terstruktur (indented)
void printExpr(const ExprPtr& e, int depth = 0) {
    std::string indent(depth * 2, ' ');
    if (!e) return;
    if (e->kind == ExprNode::Kind::IDENT) {
        std::cout << indent << "Ident(" << e->value << ")\n";
    } else if (e->kind == ExprNode::Kind::NUMBER) {
        std::cout << indent << "Number(" << e->value << ")\n";
    } else {
        std::cout << indent << "BinOp(" << e->value << ")\n";
        printExpr(e->left, depth + 1);
        printExpr(e->right, depth + 1);
    }
}

void printAST(const IfElseNode& node) {
    std::cout << "IfElseNode\n";
    std::cout << "|- Condition (" << node.condition.op << ")\n";
    std::cout << "|  |- Left:\n";
    printExpr(node.condition.left, 3);
    std::cout << "|  |- Right:\n";
    printExpr(node.condition.right, 3);
    std::cout << "|- Then Block:\n";
    for (auto& s : node.thenBlock) {
        std::cout << "|  |- Assign(" << s.var << ")\n";
        printExpr(s.expr, 3);
    }
    std::cout << "|- Else Block:\n";
    for (auto& s : node.elseBlock) {
        std::cout << "|  |- Assign(" << s.var << ")\n";
        printExpr(s.expr, 3);
    }
}

// =========================================================
// 3. ANALISIS SEMANTIK
// =========================================================

class SemanticAnalyzer {
public:
    // Mengembalikan daftar variabel yang sudah "dideklarasikan" (di-assign)
    // sebelum blok if-else, untuk mensimulasikan symbol table sederhana.
    explicit SemanticAnalyzer(std::vector<std::string> preDeclaredVars) {
        for (auto& v : preDeclaredVars) symbolTable_[v] = true;
    }

    void analyze(const IfElseNode& node) {
        // Cek variabel pada kondisi
        checkExprDeclared(node.condition.left);
        checkExprDeclared(node.condition.right);
        checkNoDivByZero(node.condition.left);
        checkNoDivByZero(node.condition.right);

        // Statement pada blok then: variabel yang di-assign otomatis "dideklarasikan"
        for (auto& s : node.thenBlock) {
            checkExprDeclared(s.expr);
            checkNoDivByZero(s.expr);
            symbolTable_[s.var] = true; // deklarasi implisit
        }
        for (auto& s : node.elseBlock) {
            checkExprDeclared(s.expr);
            checkNoDivByZero(s.expr);
            symbolTable_[s.var] = true;
        }

        std::cout << "[Semantik OK] Semua variabel valid, tidak ada pembagian dengan nol.\n";
    }

private:
    std::map<std::string, bool> symbolTable_;

    void checkExprDeclared(const ExprPtr& e) {
        if (!e) return;
        if (e->kind == ExprNode::Kind::IDENT) {
            if (symbolTable_.find(e->value) == symbolTable_.end()) {
                throw std::runtime_error("Semantic Error: variabel '" + e->value +
                    "' digunakan sebelum dideklarasikan/di-assign.");
            }
        } else if (e->kind == ExprNode::Kind::BINOP) {
            checkExprDeclared(e->left);
            checkExprDeclared(e->right);
        }
    }

    void checkNoDivByZero(const ExprPtr& e) {
        if (!e) return;
        if (e->kind == ExprNode::Kind::BINOP) {
            if (e->value == "/" && e->right->kind == ExprNode::Kind::NUMBER && e->right->value == "0") {
                throw std::runtime_error("Semantic Error: pembagian dengan nol terdeteksi secara statis.");
            }
            checkNoDivByZero(e->left);
            checkNoDivByZero(e->right);
        }
    }
};

// =========================================================
// 4. GENERASI KODE ANTARA (THREE-ADDRESS CODE)
// =========================================================

class TACGenerator {
public:
    std::vector<std::string> generate(const IfElseNode& node) {
        code_.clear();
        tempCounter_ = 1;
        labelCounter_ = 1;

        std::string leftVal = genExpr(node.condition.left);
        std::string rightVal = genExpr(node.condition.right);

        std::string labelElse = newLabel();
        std::string labelEnd = newLabel();

        emit("ifFalse " + leftVal + " " + node.condition.op + " " + rightVal + " goto " + labelElse);

        for (auto& s : node.thenBlock) {
            std::string val = genExpr(s.expr);
            emit(s.var + " = " + val);
        }
        emit("goto " + labelEnd);

        emit(labelElse + ":");
        for (auto& s : node.elseBlock) {
            std::string val = genExpr(s.expr);
            emit(s.var + " = " + val);
        }

        emit(labelEnd + ":");

        return code_;
    }

private:
    std::vector<std::string> code_;
    int tempCounter_;
    int labelCounter_;

    void emit(const std::string& line) { code_.push_back(line); }

    std::string newTemp() { return "t" + std::to_string(tempCounter_++); }
    std::string newLabel() { return "L" + std::to_string(labelCounter_++); }

    // Menghasilkan kode untuk sub-ekspresi, mengembalikan nama variabel/temp/angka hasil
    std::string genExpr(const ExprPtr& e) {
        if (e->kind == ExprNode::Kind::IDENT || e->kind == ExprNode::Kind::NUMBER) {
            return e->value;
        }
        // BINOP -> perlu variabel sementara
        std::string leftVal = genExpr(e->left);
        std::string rightVal = genExpr(e->right);
        std::string temp = newTemp();
        emit(temp + " = " + leftVal + " " + e->value + " " + rightVal);
        return temp;
    }
};

// =========================================================
// PROGRAM UTAMA
// =========================================================

void printTokens(const std::vector<Token>& tokens) {
    for (auto& t : tokens) {
        if (t.type == TokenType::END) continue;
        std::cout << "<" << tokenTypeName(t.type) << ", \"" << t.text << "\"> ";
    }
    std::cout << "\n";
}

void runPipeline(const std::string& sourceCode, const std::vector<std::string>& preDeclared) {
    std::cout << "==============================================\n";
    std::cout << "Source code masukan:\n" << sourceCode << "\n";
    std::cout << "==============================================\n\n";

    // 1. Leksikal
    std::cout << ">> TAHAP 1: ANALISIS LEKSIKAL (TOKENIZING)\n";
    Lexer lexer(sourceCode);
    std::vector<Token> tokens = lexer.tokenize();
    printTokens(tokens);
    std::cout << "\n";

    // 2. Sintaksis
    std::cout << ">> TAHAP 2: ANALISIS SINTAKSIS (AST)\n";
    Parser parser(tokens);
    IfElseNode ast = parser.parseIfElse();
    printAST(ast);
    std::cout << "\n";

    // 3. Semantik
    std::cout << ">> TAHAP 3: ANALISIS SEMANTIK\n";
    SemanticAnalyzer semanticAnalyzer(preDeclared);
    semanticAnalyzer.analyze(ast);
    std::cout << "\n";

    // 4. Generasi TAC
    std::cout << ">> TAHAP 4: GENERASI THREE-ADDRESS CODE (TAC)\n";
    TACGenerator tacGen;
    std::vector<std::string> tac = tacGen.generate(ast);
    for (auto& line : tac) std::cout << line << "\n";
    std::cout << "\n";
}

int main() {
    try {
        // Contoh 1: kondisi & assignment sederhana
        runPipeline(
            "if (x > 5) { y = 1; } else { y = 0; }",
            {"x"}
        );

        std::cout << "\n\n";

        // Contoh 2: ekspresi lebih kompleks untuk menunjukkan penggunaan temp variable
        runPipeline(
            "if (a + b > c * 2) { z = a + b; } else { z = c - a; }",
            {"a", "b", "c"}
        );

        std::cout << "\n\n";

        // Contoh 3: memicu Semantic Error (variabel belum dideklarasikan)
        std::cout << ">> DEMONSTRASI ERROR SEMANTIK\n";
        runPipeline(
            "if (m > 1) { n = 1; } else { n = 0; }",
            {} // m tidak dideklarasikan
        );

    } catch (const std::exception& ex) {
        std::cout << "[GAGAL] " << ex.what() << "\n";
    }

    return 0;
}
