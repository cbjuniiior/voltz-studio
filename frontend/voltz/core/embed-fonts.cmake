# Gera o .cpp com os bytes das duas fontes do texto das anotações (DM Sans Regular e Bold, OFL), embutidas no voltz-core:
# a sobreposição (obs-studio) e a fonte Voltz Anotações (plugin) desenham o texto com os mesmos bytes.
# Uso (add_custom_command do CMakeLists.txt): cmake -DOUTPUT=<.cpp> -DREGULAR=<.ttf> -DBOLD=<.ttf> -P embed-fonts.cmake
cmake_minimum_required(VERSION 3.28)

foreach(required IN ITEMS OUTPUT REGULAR BOLD)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "embed-fonts.cmake: falta -D${required}=...")
  endif()
endforeach()

function(voltz_embed_font path symbol out_var)
  file(READ "${path}" hex HEX)
  string(LENGTH "${hex}" length)
  if(length LESS 1024)
    message(FATAL_ERROR "embed-fonts.cmake: fonte vazia ou pequena demais: ${path}")
  endif()
  # 16 bytes por linha
  string(REPEAT "[0-9a-f]" 32 chunk)
  string(REGEX REPLACE "(${chunk})" "\\1\n" hex "${hex}")
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
  set(
    ${out_var}
    "extern const unsigned char ${symbol}[];\nconst unsigned char ${symbol}[] = {\n${bytes}\n};\nextern const size_t ${symbol}Size;\nconst size_t ${symbol}Size = sizeof(${symbol});\n"
    PARENT_SCOPE
  )
endfunction()

voltz_embed_font("${REGULAR}" kDmSansRegular regular)
voltz_embed_font("${BOLD}" kDmSansBold bold)

file(
  WRITE "${OUTPUT}.tmp"
  "// Gerado pelo CMake (frontend/voltz/core/embed-fonts.cmake) a partir de frontend/voltz/shell/fonts. NAO editar.\n#include <cstddef>\n\nnamespace voltz::fontdata {\n\n${regular}\n${bold}\n} // namespace voltz::fontdata\n"
)
file(COPY_FILE "${OUTPUT}.tmp" "${OUTPUT}" ONLY_IF_DIFFERENT)
file(REMOVE "${OUTPUT}.tmp")
