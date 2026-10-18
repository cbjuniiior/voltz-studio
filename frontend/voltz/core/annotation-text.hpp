#pragma once

#include <cstddef>
#include <string>

namespace voltz {

// Texto das anotações (Plano 4D): UTF-8, linhas separadas por '\n'. Limites do que se digita no modo desenho e do que
// se lê do anotacoes.jsonl.
constexpr size_t kMaxTextChars = 280; // caracteres (códigos Unicode), contando os '\n'
constexpr size_t kMaxTextLines = 12;
constexpr char32_t kReplacementChar = 0xFFFD;

// UTF-8 → códigos. Sequência inválida (byte solto, curta, longa demais, substituto, > U+10FFFF) vira U+FFFD.
std::u32string decodeUtf8(const std::string &text);
// Códigos → UTF-8. Código inválido (substituto, > U+10FFFF) vira U+FFFD.
std::string encodeUtf8(const std::u32string &text);

// Caractere aceito no texto: '\n' ou imprimível (não é controle C0/C1 nem DEL, nem substituto, nem > U+10FFFF).
bool isTextCodepoint(char32_t codepoint);
// Caracteres (códigos) do texto em UTF-8.
size_t textCharCount(const std::string &text);
// Nada a mostrar: vazio ou só espaços (U+0020, U+00A0) e quebras de linha. Texto assim confirmado ou cancelado sai sem
// registro (não vai ao plugin nem ao anotacoes.jsonl).
bool textIsBlank(const std::string &text);
// Acrescenta `codepoint` no fim se for aceito e couber (kMaxTextChars caracteres, kMaxTextLines linhas). Falso = nada
// mudou.
bool appendTextCodepoint(std::string &text, char32_t codepoint);
// Tira o último caractere. Falso se o texto está vazio.
bool popTextCodepoint(std::string &text);
// Texto vindo de fora (anotacoes.jsonl, pedido de teste): UTF-8 válido, "\r\n" e "\r" viram '\n', sem os outros
// controles, cortado em kMaxTextChars caracteres e kMaxTextLines linhas.
std::string sanitizeAnnotationText(const std::string &text);

} // namespace voltz
