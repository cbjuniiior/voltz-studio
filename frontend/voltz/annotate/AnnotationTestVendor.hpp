#pragma once

#include <string>

namespace voltz {

class AnnotationController;

// Só com VOLTZ_TEST_LIVE: fornecedor "voltz-teste-casca" do obs-websocket, registrado pela casca depois do
// OBSBasic::OBSInit (todos os módulos já carregados; plugins usam o obs_module_post_load pelo mesmo motivo). Dados por
// texto JSON (obs_data não guarda lista de números):
//   InjectStroke {ferr?, cor?, esp?, pts:"[[x,y],...]", aberto?} — começa, estende e (sem "aberto") termina um traço;
//   ExtendStroke {pts}, EndStroke, UndoStroke, ClearStrokes;
//   ProbeOverlay {pts} — guarda os pixels da sobreposição nesses pontos (lidos no próximo AnnotationState);
//   SetAnnotationStyle {estilo:"{...}"} — AnnotationController::setStyle (o que a barra do modo desenho faz);
//   ToolbarImage {queimadas?} → {png (base64), largura, altura, janela_nativa} — uma DrawToolbar NUNCA mostrada,
//   desenhada por QWidget::grab() com o estilo atual e o aviso de "queimadas" se o controlador está queimado ou se
//   "queimadas" vem true (só no desenho; nada muda no controlador) (janela_nativa tem de ser false: nenhuma janela nasce);
//   AnnotationState → {"estado": texto JSON publicado pelo AnnotationController (republicado na thread da UI antes de
//   responder, até 2 s), "ok"}.
// O callback roda na thread do websocket, sem lock do obs-websocket: os pedidos que mudam algo são ENFILEIRADOS para a
// thread da UI e respondem na hora (quem testa espera ~400 ms antes de ler o estado). Nada mexe no mouse nem no teclado.
class AnnotationTestVendor {
public:
	// Thread da UI. Registra o fornecedor (uma vez) e aponta os pedidos para `controller`.
	static void start(AnnotationController *controller);
	// EXIT (thread da UI): solta o controlador (pedidos em andamento viram no-op) e tira os pedidos do fornecedor.
	static void stop();
	// Thread da UI: estado devolvido por AnnotationState.
	static void publish(const std::string &stateJson);
};

} // namespace voltz
