#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <cctype>
#include <algorithm>
#include <unordered_set>
#include <cstring>    
#include <queue> 
#include <map>
#include <stack>

#define MAX_PROD 100
#define MAX 50
#define MAX_STATE 100
#define MAX_INPUT_SIZE 256
#define MAX_TOKEN_LEN 64 

// Token类型定义
enum TokenType {
    TOK_IDENTIFIER,   // 标识符
    TOK_TRUE,         // 布尔常量 "true"
    TOK_FALSE,        // 布尔常量 "false"
    TOK_UNION,        // 逻辑 "V"
    TOK_INTERSECTION, // 逻辑 " ^ "
    TOK_OR,           // 逻辑 "||"
    TOK_AND,          // 逻辑 "&&"
    TOK_LPAREN,       // 左括号 '('
    TOK_RPAREN,       // 右括号 ')'
    TOK_NOT,          // 逻辑 NOT
    TOK_ILLEGAL,      // 非法字符
    TOK_END,          // 输入结束
};

struct Token {
    TokenType type;
    std::string value;  
};

// 产生式结构体：左部和右部
struct Production {
    std::string left;
    std::string rights; // 右部可能包含多个符号

    bool operator==(const Production& other) const {
        return (left == other.left) && (rights == other.rights); 
    }
};

struct LR0Item {
    Production p;        // 产生式
    int dot_location;    // 点的位置

    bool operator==(const LR0Item& other) const {
        return (p == other.p) && (dot_location == other.dot_location);
    }
};

// 重载输出流操作符
std::ostream& operator<<(std::ostream& os, const LR0Item& item) {
    os << item.p.left << " -> ";
    for (int i = 0; i < item.p.rights.size(); ++i) {
        if (i == item.dot_location) {
            os << ".";
        }
        os << item.p.rights[i];
    }
    if (item.dot_location == item.p.rights.size()) {
        os << ".";
    }
    return os;
}

struct LR0Items {
    // 动态大小的数组
    std::vector<LR0Item> items;
};
std::ostream& operator<<(std::ostream& os, const LR0Items& items) {
    for (const auto& item : items.items) {
        os << item << std::endl;
    }
    return os;
};

struct CanonicalCollection {
    std::vector<LR0Items> items;
};
std::ostream& operator<<(std::ostream& os, const CanonicalCollection& cc) {
    os << "Canonical Collection: " << std::endl;
    for (size_t i = 0; i < cc.items.size(); ++i) {
        os << "State " << i << ": " << std::endl;
        os << cc.items[i] << std::endl;
    }

    return os;
}

struct Grammar {
    int num;  // 产生式数量
    std::vector<std::string> T;  // 终结符
    std::vector<std::string> N;  // 非终结符
    std::vector<Production> prods;  // 产生式
} grammar;

// FIRST集和FOLLOW集
std::unordered_map<std::string,std::unordered_set<std::string>> first;  // 非终结符的FIRST集
std::unordered_map<std::string, std::unordered_set<std::string>> follow; // 非终结符的FOLLOW集


enum ActionType {
    SHIFT,  // 移进
    REDUCE, // 规约
    ACCEPT, // 接受
    ERROR   // 错误
};

// 用于存储动作信息
struct ActionItem {
    ActionType actionType;  // 动作类型
    int stateOrRule;        // 如果是移进，表示目标状态；如果是规约，表示产生式编号
};
std::map<int, std::map<std::string, ActionItem>> action;
std::map<int, std::map<std::string, int>> goton; 

// 分析栈
struct StackItem {
    int state;
    std::string symbol; 
};

// 词法分析
Token create_token(TokenType type, char ch) {
    Token token;
    token.type = type;
    token.value = std::string(1, ch);  
    return token;
}
// 处理标识符或布尔值
Token handle_identifier_or_boolean(int ch, std::ifstream& source) {
    std::string buffer;
    while (isalnum(ch) || ch == '_') {  
        buffer += ch;
        ch = source.get();  
    }
    source.unget();

    Token token;
    if (buffer == "true") {
        token.type = TOK_TRUE;
    } else if (buffer == "false") {
        token.type = TOK_FALSE;
    } else {
        token.type = TOK_IDENTIFIER;
    }

    token.value = buffer;
    return token;
}

// 获取下一个 token
Token get_next_token(std::ifstream& source) {
    int ch = source.get();  
    Token token;

    while (isspace(ch)) {
        ch = source.get();
    }

    if (ch == 'V') {
        return create_token(TOK_UNION, ch);
    }

    if (ch == '|') {
        ch = source.get();
        if (ch == '|') {
            return create_token(TOK_OR, 'V');
        } else {
            source.unget();
            return create_token(TOK_ILLEGAL, '|');
        }
    }

    if (isalpha(ch)) {
        return handle_identifier_or_boolean(ch, source);
    }

    if (ch == '^') {
        return create_token(TOK_INTERSECTION, '^');
    }

    if (ch == '&') {
        ch = source.get();
        if (ch == '&') {
            return create_token(TOK_AND, '^');
        } else {
            source.unget();
            return create_token(TOK_ILLEGAL, '&');
        }
    }

    if (ch == '-' || ch == '!') {
        return create_token(TOK_NOT, '-');
    }

    if (ch == '(') {
        return create_token(TOK_LPAREN, '(');
    }

    if (ch == ')') {
        return create_token(TOK_RPAREN, ')');
    }

    if (ch == EOF) {
        return create_token(TOK_END, '\0');
    }

    return create_token(TOK_ILLEGAL, ch);
}


