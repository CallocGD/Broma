#pragma once

#include "lex.hpp"
#include "ast.hpp"
#include <queue>

#define loop while(true)
using Tokens = queue<Token>;

Token next(Tokens& tokens) {
	if (tokens.size() == 0) {
		cacerr("Unexpected EOF while parsing\n");
	}
	Token p = tokens.front();
	tokens.pop();
	return p;
}

Token peek(Tokens& tokens) {
	if (tokens.size() == 0) {
		cacerr("Unexpected EOF while parsing\n");
	}
	Token p = tokens.front();
	return p;
}

Token next_expect(Tokens& tokens, TokenType type, string tname) {
	auto t = next(tokens);
	if (t.type == type)
		return t;
	cacerr("Expected %s, found %s (next tokens %s)\n", tname.c_str(), t.slice.c_str(), next(tokens).slice.c_str());
}

bool next_if_type(TokenType type, Tokens& tokens) {
	auto t = peek(tokens);
	if (t.type == type) {
		next(tokens);
		return false;
	}
	return true;
}

string parseQualifiedName(Tokens& tokens) {
	string qual;
	while (peek(tokens).type == kIdent || peek(tokens).type == kQualifier) {
		qual += next(tokens).slice;
	}
	if (qual.size() == 0)
		cacerr("Expected identifier, found %s\n", next(tokens).slice.c_str());

	return qual;
}

string parseAttribute(Tokens& tokens) { // if attributes are expanded we will need to create a new attribute ast type intead of string
	string attrib_name = next_expect(tokens, kIdent, "identifier").slice;
	if (attrib_name == "mangle") {
		next_expect(tokens, kParenL, "(");
		string mangle = next_expect(tokens, kString, "string").slice;
		next_expect(tokens, kParenR, ")");
		next_expect(tokens, kAttrR, "]]");
		return mangle;
	} else {
		// we can add more later
		cacerr("Invalid attribute %s\n", attrib_name.c_str());
		return "";
	}
}

void parseFunction(ClassDefinition& c, Function myFunction, Tokens& tokens) {
	next_expect(tokens, kParenL, "(");

	while (next_if_type(kParenR, tokens)) {
		string args;
		while (next_if_type(kComma, tokens) && peek(tokens).type != kParenR) {
			args += next(tokens).slice;
		}
		if (args.size() == 0)
			continue;

		myFunction.args.push_back(args);
	}

	if (!next_if_type(kConst, tokens))
		myFunction.is_const = true;
	next_expect(tokens, kEqual, "=");

	for (int k = 0; k < 4; ++k) {
		if (k == 3)
			cacerr("Maximum of 3 binds allowed\n");

		auto t = peek(tokens);
		if (t.type == kComma)
			myFunction.binds[k] = "";
		else if (t.type == kAddress) {
			next(tokens);
			myFunction.binds[k] = t.slice;
		} else 
			cacerr("Expected address, found %s\n", t.slice.c_str());

		t = next(tokens);
		if (t.type == kSemi)
			break;
		if (t.type != kComma)
			cacerr("Expected comma, found %s.\n", t.slice.c_str());
	}

	c.functions.push_back(myFunction);
	c.in_order.push_back(&c.functions.back());
}

void parseMember(ClassDefinition& c, string type, string varName, Tokens& tokens) {
	//cacerr("not implemented yet!!!\n");

	Member myMember;
	myMember.type = type;
	myMember.name = varName;
	if (!next_if_type(kBrackL, tokens)) {
		auto num_maybe = next_expect(tokens, kIdent, "number").slice;
		if (num_maybe.find_first_not_of("0123456789") != string::npos) {
			cacerr("Expected number, found %s\n", num_maybe.c_str());
		}
		myMember.count = strtoll(num_maybe.c_str(), NULL, 10);

		next_expect(tokens, kBrackR, "]");
	}

	if (!next_if_type(kEqual, tokens)) {
		for (int k = 0; k < 4; ++k) {
			if (k == 3)
				cacerr("Maximum of 3 hardcodes allowed\n");

			auto t = next(tokens);
			if (t.type == kComma)
				myMember.hardcodes[k] = "";
			else if (t.type == kAddress)
				myMember.hardcodes[k] = t.slice;
			else 
				cacerr("Expected address, found %s\n", t.slice.c_str());

			t = next(tokens);
			if (t.type == kSemi)
				break;
			if (t.type != kComma)
				cacerr("Expected comma, found %s.\n", t.slice.c_str());
		}
	} else next_expect(tokens, kSemi, ";");

	c.members.push_back(myMember);
	c.in_order.push_back(&c.members.back());
}

void parseField(ClassDefinition& c, Tokens& tokens) {
	string attrib;
	if (!next_if_type(kAttrL, tokens)) {
		attrib = parseAttribute(tokens);
	}

	bool virt = false;
	bool stat = false;

	if (!next_if_type(kVirtual, tokens)) {
		virt = true;
		if (!next_if_type(kStatic, tokens)) {
			stat = true;
		}
	} else if (!next_if_type(kStatic, tokens)) {
		stat = true;
		if (!next_if_type(kVirtual, tokens)) {
			virt = true;
		}
	}

	if (peek(tokens).type == kInlineExpr) {
		Inline i;
		i.inlined = next(tokens).slice;
		c.inlines.push_back(i);
		c.in_order.push_back(&c.inlines.back());
		return;
	}

	vector<Token> return_name;
	loop {
		Token t = peek(tokens);
		bool break_ = false;
		switch (t.type) {
			case kStar:
			case kAmp:
			case kIdent:
			case kConst:
			case kTemplateExpr:
			case kQualifier:
			case kDtor:
				return_name.push_back(t);
				next(tokens);
				break;
			default:
				break_ = true;
				break;
		}
		if (break_)
			break;
	}

	if (return_name.empty())
		cacerr("Expected identifier, found %s\n", next(tokens).slice.c_str());
	if (return_name.back().type != kIdent)
		cacerr("Expected identifier, found %s\n", return_name.back().slice.c_str());

	string varName = return_name.back().slice;
	return_name.pop_back();
	string return_type;
	for (auto& i : return_name)
		return_type += i.slice;

	if (peek(tokens).type == kParenL) {
		Function myFunction;
		myFunction.return_type = return_type;
		myFunction.android_mangle = attrib;
		myFunction.is_virtual = virt;
		myFunction.is_static = stat;
		myFunction.name = varName;
		return parseFunction(c, myFunction, tokens);
	}

	if (virt)
		cacerr("Unexpected virtual keyword\n")
	if (stat)
		cacerr("Unexpected static keyword\n")
	return parseMember(c, return_type, varName, tokens);
}

void parseClass(Root& r, Tokens& tokens) {
	ClassDefinition myClass;
	next_expect(tokens, kClass, "'class'");
	myClass.name = parseQualifiedName(tokens);

	if (!next_if_type(kColon, tokens)) {
		loop {
			myClass.addSuperclass(parseQualifiedName(tokens));
			//auto t = next(tokens);
			if (!next_if_type(kBraceL, tokens)) 
				break;
			next_expect(tokens, kComma, "comma");
		}
	} else {
		next_expect(tokens, kBraceL, "{");
	}

	while (next_if_type(kBraceR, tokens)) {
		parseField(myClass, tokens);
	}

	r.classes[myClass.name] = myClass;
}

Root parseTokens(vector<Token> ts) {
	Root root;
	Tokens tokens;
	for (auto& t : ts) {
		tokens.push(t);
	}

	while (tokens.size() > 0) {
		parseClass(root, tokens);
	}

	//cacerr("s %d\n", tokens.size());
	return root;
}