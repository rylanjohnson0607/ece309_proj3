// src/tools/calculator_eval.cpp — YOU IMPLEMENT (spec §4.6).
//
// calc_evaluate: evaluate a well-formed token list with two stacks
// (one for numbers, one for operators and '('). See spec §4.6 and the
// worked trace in Appendix C. std::stack or std::vector are both fine.

#include "tools/calculator.h"

namespace {

int precedence(CalcToken::Kind kind) {
    switch (kind) {
        case CalcToken::Kind::Plus:
        case CalcToken::Kind::Minus:
            return 1;
        case CalcToken::Kind::Times:
        case CalcToken::Kind::Divide:
            return 2;
        default:
            return 0; // Numbers and parentheses are not binary operators.
    }
}

// Vectors act as stacks: back() reads the top and pop_back() removes it.
bool apply_top(std::vector<double>& values,
                               std::vector<CalcToken::Kind>& ops,
                               std::string* error) {
    if (ops.empty() || values.size() < 2) {
        *error = "missing operator or operands";
        return false;
    }

    const CalcToken::Kind op = ops.back();
    if (precedence(op) == 0) {
        *error = "expected arithmetic operator";
        return false;
    }
    if (op == CalcToken::Kind::Divide && values.back() == 0.0) {
        *error = "division by zero";
        return false;
    }

    ops.pop_back();
    const double right = values.back();
    values.pop_back();
    const double left = values.back();
    values.pop_back();

    double result = 0.0;
    switch (op) {
        case CalcToken::Kind::Plus:   result = left + right; break;
        case CalcToken::Kind::Minus:  result = left - right; break;
        case CalcToken::Kind::Times:  result = left * right; break;
        case CalcToken::Kind::Divide: result = left / right; break;
        default: // Rejected by the precedence check above.
            *error = "expected arithmetic operator";
            return false;
    }
    values.push_back(result);
    return true;
}

} // namespace

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out, std::string* error) {
    using Kind = CalcToken::Kind;
    std::vector<double> values;
    std::vector<Kind> ops;
    bool expect_value = true;

    for (const auto& token : tokens) {
        if (token.kind == Kind::Number) {
            if (!expect_value) {
                *error = "missing operator";
                return false;
            }
            values.push_back(token.value);
            expect_value = false;
        } else if (token.kind == Kind::LParen) {
            if (!expect_value) {
                *error = "missing operator before parenthesis";
                return false;
            }
            ops.push_back(Kind::LParen);
        } else if (token.kind == Kind::RParen) {
            if (expect_value) {
                *error = "missing value before closing parenthesis";
                return false;
            }
            // Finish only the operations inside this pair of parentheses.
            while (!ops.empty() && ops.back() != Kind::LParen) {
                if (!apply_top(values, ops, error)) return false;
            }
            if (ops.empty()) {
                *error = "unbalanced parentheses";
                return false;
            }
            ops.pop_back(); // Discard the matching '(', not a numeric value.
        } else {
            if (precedence(token.kind) == 0 || expect_value) {
                *error = "invalid operator or missing value";
                return false;
            }
            // Equal precedence is included so subtraction/division go left to right.
            while (!ops.empty() && ops.back() != Kind::LParen &&
                   precedence(ops.back()) >= precedence(token.kind)) {
                if (!apply_top(values, ops, error)) return false;
            }
            ops.push_back(token.kind);
            expect_value = true;
        }
    }

    if (expect_value) {
        *error = "empty expression or missing final value";
        return false;
    }
    while (!ops.empty()) {
        if (ops.back() == Kind::LParen) {
            *error = "unbalanced parentheses";
            return false;
        }
        if (!apply_top(values, ops, error)) return false;
    }
    if (values.size() != 1) {
        *error = "invalid expression";
        return false;
    }
    *out = values.back();
    error->clear();
    return true;
}