// 读取文法
void read_grammar_from_file(std::ifstream& source) {
    // 读取非终结符
    std::string line;
    std::getline(source, line);
    std::istringstream non_terminal_stream(line);
    std::string symbol;
    while (std::getline(non_terminal_stream, symbol, ',')) {
        grammar.N.push_back(symbol);
    }

    // 读取终结符
    std::getline(source, line);
    std::istringstream terminal_stream(line);
    while (std::getline(terminal_stream, symbol, ',')) {
        grammar.T.push_back(symbol);
    }

    // 读取产生式
    while (std::getline(source, line)) {
        if (line.empty()) continue;  // 忽略空行
        // 在当前行 line 中查找字符串 "->" 的位置，返回该位置的索引。如果找到了 "->"，返回的位置会是一个有效的索引；
        // 如果没有找到，返回 std::string::npos
        size_t arrow_pos = line.find("->");
        if (arrow_pos != std::string::npos) {
            std::string lhs = line.substr(0, arrow_pos);  // 左部
            std::string rhs = line.substr(arrow_pos + 2);  // 右部
                        
            grammar.N.push_back(lhs);  // 将其他左部加入非终结符列表
            
            Production p;
            p.left = lhs;

            // 右部的符号按空格分开
            std::istringstream rhs_stream(rhs);
            char symbol;
            while (rhs_stream >> symbol) {
                p.rights.push_back(symbol);
            }

            grammar.prods.push_back(p);
        }
    }

    // // 打印非终结符集合
    // std::cout << "Non-Terminals: ";
    // for (const auto& nt : grammar.N) {
    //     std::cout << nt << " ";
    // }
    // std::cout << "\n";

    // // 打印终结符集合
    // std::cout << "Terminals: ";
    // for (const auto& t : grammar.T) {
    //     std::cout << t << " ";
    // }
    // std::cout << "\n";

    // // 打印每条产生式
    // std::cout << "Productions:\n";
    // for (const auto& prod : grammar.prods) {
    //     std::cout << prod.left << " -> ";
    //     for (const auto& symbol : prod.rights) {
    //         std::cout << symbol << " ";
    //     }
    //     std::cout << "\n";
    // }
}


// // 获取FIRST集
// void getFirstSet() {
//     // 为所有终结符初始化 FIRST 集
//     for (const auto& t : grammar.T) {
//         first[t].insert(t);  // 终结符的 FIRST 集包含它自己
//     }
    
//     bool change = true;
//     while (change) {
//         change = false;
//         for (const auto& p : grammar.prods) {
//             std::string X = p.left;  // 当前产生式左部

//             if (p.rights == "true" || p.rights == "false"){
//                 if (first.find(p.rights) != first.end()) {
//                     // 将 FIRST(symbol) 添加到 FIRST(X) 中
//                     for (const auto& f : first[p.rights]) {
//                         if (first[X].insert(f).second) {
//                             change = true;  // 标记发生了变化
//                         }
//                     }
//                 }
//             }else{
//                 // 遍历右部的每个符号（rights 是一个字符串）
//                 for (const char& symbol : p.rights) {
                    
//                     std::string symbol_str(1, symbol);  // 将字符转换为字符串
                    
//                     // 如果符号是终结符或已经计算过的非终结符
//                     if (first.find(symbol_str) != first.end()) {
//                         // 将 FIRST(symbol) 添加到 FIRST(X) 中
//                         for (const auto& f : first[symbol_str]) {
//                             if (first[X].insert(f).second) {
//                                 change = true;  // 标记发生了变化
//                             }
//                         }
//                     } else {
//                         // 如果符号是终结符，直接添加到 FIRST(X)
//                         if (first[X].insert(symbol_str).second) {
//                             change = true;  // 标记发生了变化
//                         }
//                         break;  // 如果是终结符，就没有必要继续处理下一个符号
//                     }
//                 }
//             }
            
//         }
//     }
// }

// void getFollowSet() {
//     // 初始化 Follow 集：每个非终结符的 Follow 集包含 $（结束符）
//     for (const auto& t : grammar.N) {
//         follow[t].insert("$");
//     }

//     bool change = true;
//     while (change) {
//         change = false;
//         // 遍历每个产生式
//         for (const auto& p : grammar.prods) {
//             std::string A = p.left;  // A -> 右部产生式

//             for (size_t i = 0; i < p.rights.size(); ++i) {
//                 std::string alpha = p.rights.substr(i);  // 当前右部的剩余部分

