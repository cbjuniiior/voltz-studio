#include "annotation-text.hpp"

#include <algorithm>
#include <cstdint>

namespace voltz {

namespace {

bool validCodepoint(char32_t codepoint)
{
	return codepoint <= 0x10FFFF && !(codepoint >= 0xD800 && codepoint <= 0xDFFF);
}

void appendEncoded(std::string &out, char32_t codepoint)
{
	if (!validCodepoint(codepoint)) {
		codepoint = kReplacementChar;
	}
	if (codepoint < 0x80) {
		out.push_back(static_cast<char>(codepoint));
	} else if (codepoint < 0x800) {
		out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
		out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
	} else if (codepoint < 0x10000) {
		out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
		out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
	} else {
		out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
		out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
	}
}

size_t lineCount(const std::u32string &text)
{
	return static_cast<size_t>(std::count(text.begin(), text.end(), U'\n')) + 1;
}

} // namespace

std::u32string decodeUtf8(const std::string &text)
{
	std::u32string out;
	out.reserve(text.size());
	size_t i = 0;
	while (i < text.size()) {
		const auto lead = static_cast<uint8_t>(text[i]);
		size_t length = 0;
		char32_t codepoint = 0;
		char32_t minimum = 0;
		if (lead < 0x80) {
			out.push_back(lead);
			i++;
			continue;
		} else if ((lead & 0xE0) == 0xC0) {
			length = 2;
			codepoint = lead & 0x1Fu;
			minimum = 0x80;
		} else if ((lead & 0xF0) == 0xE0) {
			length = 3;
			codepoint = lead & 0x0Fu;
			minimum = 0x800;
		} else if ((lead & 0xF8) == 0xF0) {
			length = 4;
			codepoint = lead & 0x07u;
			minimum = 0x10000;
		} else {
			out.push_back(kReplacementChar); /* byte de continuação solto ou inválido */
			i++;
			continue;
		}
		size_t used = 1;
		bool ok = true;
		for (; used < length; used++) {
			if (i + used >= text.size() || (static_cast<uint8_t>(text[i + used]) & 0xC0) != 0x80) {
				ok = false;
				break;
			}
			codepoint = (codepoint << 6) | (static_cast<uint8_t>(text[i + used]) & 0x3Fu);
		}
		if (!ok || codepoint < minimum || !validCodepoint(codepoint)) {
			out.push_back(kReplacementChar);
			i += ok ? length : used; /* sequência curta: recomeça no byte que quebrou */
			continue;
		}
		out.push_back(codepoint);
		i += length;
	}
	return out;
}

std::string encodeUtf8(const std::u32string &text)
{
	std::string out;
	out.reserve(text.size());
	for (const char32_t codepoint : text) {
		appendEncoded(out, codepoint);
	}
	return out;
}

bool isTextCodepoint(char32_t codepoint)
{
	if (codepoint == U'\n') {
		return true;
	}
	if (codepoint < 0x20 || (codepoint >= 0x7F && codepoint <= 0x9F)) {
		return false;
	}
	return validCodepoint(codepoint);
}

size_t textCharCount(const std::string &text)
{
	return decodeUtf8(text).size();
}

bool appendTextCodepoint(std::string &text, char32_t codepoint)
{
	if (!isTextCodepoint(codepoint)) {
		return false;
	}
	const std::u32string current = decodeUtf8(text);
	if (current.size() >= kMaxTextChars) {
		return false;
	}
	if (codepoint == U'\n' && lineCount(current) >= kMaxTextLines) {
		return false;
	}
	appendEncoded(text, codepoint);
	return true;
}

bool popTextCodepoint(std::string &text)
{
	if (text.empty()) {
		return false;
	}
	std::u32string current = decodeUtf8(text);
	current.pop_back();
	text = encodeUtf8(current);
	return true;
}

bool textIsBlank(const std::string &text)
{
	for (const char32_t codepoint : decodeUtf8(text)) {
		if (codepoint != U' ' && codepoint != U'\n' && codepoint != 0xA0) {
			return false;
		}
	}
	return true;
}

std::string sanitizeAnnotationText(const std::string &text)
{
	const std::u32string input = decodeUtf8(text);
	std::u32string out;
	out.reserve(std::min(input.size(), kMaxTextChars));
	size_t lines = 1;
	for (size_t i = 0; i < input.size() && out.size() < kMaxTextChars; i++) {
		char32_t codepoint = input[i];
		if (codepoint == U'\r') {
			if (i + 1 < input.size() && input[i + 1] == U'\n') {
				continue; /* "\r\n": o '\n' seguinte conta */
			}
			codepoint = U'\n';
		}
		if (!isTextCodepoint(codepoint)) {
			continue;
		}
		if (codepoint == U'\n') {
			if (lines >= kMaxTextLines) {
				break;
			}
			lines++;
		}
		out.push_back(codepoint);
	}
	return encodeUtf8(out);
}

} // namespace voltz
