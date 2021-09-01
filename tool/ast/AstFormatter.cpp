#include "AstFormatter.h"

#include <sstream>
#include <iterator>
#include <algorithm>
#include <numeric>

static constexpr const char* resetColor = "\x1b[0m";

static inline std::string getColorFor(FormatOptions::Highlight kind)
{
	static constexpr const char* red = "\x1b[31;1m";
	static constexpr const char* green = "\x1b[32;1m";
	static constexpr const char* yellow = "\x1b[33;1m";
	static constexpr const char* blue = "\x1b[34;1m";
	static constexpr const char* magenta = "\x1b[35;1m";
	static constexpr const char* cyan = "\x1b[36;1m";
	static constexpr const char* white = "\x1b[37;1m";

	switch(kind)
	{
	case FormatOptions::Highlight::Function:
		return red;
	case FormatOptions::Highlight::TypeDef:
		return yellow;
	case FormatOptions::Highlight::TypeRef:
		return white;
	case FormatOptions::Highlight::Argument:
		return green;
	case FormatOptions::Highlight::Member:
		return cyan;
	case FormatOptions::Highlight::Primitive:
		return blue;
	case FormatOptions::Highlight::Session:
		return magenta;
	default:
		return resetColor;
	}
}

inline std::string FormatOptions::colorize(const std::string& str, Highlight kind) const
{
	if(colored)
	{
		return getColorFor(kind) + str + resetColor;
	}

	return str;
}

inline std::string FormatOptions::indent(const int n) const {
	return std::string(n * indentStep, ' ');
}

inline std::string FormatOptions::formatNewlineIndentDelimit(const int n, const std::string& str, const char start, const char end) const
{
	if(pretty && str.find('\n') != std::string::npos)
	{
		std::stringstream ss;
		ss << "\n" << indent(n) << start;
		ss << indent(n + 1) << str << "\n";
		ss << indent(n) << end;
		return ss.str();
	}
	else
	{
		return start + str + end;
	}
}

static inline std::string typeRef(const FormatOptions& opts, const int n, const Ast::TypeRef& t);

static inline std::string formatComent(const FormatOptions& opts, const int n, const std::string &text)
{
	std::stringstream ss;

	if(text.length())
	{
		ss << "/* " << text << " */";

		if(opts.pretty)
		{
			ss << "\n" << opts.indent(n);
		}
		else
		{
			ss << " ";
		}
	}

	return ss.str();
}

static inline std::string memberItem(const FormatOptions& opts, const int n, const Ast::Var &v, FormatOptions::Highlight h) {
	return formatComent(opts, n, v.docs) + opts.colorize(v.name, h) + ": "  + typeRef(opts, n, v.type);
}

template<class C, class F>
static inline std::string list(const FormatOptions& opts, const int n, const C &vs, F&& f)
{
	if(vs.size())
	{
		if(const auto first = f(opts, n, vs.front()); vs.size() == 1 && first.find('\n') == std::string::npos)
		{
			return first;
		}

		std::stringstream ss;

		bool isFirst = true;
		for(const auto& v: vs)
		{
			if(opts.pretty)
			{
				ss << (isFirst ? "\n" : ",\n") << opts.indent(n) << f(opts, n, v);
			}
			else
			{
				ss << (isFirst ? "" : ", ") << f(opts, n, v);
			}

			isFirst = false;
		}

		return ss.str();
	}

	return {};
}

static inline std::string typeRefKindToString(const FormatOptions& opts, const int n, const Ast::Primitive& p) {
	return opts.colorize(std::string(p.isSigned ? "i" : "u") + std::to_string(p.length), FormatOptions::Highlight::Primitive);
}

static inline std::string typeRefKindToString(const FormatOptions& opts, const int n, const Ast::Collection& c) {
	return opts.formatNewlineIndentDelimit(n, typeRef(opts, n + 1, *c.elementType), '[', ']');
}

