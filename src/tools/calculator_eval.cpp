// src/tools/calculator_eval.cpp — calc_evaluate (spec §4.6).
//
// The classic two-stack algorithm: `values` holds numbers, `ops` holds
// operators and '('. Iterative only, so 20,000 terms or 5,000 nested
// parentheses cannot overflow the call stack.

#include "tools/calculator.h"

#include <stack>

namespace {

using Kind = CalcToken::Kind;

bool is_operator(Kind k) {
    return k == Kind::Plus || k == Kind::Minus || k == Kind::Times || k == Kind::Divide;
}

int precedence(Kind k) {
    return (k == Kind::Times || k == Kind::Divide) ? 2 : 1;
}

// Pops one operator and two operands, pushes `left op right`.
bool apply_top(std::stack<double>& values, std::stack<Kind>& ops, std::string* error) {
    if (ops.empty() || !is_operator(ops.top())) {
        *error = "malformed expression: expected an operator";
        return false;
    }
    Kind op = ops.top();
    ops.pop();

    if (values.size() < 2) {
        *error = "malformed expression: missing operand";
        return false;
    }
    double right = values.top();   // first pop is the RIGHT operand
    values.pop();
    double left = values.top();
    values.pop();

    double result = 0.0;
    switch (op) {
        case Kind::Plus:   result = left + right; break;
        case Kind::Minus:  result = left - right; break;
        case Kind::Times:  result = left * right; break;
        case Kind::Divide:
            if (right == 0.0) {
                *error = "division by zero";
                return false;
            }
            result = left / right;
            break;
        default:
            *error = "malformed expression: unknown operator";
            return false;
    }
    values.push(result);
    return true;
}

}  // namespace

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out, std::string* error) {
    std::stack<double> values;
    std::stack<Kind>   ops;

    for (const CalcToken& tok : tokens) {
        switch (tok.kind) {
            case Kind::Number:
                values.push(tok.value);
                break;

            case Kind::LParen:
                ops.push(Kind::LParen);
                break;

            case Kind::RParen:
                while (!ops.empty() && ops.top() != Kind::LParen) {
                    if (!apply_top(values, ops, error)) return false;
                }
                if (ops.empty()) {
                    *error = "malformed expression: unmatched ')'";
                    return false;
                }
                ops.pop();   // discard the '('
                break;

            case Kind::Plus:
            case Kind::Minus:
            case Kind::Times:
            case Kind::Divide:
                // >= makes equal-precedence operators apply left to right.
                while (!ops.empty() && is_operator(ops.top()) &&
                       precedence(ops.top()) >= precedence(tok.kind)) {
                    if (!apply_top(values, ops, error)) return false;
                }
                ops.push(tok.kind);
                break;
        }
    }

    while (!ops.empty()) {
        if (ops.top() == Kind::LParen) {
            *error = "malformed expression: unmatched '('";
            return false;
        }
        if (!apply_top(values, ops, error)) return false;
    }

    if (values.size() != 1) {
        *error = "malformed expression: expected exactly one result";
        return false;
    }
    *out = values.top();
    return true;
}
