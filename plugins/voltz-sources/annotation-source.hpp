#pragma once

namespace voltz {

// Registra a fonte "voltz_annotations_source" (Voltz Anotações): tamanho = tamanho da captura do ajuste "alvo" (UUID);
// desenha os traços do AnnotationFeed com o rasterizador do núcleo (o mesmo da sobreposição) numa textura que só é
// refeita quando os traços mudam. Com "queimadas", não desenha. Fica fora do "Adicionar fonte"
// (OBS_SOURCE_CAP_DISABLED): quem cria é o preset/migração do Voltz.
void registerAnnotationsSource();

} // namespace voltz