//                 for (size_t j = 0; j < alpha.size(); ++j) {
//                     std::string B(1, alpha[j]);  // 当前符号 B
//                     if (first.find(B) != first.end()) {  // 如果 B 是非终结符
//                         // 检查 Beta 是空的情况
//                         if (j == alpha.size() - 1 || first[alpha.substr(j+1)].count("ε") > 0) {
//                             // 如果 Beta 可以推导出 ε，Follow(A) 的元素应该进入 Follow(B)
//                             for (const auto& f : follow[A]) {
//                                 if (follow[B].insert(f).second) {
//                                     change = true;  // 标记发生了变化
//                                 }
//                             }
//                         }

//                         // 如果符号后面有非空部分，且这个部分的首符号是终结符
//                         if (j < alpha.size() - 1) {
//                             std::string next_symbol(1, alpha[j + 1]);
//                             if (first.find(next_symbol) != first.end()) {
//                                 for (const auto& f : first[next_symbol]) {
//                                     if (follow[B].insert(f).second) {
//                                         change = true;
//                                     }
//                                 }
//                             }
//                         }
//                     }
//                 }
//             }
//         }
//     }
// }


void closure(LR0Items &items) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = 0; i < items.items.size(); ++i) {
            LR0Item &item = items.items[i];

            // 如果点没有在右部的最后位置
            if (item.dot_location < item.p.rights.size()) {
                std::string rightPart = item.p.rights.substr(item.dot_location, 1);  // 获取当前符号
                // 如果符号是非终结符，添加相应的产生式
                if (std::find(grammar.N.begin(), grammar.N.end(), rightPart) != grammar.N.end()) {
                    // 当前符号是非终结符，添加相应的产生式
                    for (const auto& prod : grammar.prods) {
                        if (prod.left == rightPart) {  // 如果产生式的左部与当前符号相同
                            LR0Item new_item = {prod, 0};  // 点从0开始
                            // 如果新产生的项还不在当前的项集中，则添加
                            if (std::find(items.items.begin(), items.items.end(), new_item) == items.items.end()) {
                                items.items.push_back(new_item);
                                changed = true;
                            }
                        }
                    }
                }
            }
        }
    }
}

void go(const LR0Items &items, const std::string& symbol, LR0Items &new_items) {
    // 遍历当前项集中的每一项
    // std::cout << "symbol: " << symbol << std::endl;
    for (const LR0Item &item : items.items) {
        // 判断点所在位置是否在产生式的右部范围内
        if (item.dot_location < item.p.rights.size()) {
            std::string rightSymbol = item.p.rights.substr(item.dot_location, symbol.size());  // 获取当前符号，长度为 symbol 的大小
            // std::cout << "rightSymbol: " << rightSymbol << std::endl;

            // 如果符号是 "true" 或 "false"，直接比较
            if (rightSymbol == symbol) {
                LR0Item new_item = {item.p, item.dot_location + symbol.size()};  // 移动点，跳过整个符号
                new_items.items.push_back(new_item);  // 将新的项添加到新的项集中
                // std::cout << "new_item: " << new_item << std::endl;
            }
            // 普通情况：右部是单个符号的情况（如终结符或非终结符）
            else if (symbol.size() == 1 && rightSymbol == symbol) {
                // 如果符号是单个字符，进行匹配
                LR0Item new_item = {item.p, item.dot_location + 1};  // 移动点
                new_items.items.push_back(new_item);  // 将新的项添加到新的项集中
                // std::cout << "new_item: " << new_item << std::endl;
            }
        }
    }

    // std::cout << "new_items: " << new_items << std::endl;
    closure(new_items);  // 执行闭包
}



std::string getStateKey(const LR0Items& items) {
    std::string key;
    for (const auto& item : items.items) {
        key += item.p.left;
        key += std::to_string(item.dot_location);
    }
    return key;
}


CanonicalCollection buildCanonicalCollection() {
    CanonicalCollection cc;
    std::unordered_map<std::string, int> visited;
    std::queue<LR0Items> stateQueue;

    // 初始项目集
    LR0Items I0;
    LR0Item startItem = {grammar.prods[0], 0}; 
    I0.items.push_back(startItem);
    closure(I0);  // 计算闭包
    cc.items.push_back(I0);

    // 初始化状态队列
    stateQueue.push(I0);

    // 处理项目集
    while (!stateQueue.empty()) {
        LR0Items currentItems = stateQueue.front();
        stateQueue.pop();

        // 处理终结符
        for (const std::string& terminal : grammar.T) {
            LR0Items newState;
            go(currentItems, terminal, newState);  // 用符号（可能是字符串）调用 go
            if (!newState.items.empty()) {
                std::string stateKey = getStateKey(newState);
                if (visited.find(stateKey) == visited.end()) {
                    visited[stateKey] = cc.items.size();
                    cc.items.push_back(newState);
                    stateQueue.push(newState);
                }
            }
        }

        // 处理非终结符
        for (const std::string& nonTerminal : grammar.N) {
            LR0Items newState;
            go(currentItems, nonTerminal, newState);  // 同样处理非终结符
            if (!newState.items.empty()) {
                std::string stateKey = getStateKey(newState);
                if (visited.find(stateKey) == visited.end()) {
                    visited[stateKey] = cc.items.size();
                    cc.items.push_back(newState);
                    stateQueue.push(newState);
                }
            }
        }
    }

    return cc;
}



