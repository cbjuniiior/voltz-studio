#pragma once

#include "cursor-track.hpp"

#include <cstdlib>

namespace voltz::live {

// Canal frontend → plugin voltz-sources: procedimentos no proc handler global do libobs (obs_get_proc_handler()),
// registrados pelo plugin no obs_module_load. Rodam na thread de quem chama; o plugin guarda tudo num estado com mutex.
// Coordenadas em pixels da captura alvo (as mesmas do cursor.jsonl).
constexpr const char *kMouseSourceId = "voltz_mouse_source";
// Ajuste da fonte Voltz Mouse: UUID da captura que ela cobre (dá o tamanho da fonte).
constexpr const char *kTargetSetting = "alvo";

constexpr const char *kProcPosition = "voltz_mouse_position";
constexpr const char *kDeclPosition = "void voltz_mouse_position(in int x, in int y, in bool inside)";
constexpr const char *kProcClick = "voltz_mouse_click";
constexpr const char *kDeclClick = "void voltz_mouse_click(in int x, in int y, in int button, in int t_ns)";
constexpr const char *kProcStyle = "voltz_mouse_style";
constexpr const char *kDeclStyle = "void voltz_mouse_style(in string json)";
constexpr const char *kProcWanted = "voltz_mouse_wanted";
constexpr const char *kDeclWanted = "void voltz_mouse_wanted(out int count)";

constexpr int kButtonLeft = 1;
constexpr int kButtonRight = 2;
constexpr int kButtonMiddle = 3;

inline int buttonCode(MouseButton button)
{
	switch (button) {
	case MouseButton::Right:
		return kButtonRight;
	case MouseButton::Middle:
		return kButtonMiddle;
	case MouseButton::Left:
	case MouseButton::None:
		break;
	}
	return kButtonLeft;
}

inline MouseButton buttonFromCode(int code)
{
	if (code == kButtonRight) {
		return MouseButton::Right;
	}
	if (code == kButtonMiddle) {
		return MouseButton::Middle;
	}
	return MouseButton::Left;
}

// Anotações (Plano 4B): fonte Voltz Anotações e procedimentos frontend → plugin. `json` = strokeToJsonLine (vale o `id`
// do procedimento); `points` = pointsToJson, acrescentados com appendStrokePoints (a mesma regra do StrokeStore).
// `burned` = anotações "queimadas" na captura (spec §6.3): a fonte não desenha, para não duplicar.
constexpr const char *kAnnotationsSourceId = "voltz_annotations_source";
constexpr const char *kProcAnnotBegin = "voltz_annot_begin";
constexpr const char *kDeclAnnotBegin = "void voltz_annot_begin(in int id, in string json)";
constexpr const char *kProcAnnotExtend = "voltz_annot_extend";
constexpr const char *kDeclAnnotExtend = "void voltz_annot_extend(in int id, in string points)";
constexpr const char *kProcAnnotRemove = "voltz_annot_remove";
constexpr const char *kDeclAnnotRemove = "void voltz_annot_remove(in int id)";
constexpr const char *kProcAnnotClear = "voltz_annot_clear";
constexpr const char *kDeclAnnotClear = "void voltz_annot_clear()";
constexpr const char *kProcAnnotBurned = "voltz_annot_burned";
constexpr const char *kDeclAnnotBurned = "void voltz_annot_burned(in bool burned)";

// Só testes: com VOLTZ_TEST_LIVE o plugin registra o fornecedor "voltz-teste" no obs-websocket (InjectMouse,
// MouseState, SetStyle, AddMouseSource e, no 4B, AnnotationFeedState, AddAnnotationSource, FeedStroke, FeedClear), o
// frontend registra "voltz-teste-casca" (traços e estado das anotações) e o mouse real não é lido. As variáveis abaixo
// só valem junto com VOLTZ_TEST_LIVE: retângulo da sobreposição "x,y,largura,altura" (pixels físicos) no lugar do
// monitor da Tela; anotações "queimadas" forçadas (como num Windows < 10 2004).
constexpr const char *kTestEnv = "VOLTZ_TEST_LIVE";
constexpr const char *kTestVendor = "voltz-teste";
constexpr const char *kTestVendorShell = "voltz-teste-casca";
constexpr const char *kTestOverlayRectEnv = "VOLTZ_TEST_OVERLAY_RECT";
constexpr const char *kTestBurnedEnv = "VOLTZ_TEST_ANNOT_BURNED";

inline bool testModeEnabled()
{
	const char *value = std::getenv(kTestEnv);
	return value && *value;
}

} // namespace voltz::live
