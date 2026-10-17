#pragma once

namespace voltz {

// Só com VOLTZ_TEST_LIVE (chamar no obs_module_post_load): fornecedor "voltz-teste" do obs-websocket.
// Mouse (4A): InjectMouse {x,y,inside?,click?: "l"|"r"|"m"}, MouseState (x, y, inside, ripples, showing, style),
// SetStyle {style}, AddMouseSource {sceneName, targetName, name?}.
// Anotações (4B): AnnotationFeedState ("estado": texto JSON do AnnotationFeed), AddAnnotationSource {sceneName,
// targetName, name?}, FeedStroke {id, json} e FeedClear (alimentam o feed direto, sem o frontend).
// O CreateInput do websocket recusa fonte OBS_SOURCE_CAP_DISABLED, por isso os Add*. Nada mexe no mouse do usuário.
void registerTestVendor();

} // namespace voltz