void generateLR0Table(CanonicalCollection& cc) {
    // 遍历每个状态
    for (int i = 0; i < cc.items.size(); ++i) {
        const LR0Items& items = cc.items[i];

        // 处理终结符，填充 Action 表
        for (const std::string& terminal : grammar.T) {
            std::string input = terminal;  // 使用整个终结符字符串作为输入
            LR0Items newState;
            go(items, input, newState);  // 传入字符串作为输入

            if (!newState.items.empty()) {
                // 查找是否有相同状态
                int newStateIndex = -1;
                for (int j = 0; j < cc.items.size(); ++j) {
                    if (cc.items[j].items == newState.items) {
                        // for (int k = 0; k < cc.items[j].items.size() ; k++){
                        //     std::cout << "cc.items[j].items:\n" << cc.items[j].items[k] << std::endl;
                        //     std::cout << "newState.items:\n" << newState.items[k]<< std::endl;
                        // }
                        // printf("-----------------------");
                        // std::cout << "newIndex:"<< j <<std::endl;
                        newStateIndex = j;
                        break;
                    }
                }

                if (newStateIndex != -1) {
                    // 如果找到了新的状态，执行移进操作
                    action[i][input] = {SHIFT, newStateIndex};
                }
            }
        }

        // 处理非终结符，填充 Goto 表
        for (const std::string& nonTerminal : grammar.N) {
            std::string input = nonTerminal;  // 使用整个非终结符字符串作为输入
            LR0Items newState;
            go(items, input, newState);  // 传入字符串作为输入

            if (!newState.items.empty()) {
                // 查找是否有相同状态
                int newStateIndex = -1;
                for (int j = 0; j < cc.items.size(); ++j) {
                    if (cc.items[j].items == newState.items) {
                        newStateIndex = j;
                        break;
                    }
                }

                if (newStateIndex != -1) {
                    // 如果找到了新的状态，执行 Goto 操作
                    goton[i][input] = newStateIndex;
                }
            }
        }

        // 处理规约操作，填充 Action 表
        for (const auto& item : items.items) {
            if (item.dot_location == item.p.rights.size()) {  // 如果点在右部末尾
                // 对应的产生式右部完全匹配，可以进行规约
                std::string left = item.p.left;  // 规约的左部符号

                // 直接使用产生式的位置（即产生式的顺序号）作为规则编号
                int ruleNumber = 0;
                for (int j = 0; j < grammar.prods.size(); ++j) {
                    if (grammar.prods[j].left == item.p.left && grammar.prods[j].rights == item.p.rights) {
                        ruleNumber = j;  // 记录规约的产生式编号
                        break;
                    }
                }
                
                // 特殊处理 'true' 和 'false'，这些应当作为整体处理
                for (const std::string& terminal : grammar.T) {
                    if (ruleNumber==0){
                        break;
                    }
                   
                    action[i][terminal] = {REDUCE, ruleNumber};  // 记录规约操作
                    
                    action[i][terminal] = {REDUCE, ruleNumber};  // 默认的规约操作
                    
                }
                action[i]["$"] = {REDUCE,ruleNumber};
            }
        }
        
        const LR0Item& item = items.items[0];
        // 检查是否匹配到开始符号的产生式
        if (item.p.left == "S'" && item.dot_location == item.p.rights.size()) {
            // 如果栈顶符号是开始符号并且输入流已消耗完
             action[i]["$"] = {ACCEPT, 0};  // 接受状态
        }
        
    }

    // // 打印 Action 和 Goto 表
    // std::cout << "Action Table:\n";
    // for (const auto& entry : action) {
    //     for (const auto& actionEntry : entry.second) {
    //         const ActionItem& actionItem = actionEntry.second;
    //         std::cout << "state " << entry.first << " -> input " << actionEntry.first << " : ";
    //         if (actionItem.actionType == SHIFT) {
    //             std::cout << "shift to state " << actionItem.stateOrRule;
    //         } else if (actionItem.actionType == REDUCE) {
    //             std::cout << "reduce by rule " << actionItem.stateOrRule;
    //         } else if (actionItem.actionType == ACCEPT) {
    //             std::cout << "accept";
    //         } else {
    //             std::cout << "error";
    //         }
    //         std::cout << std::endl;
    //     }
    // }

    // std::cout << "Goto Table:\n";
    // for (const auto& entry : goton) {
    //     for (const auto& gotoEntry : entry.second) {
    //         std::cout << "state " << entry.first << " -> nonTerminal " << gotoEntry.first << " : " << gotoEntry.second << std::endl;
    //     }
    // }
}
void printStateStack(std::stack<int> stateStack) {
    std::vector<int> temp;
    while (!stateStack.empty()) {
        temp.push_back(stateStack.top());
        stateStack.pop();
    }
    
    std::cout << "State Stack: ";
    for (int state : temp) {
        std::cout << state << " ";
    }
    std::cout << std::endl;
}

// 打印符号栈
void printSymbolStack(std::stack<std::string> symbolStack) {
    std::vector<std::string> temp;
    while (!symbolStack.empty()) {
        temp.push_back(symbolStack.top());
        symbolStack.pop();
    }

    std::cout << "Symbol Stack: ";
    for (const std::string& symbol : temp) {
        std::cout << symbol << " ";
    }
    std::cout << std::endl;
}

