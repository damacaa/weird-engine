#pragma once

#include "MathExpressions.h"

bool compareVectorWithCharArray(const std::vector<char>& vec, const char* charArray)
{
	size_t arrayLength = std::strlen(charArray);

	// Check if sizes match
	if (vec.size() != arrayLength)
	{
		return false;
	}
	// Compare element by element
	return std::equal(vec.begin(), vec.end(), charArray);
}

//std::shared_ptr<IMathExpression> getExpression(std::string text, std::unordered_map<int, FloatVariable>& variables)
//{
//	std::vector<char> letters{};
//
//	int level = 0;
//	for (const char c : text)
//	{
//		switch (c)
//		{
//		case '(':
//		{
//			level++;
//			if (level == 1)
//			{
//				break;
//			}
//
//			// Interpret letters as a function
//
//			if (letters.size() == 0)
//			{
//				// Sub expression
//			}
//			else
//			{
//				// Function
//			}
//
//			break;
//		}
//
//		case ')':
//		{
//			// Interpret letters as second parameter
//			level--;
//			if (level == 0)
//			{
//				// Done
//				break;
//			}
//
//
//			break;
//		}
//		case ' ':
//		{
//			// Variable or sub-expression?
//
//			if (level == 1)
//			{
//
//			}
//
//			break;
//		}
//		case ',':
//		{
//			// Interpret letters as a first parameter
//
//			break;
//		}
//		default:
//			letters.push_back(c);
//			break;
//		}
//	}
//}


#include <regex>
#include <queue>

std::queue<std::string> tokenize(const std::string& input) {
	std::regex tokenRegex(R"([\w\.]+|\+|\-|\*|\/|\(|\)|,)");
	std::sregex_iterator begin(input.begin(), input.end(), tokenRegex);
	std::sregex_iterator end;

	std::queue<std::string> tokens;
	for (auto it = begin; it != end; ++it) {
		tokens.push(it->str());
	}

	return tokens;
}

std::shared_ptr<IMathExpression> deserializeExpression(const std::string& expression);

std::shared_ptr<IMathExpression> parseExpression(std::queue<std::string>& tokens);
std::shared_ptr<IMathExpression> parseFunction(std::queue<std::string>& tokens);
std::shared_ptr<IMathExpression> parseOperation(std::queue<std::string>& tokens);

std::shared_ptr<IMathExpression> parsePrimary(std::queue<std::string>& tokens) {
	if (tokens.empty()) throw std::runtime_error("Unexpected end of expression.");

	std::string token = tokens.front();
	tokens.pop();

	if (std::isdigit(token[0]) || token[0] == '-') {
		// Parse a constant
		return std::make_shared<StaticVariable>(std::stof(token));
	}
	else if (token.substr(0, 3) == "var") {
		// Parse a variable
		return std::make_shared<FloatVariable>(std::stoi(token.substr(3)));
	}
	else if (token == "(") {
		// Parse a sub-expression inside parentheses
		auto expr = parseExpression(tokens);

		if (tokens.empty() || tokens.front() != ")")
			throw std::runtime_error("Expected ')'");

		tokens.pop(); // Consume ')'
		return expr;
	}
	else if (token == "sin" || token == "abs") {
		// Parse unary functions
		if (tokens.empty() || tokens.front() != "(") throw std::runtime_error("Expected '(' after " + token);
		tokens.pop(); // Consume '('
		auto value = parseExpression(tokens);
		if (tokens.empty() || tokens.front() != ")") throw std::runtime_error("Expected ')'");
		tokens.pop(); // Consume ')'

		if (token == "sin") return std::make_shared<Sine>(value);
		if (token == "abs") return std::make_shared<Abs>(value);
	}

	throw std::runtime_error("Unexpected token: " + token);
}


std::shared_ptr<IMathExpression> parseBinary(std::queue<std::string>& tokens, int precedence = 0) {

	// Remove the first element
	tokens.pop();
	std::deque<int>& d = *reinterpret_cast<std::deque<int>*>(&tokens);
	// Remove the last element
	d.pop_back();

	auto left = parsePrimary(tokens);

	while (!tokens.empty()) {
		std::string op = tokens.front();

		int currentPrecedence = 0;
		if (op == "+" || op == "-") currentPrecedence = 1;
		else if (op == "*" || op == "/") currentPrecedence = 2;

		// If the current operator has lower precedence, stop parsing
		if (currentPrecedence < precedence) break;

		tokens.pop(); // Consume the operator

		// Parse the right-hand side of the binary operation
		auto right = parseBinary(tokens, currentPrecedence + 1);

		// Construct the appropriate binary operation
		if (op == "+") left = std::make_shared<Addition>(left, right);
		if (op == "-") left = std::make_shared<Substraction>(left, right);
		if (op == "*") left = std::make_shared<Multiplication>(left, right);
	}

	return left;
}


std::shared_ptr<IMathExpression> parseExpression(std::queue<std::string>& tokens)
{
	return parseBinary(tokens);
}


std::shared_ptr<IMathExpression> deserializeExpression(const std::string& expression)
{
	auto tokens = tokenize(expression);
	return parseExpression(tokens);
}