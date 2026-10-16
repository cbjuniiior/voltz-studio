#pragma once

#include <string>

namespace voltz {

// Escreve `content` em `utf8Path` de forma atômica: grava "<arquivo>.tmp" ao lado, força para o disco e troca pelo
// destino (MoveFileExW com REPLACE_EXISTING|WRITE_THROUGH no Windows). Em falha, o destino anterior fica intacto,
// o temporário é apagado e `error` (se dado) recebe a causa. Não cria pastas.
bool writeFileAtomically(const std::string &utf8Path, const std::string &content, std::string *error = nullptr);

// Lê o arquivo inteiro (binário). Falso se não abrir; `out` só muda em caso de sucesso.
bool readFileText(const std::string &utf8Path, std::string &out);

} // namespace voltz