bool parse(const std::vector<Token>& inputTokens) {
    std::stack<int> stateStack;  // 状态栈
    std::stack<std::string> symbolStack;  // 符号栈
    
    stateStack.push(0);  // 初始状态
    int inputIndex = 0;

    while (true) {
        int currentState = stateStack.top();
        // std::cout << "currentState: " << currentState << std::endl;

        // 检查是否已读取完输入，避免越界
        if (inputIndex > inputTokens.size()) {
            std::cout << "Input ended unexpectedly!" << std::endl;
            return false;  // 输入结束，但没有正确解析
        }

        // 获取当前输入符号
        std::string currentInput = inputTokens[inputIndex].value;
        // std::cout << "currentInput: " << currentInput << std::endl;

        // 查找 Action 表中的操作
        if (action[currentState].find(currentInput) != action[currentState].end()) {
            const ActionItem& actionItem = action[currentState][currentInput];
            
            if (actionItem.actionType == SHIFT) {  // 移进操作
                stateStack.push(actionItem.stateOrRule);  // 推入新状态
                symbolStack.push(currentInput);  // 推入符号
                // printStateStack(stateStack);
                // printSymbolStack(symbolStack);
                // std::cout << currentInput << std::endl;
                inputIndex++;  // 输入指针前移
                std::cout << "SHIFT to state " << actionItem.stateOrRule << std::endl;
            } 
            else if (actionItem.actionType == REDUCE) {  // 规约操作
                // printf("in reduce\n");
                int ruleIndex = actionItem.stateOrRule;  // 规则编号
                const Production& rule = grammar.prods[ruleIndex];
                // 根据规则右部长度弹出符号栈和状态栈
                if (rule.rights == "true" || rule.rights == "false"){
                    stateStack.pop();
                    symbolStack.pop();
                } else{
                    for (size_t i = 0; i < rule.rights.size(); ++i) {
                        stateStack.pop();
                        symbolStack.pop();
                    }
                }

                // 根据规约的左部（非终结符）查找 Goto 表中的新状态
                std::string nonTerminal = rule.left;
    
                int newState = goton[stateStack.top()][nonTerminal];
                if (newState!=stateStack.top()){
                    stateStack.push(newState);  // 推入新的状态
                }
                
                symbolStack.push(nonTerminal);  // 推入新的符号
                // printStateStack(stateStack);
                // printSymbolStack(symbolStack);
                std::cout << "REDUCE by rule " << ruleIndex << " (" << rule.left << " -> " << rule.rights << ")" << std::endl;
            } 
            else if (actionItem.actionType == ACCEPT) {  // 接受操作
                std::cout << "Input parsed successfully!" << std::endl;
                return true;
            } 
            else {  // 错误操作
                std::cout << "Parsing error!" << std::endl;
                return false;
            }
        } else {
            // 如果没有找到匹配的动作
            std::cout << "No action found for state " << currentState << " and input " << currentInput << std::endl;
            return false;
        }
    }
}

class ASTNode {
public:
    std::string type;        // 节点类型：operator, constant, variable
    std::string value;       // 节点值
    ASTNode* left;          // 左子节点
    ASTNode* right;         // 右子节点

    ASTNode(const std::string& t, const std::string& v) 
        : type(t), value(v), left(nullptr), right(nullptr) {}

    ~ASTNode() {
        delete left;
        delete right;
    }
};

// 打印语法树的类
class ASTPrinter {
public:
    // 打印语法树（带缩进的格式）
    static void printTree(ASTNode* root, int level = 0) {
        if (!root) return;

        // 打印缩进
        std::string indent(level * 4, ' ');
        
        // 打印当前节点
        std::cout << indent << "Type: " << root->type 
                 << ", Value: " << root->value << std::endl;

        // 递归打印子节点
        if (root->left) {
            std::cout << indent << "Left child:" << std::endl;
            printTree(root->left, level + 1);
        }
        if (root->right) {
            std::cout << indent << "Right child:" << std::endl;
            printTree(root->right, level + 1);
        }
    }

    // 以树形结构打印（更直观的显示方式）
    static void printTreeStructure(ASTNode* root, std::string prefix = "", bool isLeft = true) {
        if (!root) return;

        std::cout << prefix;
        std::cout << (isLeft ? "├── " : "└── ");
        std::cout << root->type << ": " << root->value << std::endl;

        // 计算新的前缀
        std::string newPrefix = prefix + (isLeft ? "│   " : "    ");

        // 递归打印子节点
        if (root->left) {
            printTreeStructure(root->left, newPrefix, root->right != nullptr);
        }
        if (root->right) {
            printTreeStructure(root->right, newPrefix, false);
        }
    }
};

class ASTBuilder {
private:
    std::vector<std::string> tokens;
    int currentIndex;

    bool hasMoreTokens() {
        return currentIndex < tokens.size();
    }

    std::string getNextToken() {
        if (hasMoreTokens()) {
            return tokens[currentIndex++];
        }
        return "";
    }