static inline std::string typeRefKindToString(const FormatOptions& opts, const int n, const std::string& p) {
	return opts.colorize(p, FormatOptions::Highlight::TypeRef);
}

static inline std::string typeRef(const FormatOptions& opts, const int n, const Ast::TypeRef& t) {
	return std::visit([n, &opts](const auto& x){ return typeRefKindToString(opts, n, x); }, t);
}

template<class C>
static inline std::string typeDefKindToString(const FormatOptions& opts, const int n, const C& v) {
	return typeRefKindToString(opts, n, v);
}

static inline std::string typeDefKindToString(const FormatOptions& opts, const int n, const Ast::Aggregate& c) {
	return opts.formatNewlineIndentDelimit(n, list(opts, n + 1, c.members, [](const FormatOptions& opts, const int n, const Ast::Var& v){
		return memberItem(opts, n, v, FormatOptions::Highlight::Member);}
	), '{', '}');
}

static inline std::string typeDef(const FormatOptions& opts, const int n, const Ast::TypeDef& t) {
	return std::visit([n, &opts](const auto& x){ return typeDefKindToString(opts, n, x); }, t);
}

static inline std::string argumentList(const FormatOptions& opts, const int n, const std::vector<Ast::Var> &args)
{
	return opts.formatNewlineIndentDelimit(n, list(opts, n + 1, args, [](const FormatOptions& opts, const int n, const Ast::Var& v){
		return memberItem(opts, n, v, FormatOptions::Highlight::Argument);}
	), '(', ')');
}

static inline std::string signature(const FormatOptions& opts, const int n, const Ast::Action& s) {
	return opts.colorize(s.name, FormatOptions::Highlight::Function) + argumentList(opts, n, s.args);
}

static inline std::string formatItem(const FormatOptions& opts, const int n, const Ast::Action& s) {
	return opts.indent(n) + signature(opts, n, s) + ";";
}

static inline std::string formatItem(const FormatOptions& opts, const int n, const Ast::Function& s)
{
	return opts.indent(n)
		+ signature(opts, n, s)
		+ ((s.returnType.has_value()) ? (std::string(": ") + typeRef(opts, n + 1, s.returnType.value())) : std::string{}) + ";";
}

static inline std::string formatItem(const FormatOptions& opts, const int n, const Ast::Alias& s) {
	return opts.indent(n) + opts.colorize(s.name, FormatOptions::Highlight::TypeDef) + " = " + typeDef(opts, n + 1, s.type) + ";";
}

static inline std::string formatSessionItem(const FormatOptions& opts, const int n, const Ast::Session::ForwardCall& s) {
	return "!" + signature(opts, n, s);
}

static inline std::string formatSessionItem(const FormatOptions& opts, const int n, const Ast::Session::CallBack& s) {
	return "@" + signature(opts, n, s);
}

static inline std::string formatSessionItem(const FormatOptions& opts, const int n, const Ast::Session::Ctor& s) {
	return signature(opts, n, s);
}

static inline std::string formatItem(const FormatOptions& opts, const int n, const Ast::Session& s)
{
	return opts.indent(n) + opts.colorize(s.name, FormatOptions::Highlight::Session)
			+ "\n" + opts.indent(n) + "<\n"
			+ std::accumulate(s.items.begin(), s.items.end(), std::string{}, [n, &opts](const std::string a, const auto &i){
				return a + opts.indent(n + 1) + formatComent(opts, n + 1, i.first)
						+ std::visit([&opts, n](auto& v) {return formatSessionItem(opts, n + 1, v);}, i.second) + ";\n";
			})
			+ opts.indent(n) + ">;";
}

std::string format(const FormatOptions& opts, const Ast& ast)
{
	std::stringstream ss;

	std::transform(ast.items.begin(), ast.items.end(), std::ostream_iterator<std::string>(ss, "\n"), [&opts](const auto& s)
	{
		return formatComent(opts, 0, s.first) + std::visit([&opts](const auto &s){return formatItem(opts, 0, s); }, s.second);
	});

	return ss.str();
}
