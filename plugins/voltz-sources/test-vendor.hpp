#pragma once

namespace voltz {

// Só com VOLTZ_TEST_LIVE (chamar no obs_module_post_load): fornecedor "voltz-teste" do obs-websocket com
// InjectMouse {x,y,inside?,click?: "l"|"r"|"m"}, MouseState (x, y, inside, ripples, showing, style), SetStyle {style}
// e AddMouseSource {sceneName, targetName, name?} (o CreateInput do websocket recusa fonte OBS_SOURCE_CAP_DISABLED).
// Permite testar o desenho sem mexer no mouse do usuário.
void registerTestVendor();

} // namespace voltz