    std::string peekToken() {
        if (hasMoreTokens()) {
            return tokens[currentIndex];
        }
        return "";
    }

    ASTNode* parseExpression() {
        ASTNode* left = parsePrimary();
        
        while (hasMoreTokens()) {
            std::string op = peekToken();
            if (op != "V" && op != "^") {
                break;
            }
            currentIndex++; // 消耗操作符
            ASTNode* right = parsePrimary();
            ASTNode* newNode = new ASTNode("operator", op);
            left->left = left;
            left->right = right;
            left = newNode;
        }
        
        return left;
    }

    ASTNode* parsePrimary() {
        std::string token = getNextToken();
        
        if (token == "(") {
            ASTNode* node = parseExpression();
            if (getNextToken() != ")") {
                throw std::runtime_error("Expected ')'");
            }
            return node;
        }
        else if (token == "-") {  // 处理NOT操作
            ASTNode* operand = parsePrimary();
            ASTNode* node = new ASTNode("operator", "!");
            node->left = operand;
            return node;
        }
        else if (token == "true" || token == "false") {
            return new ASTNode("constant", token);
        }
        else {
            return new ASTNode("variable", token);
        }
    }

public:
    ASTBuilder() : currentIndex(0) {}

    ASTNode* buildFromTokens(const std::vector<std::string>& tokenList) {
        tokens = tokenList;
        currentIndex = 0;
        return parseExpression();
    }
};


// 四元式结构体
struct Quadruple {
    std::string op;    // 操作符
    std::string arg1;  // 操作数1
    std::string arg2;  // 操作数2
    std::string result; // 结果
    Quadruple(std::string op, std::string arg1, std::string arg2, std::string result)
        : op(op), arg1(arg1), arg2(arg2), result(result) {}
    std::string toString() const {
        std::stringstream ss;
        ss << "(" << op << ", " << arg1 << ", " << arg2 << ", " << result << ")";
        return ss.str();
    }
};

// 四元式生成器类
class QuaternionGenerator {
private:
    std::vector<Quadruple> quaternions;  // 存储生成的四元式
    int tempVarCounter = 0;  // 临时变量计数器
    int labelCounter = 0;    // 标签计数器
    int tempVarCount = 0; 
    // 生成新的临时变量
    std::string newTemp() {
        return "t" + std::to_string(++tempVarCounter);
    }

public:
    // 添加四元式
    bool isEmpty() const {
        return quaternions.empty();
    }
    void clearQuaternions() {
        quaternions.clear();  // 使用 vector 的 clear() 方法清空
        tempVarCounter = 0;   // 重置临时变量计数器
        labelCounter = 0;     // 重置标签计数器
        tempVarCount = 0;     // 重置临时变量计数
    }
    void addQuaternion(const std::string& op, const std::string& arg1, 
                      const std::string& arg2, const std::string& result) {
        quaternions.emplace_back(op, arg1, arg2, result);
    }

    // // 生成赋值语句的四元式
    // std::string genAssign(const std::string& target, const std::string& source) {
    //     addQuaternion("=", source, "", target);
    //     return target;
    // }

    // // 生成算术运算的四元式
    // std::string genArithmeticOp(const std::string& op, const std::string& arg1, 
    //                            const std::string& arg2) {
    //     std::string temp = newTemp();
    //     addQuaternion(op, arg1, arg2, temp);
    //     return temp;
    // }

    // // 生成条件跳转的四元式
    // std::string genCondJump(const std::string& op, const std::string& arg1, 
    //                        const std::string& arg2) {
    //     std::string label = newLabel();
    //     addQuaternion("if" + op, arg1, arg2, label);
    //     return label;
    // }

    // // 生成无条件跳转的四元式
    // std::string genGoto(const std::string& label) {
    //     addQuaternion("goto", "", "", label);
    //     return label;
    // }


    // 生成逻辑运算的四元式
    std::string genNot(const std::string& arg) {
        std::string temp = newTemp();
        addQuaternion("!", arg, "", temp);
        return temp;
    }

    std::string genLogicalOp(const std::string& op, const std::string& arg1, 
                            const std::string& arg2) {
        std::string temp = newTemp();
        addQuaternion(op, arg1, arg2, temp);
        return temp;
    }
    // 打印所有生成的四元式
    void printQuaternions() const {
        for (size_t i = 0; i < quaternions.size(); ++i) {
            std::cout << i << ": " << quaternions[i].toString() << std::endl;
        }
    }

    std::vector<std::string> generateTargetCode() const {
        std::vector<std::string> targetCode;
        std::map<std::string, std::string> registerMap;  // 变量到寄存器的映射
        int registerCounter = 0;  // 寄存器计数器

        // 获取新寄存器
        auto getRegister = [&](const std::string& var) -> std::string {
            if (registerMap.find(var) == registerMap.end()) {
                registerMap[var] = "R" + std::to_string(registerCounter++);
            }
            return registerMap[var];
        };

        // 为常量分配寄存器并生成加载指令
        auto loadConstant = [&](const std::string& value) -> std::string {
            std::string reg = "R" + std::to_string(registerCounter++);
            if (value == "true") {
                targetCode.push_back("MOV " + reg + ", #1");
            } else if (value == "false") {
                targetCode.push_back("MOV " + reg + ", #0");
            } else {
                targetCode.push_back("MOV " + reg + ", " + value);
            }
            return reg;
        };

        for (const auto& q : quaternions) {
            std::string resultReg = getRegister(q.result);
            
            if (q.op == "!") {
                // 处理NOT操作
                std::string arg1Reg;
                if (q.arg1 == "true" || q.arg1 == "false") {
                    arg1Reg = loadConstant(q.arg1);
                } else {
                    arg1Reg = getRegister(q.arg1);
                }
                targetCode.push_back("NOT " + arg1Reg + ", " + resultReg);
            }
            else if (q.op == "V") {
                // 处理OR操作
                std::string arg1Reg, arg2Reg;
                
                if (q.arg1 == "true" || q.arg1 == "false") {
                    arg1Reg = loadConstant(q.arg1);
                } else {
                    arg1Reg = getRegister(q.arg1);
                }
                
                if (q.arg2 == "true" || q.arg2 == "false") {
                    arg2Reg = loadConstant(q.arg2);
                } else {
                    arg2Reg = getRegister(q.arg2);
                }
                
                targetCode.push_back("OR " + arg1Reg + ", " + arg2Reg + ", " + resultReg);
            }
            else if (q.op == "^") {
                // 处理AND操作
                std::string arg1Reg, arg2Reg;
                
                if (q.arg1 == "true" || q.arg1 == "false") {
                    arg1Reg = loadConstant(q.arg1);
                } else {
                    arg1Reg = getRegister(q.arg1);
                }
                
                if (q.arg2 == "true" || q.arg2 == "false") {
                    arg2Reg = loadConstant(q.arg2);
                } else {
                    arg2Reg = getRegister(q.arg2);
                }
                
                targetCode.push_back("AND " + arg1Reg + ", " + arg2Reg + ", " + resultReg);
            }
        }

        return targetCode;
    }

    // 打印目标代码
    void printTargetCode() const {
        std::vector<std::string> targetCode = generateTargetCode();
        std::cout << "Target Code:" << std::endl;
        for (size_t i = 0; i < targetCode.size(); ++i) {
            std::cout << i << ": " << targetCode[i] << std::endl;
        }
    }
};


bool isLogicalOperator(const std::string& op) {
    return op == "&&" || op == "||" || op == "!" || op == "V" || op == "^" || op == "-";
}


std::string parseAndGenerateQuadruples(const std::vector<std::string>& expression, QuaternionGenerator& generator, int& index) {
    std::string operand1, operand2, result;
    std::string currentOp;  // 当前操作符

    while (index < expression.size()) {
        std::string token = expression[index++];

        if (token == "(") {
            operand2 = parseAndGenerateQuadruples(expression, generator, index);
            if (!currentOp.empty() && !operand1.empty()) {
                result = generator.genLogicalOp(currentOp, operand1, operand2);
                operand1 = result;
                currentOp = "";
            } else {
                operand1 = operand2;
            }
        } 
        else if (token == ")") {
            return result.empty() ? operand1 : result;
        } 
        else if (token == "-") {
            // 处理逻辑非操作
            if (index < expression.size()) {
                std::string nextToken = expression[index++];
                operand2 = generator.genNot(nextToken);
                if (!currentOp.empty() && !operand1.empty()) {
                    result = generator.genLogicalOp(currentOp, operand1, operand2);
                    operand1 = result;
                    currentOp = "";
                } else {
                    operand1 = operand2;
                }
            }
        } 
        else if (isLogicalOperator(token)) {
            // 处理逻辑操作符
            currentOp = token;
        } 
        else {
            // 处理操作数
            operand2 = token;
            if (!currentOp.empty() && !operand1.empty()) {
                result = generator.genLogicalOp(currentOp, operand1, operand2);
                operand1 = result;
                currentOp = "";
            } else {
                operand1 = operand2;
            }
        }
    }

    return result.empty() ? operand1 : result;
}


std::string optimization(const std::vector<std::string>& expression, QuaternionGenerator& generator, int& index) {
    std::string operand1, operand2, result;
    std::string currentOp;  // 当前操作符

    while (index < expression.size()) {
        std::string token = expression[index++];

        if (token == "(") {
            operand2 = optimization(expression, generator, index);
            if (!currentOp.empty() && !operand1.empty()) {
                result = generator.genLogicalOp(currentOp, operand1, operand2);
                operand1 = result;
                currentOp = "";
            } else {
                operand1 = operand2;
            }
        } 
        else if (token == ")") {
            return result.empty() ? operand1 : result;
        } 
        else if (token == "-") {
            // 处理逻辑非操作
            if (index < expression.size()) {
                std::string nextToken = expression[index++];
                
                // 对常量进行折叠
                if (nextToken == "true") {
                    operand2 = "false";  // !true = false
                } else if (nextToken == "false") {
                    operand2 = "true";   // !false = true
                } else {
                    operand2 = generator.genNot(nextToken);  // 对其他情况使用 genNot
                }

                if (!currentOp.empty() && !operand1.empty()) {
                    result = generator.genLogicalOp(currentOp, operand1, operand2);
                    operand1 = result;
                    currentOp = "";
                } else {
                    operand1 = operand2;
                }
            }
        } 
        else if (isLogicalOperator(token)) {
            // 处理逻辑操作符
            currentOp = token;
        } 
        else {
            // 处理操作数
            operand2 = token;
            
            // 优化：常量传播（例如 true || false 直接变为 true）
            // if ((operand1 == "true" && operand2 == "false") || (operand1 == "false" && operand2 == "true")) {
            //     if (currentOp == "^") {
            //         result = "false";  // true && false = false
            //     } else {
            //         result = "true";  // true || false = true
            //     }
            //     operand1 = result;
            //     currentOp = "";
            //     return result;  // 立即返回计算结果，避免后续无效计算
            // }
            // // if  (operand1 == "true" && operand2 == "true") {
            // //     result = "true";
            // //     operand1 = result;
            // //     currentOp = "";
            // //     return result;
            // // }
            // // if (operand1 == "false" && operand2 == "false"){
            // //     result = "false";
            // //     operand1 = result;
            // //     currentOp = "";
            // //     return result;
            // // }
            // 如果有逻辑运算符，生成四元式
            if (!currentOp.empty() && !operand1.empty()) {
                result = generator.genLogicalOp(currentOp, operand1, operand2);
                operand1 = result;
                currentOp = "";
            } else {
                operand1 = operand2;
            }
        }
    }

    // 最后如果还有操作符未生成四元式，生成最后一个四元式
    if (!currentOp.empty() && !operand1.empty()) {
        result = generator.genLogicalOp(currentOp, operand1, operand2);
    }

    return result.empty() ? operand1 : result;
}



// 显示菜单
void display_menu() {
    std::cout << "选择功能：" << std::endl;
    std::cout << "1. 词法分析" << std::endl;
    std::cout << "2. 语法分析" << std::endl;
    std::cout << "3. 语义分析" << std::endl;
    std::cout << "4. 中间代码生成" << std::endl;
    std::cout << "5. 中间代码优化" << std::endl;
    std::cout << "6. 目标代码生成" << std::endl;
    std::cout << "0. 退出" << std::endl;
}

int main() {
    // 打开源文件用于词法分析
    std::ifstream source("source.txt");
    if (!source.is_open()) {
        std::cerr << "无法打开源文件！" << std::endl;
        return 1;
    }

    std::ifstream input("input.txt"); // 打开语法文件
    if (!input.is_open()) {
        std::cerr << "无法打开语法文件！" << std::endl;
        return 1;
    }

    std::vector<Token> inputTokens;
    Token token;
    while ((token = get_next_token(source)).type != TOK_END) {
        inputTokens.push_back(token);
    }
    QuaternionGenerator generator;
    std::vector<std::string> inputs;

    for (int i=0 ; i<inputTokens.size();++i) {
        inputs.push_back(inputTokens[i].value);
    }
    int choice;
    while (true) {
        display_menu();
        std::cin >> choice;

        switch (choice) {
            case 1: { // 词法分析
                for (int i=0 ; i<inputTokens.size();++i) {
                    std::cout << inputTokens[i].type<< " " << inputTokens[i].value << std::endl;
                }
                break;
            }
            case 2: { // 语法分析    
                read_grammar_from_file(input);
                // getFirstSet();
                // getFollowSet();
                // 生成LR1表和分析
                CanonicalCollection cc = buildCanonicalCollection();
                // std::cout << cc << std::endl;
                generateLR0Table(cc);
                
                inputTokens.push_back(Token{TOK_END, "$"}); // 添加结束符

                bool result = parse(inputTokens); // 调用语法分析函数
                if (result) {
                    std::cout << "语法分析成功！" << std::endl;
                } else {
                    std::cout << "语法分析失败！" << std::endl;
                }

                break;
            }
            case 3: {
                ASTBuilder builder;
        
               
                    ASTNode* tree = builder.buildFromTokens(inputs);
                    // ASTPrinter::printTree(tree);
                    ASTPrinter::printTreeStructure(tree);
                    delete tree;
                
                
             
                break;
            }
            case 4: {
                generator.clearQuaternions();
                // 解析表达式并生成四元式
                int index = 0;
                parseAndGenerateQuadruples(inputs,generator,index);
                generator.printQuaternions(); // 打印所有四元式
                break;
            }
            case 5: {
                generator.clearQuaternions();
                // 中间代码优化的功能
                int index = 0;
                optimization(inputs,generator,index);
                generator.printQuaternions();
                break;
            }
            case 6: {
                generator.clearQuaternions();
                int index = 0;
                parseAndGenerateQuadruples(inputs,generator,index);
                generator.printTargetCode();
                // 目标代码生成的功能
                break;
            }
            case 0: { // 退出程序
                source.close(); // 关闭源文件流
                return 0;
            }
            default:
                std::cerr << "无效的选择，请重新输入。" << std::endl;
        }
    }
    // 程序退出时关闭源文件流
    input.close();
    source.close();
    return 0;
}
