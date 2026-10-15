# Voltz Studio — Base + Gravação em projeto (design)

> Etapas 1 e 2 de 5. Data: 2026-10-15. Base: fork de OBS Studio 32.2.2 (`cbjuniiior/voltz-studio`, branch `voltz`).

## 1. Contexto e objetivo

Voltz Studio é um app **Windows x64 instalável**, focado em **gravar conteúdo 16:9** (tutoriais, aulas, vídeos
para YouTube), construído como fork do OBS Studio. A visão completa tem 5 etapas:

| # | Etapa | Tecnologia |
|---|-------|-----------|
| 1 | **Base** — marca, interface simplificada, presets, instalador | C++/Qt (este doc) |
| 2 | **Gravação em projeto** — faixas separadas, cursor, marcadores, anotações ao vivo | C++/Qt (este doc) |
| 3 | Editor — cortes, layout tela+câmera, zoom automático editável, exportação | UI web (React) + FFmpeg |
| 4 | Câmera com IA — remover/desfocar fundo, enquadrar rosto (ONNX/DirectML) | C++ (plugin) |
| 5 | Legendas — transcrição local (Whisper), .srt | C++ / editor |

Este documento cobre **apenas as etapas 1 e 2**. O resultado é um `VoltzStudio-Setup-1.0.0.exe` que instala um
gravador com a cara Voltz e que produz **projetos** prontos para o editor da etapa 3.

**Critério de sucesso:** instalar num Windows limpo, abrir, escolher um preset, gravar com troca de cena,
marcador, pausa, desenho na tela e destaque do mouse, e obter uma pasta de projeto completa e consistente —
convivendo com um OBS Studio instalado na mesma máquina.

## 2. Decisões (e o porquê)

| Decisão | Escolha | Por quê |
|--------|---------|---------|
| Base técnica | Fork do OBS 32.2.2 (última estável) | Motor de captura/encode maduro; 33 ainda em RC |
| Arquitetura de UI | **Casca Voltz nova** em Qt; `OBSBasic` vira motor oculto | Liberdade visual total, pouco conflito em merges do upstream, "modo clássico" como saída de emergência |
| Linguagem | C/C++ + Qt 6 (gravador); web só no editor (etapa 3) | Desempenho onde importa; web onde a UI é nova |
| Modelo de gravação | **Cenas mantidas, simplificadas** | Pedido explícito |
| Layout | **H2**: barra lateral + Cenas · Prévia · Fontes/Áudio + barra inferior | Escolhido no mockup |
| Tema | **Preto + lima elétrico** (#09090B / #D4F53C) | Escolhido no mockup |
| Lives | **Ocultas, mas mantidas** (ligáveis em Ajustes → Avançado) | Foco em gravação sem perder a opção |
| Anotações | **Ao vivo + editáveis** (vetores salvos; tela crua limpa) | Ver na hora e corrigir no editor |
| Instalador | **Inno Setup** (não NSIS) | Repo do OBS não tem script de instalador; Inno tem PT-BR nativo e script mais simples |
| Toolchain | VS 2022 Build Tools + preset local `voltz-x64` (gerador VS 17 2022) | Preset upstream exige VS 2026, mas o código só exige Windows SDK ≥ 10.0.20348 |

## 3. Arquitetura

```
VoltzStudio.exe
├── Casca Voltz (frontend/voltz/shell/)         janela Qt nova, sem bordas nativas, tema lima
│   ├── SideNav                                 Gravar · Projetos · Presets · Ajustes
│   ├── RecordPage (H2)                         Cenas | Prévia | Fontes + Áudio | ⚑ ● ⏸
│   ├── ProjectsPage                            lista de projetos gravados
│   ├── PresetsPage                             4 presets de fábrica + "aplicar"
│   └── SettingsPage                            essencial + Mouse + Anotações + Avançado
├── VoltzController (frontend/voltz/core/)      ponte casca ⇄ OBS: comandos + eventos do frontend API
├── ProjectRecorder (frontend/voltz/project/)
│   ├── ProgramOutput                           saída principal (MP4 híbrido, 3 faixas de áudio, capítulos)
│   ├── IsoOutputs                              um obs_canvas por fonte papel "Tela"/"Câmera" → MKV
│   ├── CursorTracker                           hook de mouse + amostragem 60 Hz → cursor.jsonl
│   ├── MarkerService                           botão/atalho → capítulo MP4 + marcadores.json
│   └── ProjectManifest                         projeto.voltz (JSON), escrito no início e no fim
├── Annotations (frontend/voltz/annotate/)
│   ├── OverlayWindow                           janela transparente topmost, WDA_EXCLUDEFROMCAPTURE
│   ├── Toolbar                                 caneta, marca-texto, linha, seta, retângulo, elipse, texto
│   └── StrokeStore                             traços vetoriais → anotacoes.jsonl
├── Fontes Voltz (plugins/voltz-sources/)       "Voltz Anotações" e "Voltz Mouse" (fontes OBS que renderizam
│                                                 traços e destaque a partir dos dados ao vivo)
└── OBSBasic (escondida)                        cenas, fontes, config, hotkeys; visível só no "modo clássico"
```

**Princípios**
- A casca **só exibe e comanda**; o estado vive no OBS. Mudanças feitas no modo clássico refletem na casca
  (via `OBS_FRONTEND_EVENT_*` e sinais de cena/fonte) e vice-versa.
- `ProjectRecorder` não conhece a casca: recebe `start(projectName)` / `pause()` / `stop()` / `addMarker(name)`
  e emite estados. O editor (etapa 3) depende apenas dos arquivos que ele produz.
- Lógica pura (manifesto, coordenadas, tempos, fallback de encoder, serialização de traços) fica em
  `frontend/voltz/core/` **sem dependência de Qt**, para ser testável isoladamente.
- Modificações em arquivos upstream ficam restritas a: inicialização (`obs-main.cpp`, `OBSApp.cpp`), marca
  (bootstrap/rc/ícones), caminho de config (centralizado) e CMake. Todo o resto é código novo.

## 4. Interface

### 4.1 Tokens do tema (preto + lima)
| Token | Valor | Uso |
|------|-------|-----|
| `bg` | `#09090B` | fundo da janela |
| `panel` | `#111114` | painéis, barra lateral, barras |
| `line` | `#1F1F24` | bordas e divisores |
| `text` | `#F2F2F3` | texto principal |
| `muted` | `#82828C` | texto secundário, ícones inativos |
| `accent` | `#D4F53C` | seleção, foco, medidores, ícone ativo |
| `accent-bg` | `#D4F53C1A` | fundo de item selecionado |
| `rec` | `#FF4D4D` | botão Gravar, estado gravando |

Fonte: **DM Sans** (empacotada com o app). As janelas OBS que continuam aparecendo (propriedades de fonte,
filtros, modo clássico) recebem um **tema OBS `.ovt` "Voltz"** com a mesma paleta.

### 4.2 Tela Gravar (H2)
- **Topo:** título "Gravar" + seletor de projeto ("Aula 03 ▾", define o nome da pasta).
- **Coluna esquerda — Cenas:** lista com seleção, "+", renomear, duplicar, excluir, reordenar por arrastar.
- **Centro — Prévia** 16:9 (`OBSQTDisplay`), com faixa de aviso para câmera desconectada / disco baixo.
- **Coluna direita — Fontes** da cena atual (visibilidade, travar, ordem, "+" com tipos essenciais: Tela,
  Janela, Câmera, Imagem, Texto, Mídia; "Mais…" abre o seletor completo do OBS) e **Áudio** (Mic e Sistema com
  medidor, mudo e volume; "Avançado" abre o mixer do OBS).
- **Barra inferior:** ⚑ Marcador · ● Gravar (vira ■ Parar + cronômetro) · ⏸ Pausa · estimativa de espaço
  ("~1,1 GB/min · 42 GB livres").

### 4.3 Demais telas
- **Projetos:** lista (nome, data, duração, tamanho, status `ok`/`recuperável`/`falhou`), abrir pasta,
  renomear, excluir, "Recuperar gravação".
- **Presets:** os 4 presets de §4.4 (cartões com miniatura) e "Aplicar" (cria/atualiza as cenas sem apagar cenas do
  usuário).
- **Ajustes:** Gravação (pasta, qualidade, "gravar só a versão montada"), Fontes de gravação (qual monitor,
  qual câmera, qual microfone), Mouse, Anotações, Atalhos, Avançado (live, modo clássico, abrir pasta de logs).

### 4.4 Presets de fábrica (1920×1080 @ 60)
4 presets, que criam 5 cenas ("Só tela / Só câmera" é um preset com duas cenas). Todos usam as **fontes compartilhadas** `Tela Voltz` (grupo) e `Câmera`:
1. **Tutorial** — Tela Voltz cheia + câmera circular no canto inferior direito (máscara, borda lima opcional).
2. **Só tela** — Tela Voltz cheia.
3. **Só câmera** — câmera cheia (talking head).
4. **Lado a lado** — Tela Voltz ~70% à esquerda + câmera à direita sobre fundo da marca.
5. **Abertura / Pausa** — fundo da marca + logo + texto editável ("Já volto", "Aula 03").

`Tela Voltz` é um **grupo** com três camadas: captura de tela (`monitor_capture`/`window_capture`),
`Voltz Anotações` e `Voltz Mouse`, ambas com o mesmo tamanho da captura.

## 5. Gravação em projeto

### 5.1 Pasta e arquivos
```
%USERPROFILE%\Videos\Voltz Studio\2026-10-15 15h30 · Aula 03\
├── projeto.voltz      manifesto JSON
├── programa.mp4       versão montada (MP4 híbrido) · áudio: 1=mix, 2=mic, 3=sistema · capítulos
├── tela.mkv           captura crua, resolução/fps nativos, SEM anotações/mouse Voltz
├── camera.mkv         câmera crua, resolução nativa
├── cursor.jsonl       eventos de mouse (coords de tela.mkv)
├── anotacoes.jsonl    traços vetoriais
└── marcadores.json    [{ "t_ms": 12345, "nome": "Marcador 1" }]
```

### 5.2 `projeto.voltz` (versão 1)
```json
{
  "formato": "voltz-projeto", "versao": 1,
  "app": "Voltz Studio 1.0.0", "motor": "libobs 32.2.2",
  "nome": "Aula 03", "criado_em": "2026-10-15T15:30:12-03:00",
  "status": "gravando | ok | recuperado | falhou",
  "relogio": "obs_ns", "t0_ns": 123456789000,
  "duracao_ms": 0,
  "pausas": [{ "inicio_ms": 61000, "fim_ms": 75000 }],
  "arquivos": {
    "programa": { "arquivo": "programa.mp4", "largura": 1920, "altura": 1080, "fps": 60, "offset_ms": 0 },
    "tela":     { "arquivo": "tela.mkv", "largura": 2560, "altura": 1440, "fps": 60, "offset_ms": 3,
                  "origem": { "tipo": "monitor", "id": "\\\\.\\DISPLAY1", "retangulo": [0,0,2560,1440], "escala": 1.25 } },
    "camera":   { "arquivo": "camera.mkv", "largura": 1920, "altura": 1080, "fps": 30, "offset_ms": 5 }
  },
  "mouse": {
    "halo":     { "ativo": true, "cor": "#D4F53C", "tamanho": 46, "opacidade": 0.25, "forma": "cheio" },
    "clique":   { "ativo": true, "cor": "#D4F53C", "cor_direito": "#4DA3FF" },
    "holofote": { "ativo": false, "raio": 220, "escurecer": 0.55 }
  },
  "eventos": [{ "t_ms": 90500, "tipo": "camera_desconectada" }, { "t_ms": 0, "tipo": "iso_falhou", "faixa": "camera" }]
}
```
O manifesto é criado no **início** com `status: "gravando"` e reescrito (escrita atômica: arquivo temporário +
rename) no **fim** e a cada marcador.

### 5.3 Saídas
- **Programa:** saída de gravação padrão do OBS, formato MP4 híbrido, 3 faixas de áudio (mix/mic/sistema).
  Marcadores viram capítulos via `mp4_mux_add_chapter`.
- **ISO:** para cada fonte com papel `Tela` e `Câmera`, um `obs_canvas` próprio na resolução nativa da fonte,
  renderizando **só a fonte de captura** (sem as camadas Voltz), com encoder e `ffmpeg_muxer` → MKV.
  Trocar de cena não afeta os ISO (as fontes são compartilhadas).
- **Pausa** aplica-se a todas as saídas juntas; os intervalos ficam em `pausas`.

### 5.4 Encoders
Ordem de preferência por saída: **AMD AMF → NVIDIA NVENC → Intel QSV → x264**. Programa e ISO em HEVC se
disponível, senão H.264; controle por qualidade (CQP), ISO com qualidade maior que o programa. Se a GPU não
sustentar 3 sessões, os ISO são desligados com aviso ("gravando só a versão montada") e registro em `eventos`.

### 5.5 Sincronia
Tudo usa o relógio do libobs (`os_gettime_ns`). `t0_ns` = início da saída de programa; cada arquivo registra
`offset_ms` do primeiro quadro. Eventos de cursor, traços e marcadores são gravados em ms relativos a `t0_ns`
(sem descontar pausas; o editor aplica `pausas`).

### 5.6 `cursor.jsonl`
Uma linha por evento: `{"t":1234,"x":812,"y":440,"e":"move"}`, `e ∈ move|down|up|scroll`, com `b`
(`l|r|m`) em cliques e `d` em scroll. `move` amostrado a 60 Hz só quando há movimento. Coordenadas convertidas
de desktop (DPI-aware, multi-monitor) para pixels de `tela.mkv`; em captura de janela, relativas ao retângulo
da janela, com evento `{"e":"janela","r":[x,y,w,h]}` quando ela move/redimensiona. Fora da área capturada:
evento `{"e":"fora"}`.

**Teclas (opcional, desligado por padrão):** apenas combinações com Ctrl/Alt/Win (`{"e":"atalho","k":"Ctrl+Shift+P"}`).
Texto digitado nunca é registrado.

## 6. Anotações ao vivo e destaque do mouse

### 6.1 Destaque do mouse (Ajustes → Mouse, com prévia ao vivo)
- **Halo:** liga/desliga, cor, tamanho, opacidade, forma (círculo cheio | anel).
- **Clique:** onda liga/desliga, cor, cor separada para botão direito.
- **Holofote:** liga/desliga, raio, escurecimento; atalho próprio para alternar durante a gravação.
- **Estilos salvos:** salvar/aplicar combinações nomeadas.

Renderizado pela fonte `Voltz Mouse` (dentro do grupo Tela Voltz) a partir da posição/cliques ao vivo; a
configuração usada vai para `projeto.voltz → mouse` (o editor pode trocar depois).

### 6.2 Desenho na tela
- **Atalho global** (padrão `Ctrl+Shift+D`) abre a barra flutuante e ativa o modo desenho; **Esc** sai.
- Ferramentas: caneta, marca-texto, linha, seta, retângulo, elipse, texto. 4 cores rápidas + seletor, espessura,
  desfazer, limpar tudo; opção "traços somem após N s".
- **OverlayWindow:** janela transparente topmost por monitor, `SetWindowDisplayAffinity(WDA_EXCLUDEFROMCAPTURE)`
  → o usuário vê, a captura não. Clicável apenas em modo desenho (fora dele, transparente a cliques).
- **StrokeStore** grava cada traço em `anotacoes.jsonl`:
  `{"id":7,"ferr":"seta","cor":"#D4F53C","esp":4,"pts":[[x,y],...],"t_ini":1200,"t_fim":1850,"t_limpo":9000}`
  (coordenadas de `tela.mkv`). A fonte `Voltz Anotações` desenha os mesmos traços no programa.

### 6.3 Requisito
`WDA_EXCLUDEFROMCAPTURE` exige **Windows 10 2004+**. Em versões anteriores, a camada aparece na captura: o app
avisa e marca `"anotacoes_queimadas": true` no manifesto.

## 7. Marca, convivência e o que sai do OBS

### 7.1 Identidade
- Produto **Voltz Studio**, executável **`VoltzStudio.exe`**, empresa **VoltzPixel**.
- Versão do produto **1.0.0** separada da versão do motor (**libobs 32.2.2**, mantida para compatibilidade
  com plugins). "Sobre": *Voltz Studio 1.0.0 · baseado no OBS Studio 32.2.2 (GPL-2.0)* + créditos/licença.
- **Logo — obturador de 3 lâminas:** triângulo de cantos arredondados em lima `#D4F53C`, dividido em 3 lâminas
  em catavento que convergem no centro, inclinado −14°, sobre bloco squircle preto `#0C0C0F` com borda sutil
  `#2C2C34`. Versão simplificada (cortes mais grossos) abaixo de 32 px.
  - **Estados (bandeja/barra de tarefas):** parado = obturador puro; **gravando = bolinha `#FF3B3B` no canto
    superior direito**; pausado = bolinha cinza `#8F8F98`.
  - Entregáveis: SVG mestre, `.ico` (16/24/32/48/64/256), PNGs de bandeja ×3 estados, versão monocromática.
  - Referência visual: `.superpowers/brainstorm/voltz-base/content/logo-v6.html` (variação **P**).
  - Diferenciação do logo do Google Drive (semelhança apontada na exploração): monocromático, inclinado,
    lâminas convergindo no centro (sem triângulo vazado).

### 7.2 Convivência com OBS instalado
- Config em `%APPDATA%\Voltz Studio` — **centralizar** os ~45 literais `"obs-studio/..."` do frontend numa
  única função/constante (`App()->ConfigDir()`), inclusive no modo portátil.
- Mutexes próprios (`VoltzStudioCore`, `VoltzStudioPortable…`).
- Câmera virtual **"Voltz Virtual Camera"** com **GUID novo** e memória compartilhada própria
  (`VoltzVirtualCamVideo`); `virtualcam-filter.cpp` passa a reconhecer `VoltzStudio.exe`.
- Hook de captura de jogos (`ProgramData\obs-studio-hook`): mantido (interno, não conflita).

### 7.3 Desligado / redirecionado
- **Atualizador do OBS:** não compilado/instalado; sem auto-update na v1.
- **What's New** (`ENABLE_WHATSNEW=OFF`), busca do "Sobre" (Patreon), **envio de log e de crash** → substituídos
  por "Abrir pasta de logs".
- **Listas remotas:** `ENABLE_SERVICE_UPDATES=OFF`, `ENABLE_COMPAT_UPDATES=OFF`.
- **OAuth** Twitch/YouTube/Restream: continuam fora (sem client IDs).
- **Links de ajuda** → GitHub do Voltz. **Assistente de primeira execução** do OBS → substituído pela tela de
  Presets na primeira abertura.
- **Fora do build:** `ENABLE_AJA=OFF`, `ENABLE_DECKLINK=OFF`.
- **Idiomas:** só `pt-BR` e `en-US` instalados; strings com "OBS" reescritas nesses dois; strings da casca em
  arquivo próprio (`frontend/voltz/data/locale/*.ini`).

### 7.4 Instalador — `VoltzStudio-Setup-1.0.0.exe` (Inno Setup)
- PT-BR + EN; página de licença GPL-2.0.
- Instala em `Program Files\Voltz Studio` (admin — registro da câmera virtual).
- Atalhos: menu Iniciar; área de trabalho opcional.
- Instala **VC++ Redistributable x64** se ausente.
- Registra/desregistra a câmera virtual (`regsvr32` dos módulos `voltz-virtualcam-module32/64`).
- Desinstalador pergunta se apaga `%APPDATA%\Voltz Studio`.
- Sem assinatura de código na v1 (SmartScreen mostrará "editor desconhecido"); script preparado para assinar
  quando houver certificado.
- Script em `installer/voltz-studio.iss`, alimentado pelo `cmake --install` em `build_x64/install`.

### 7.5 GPL
Fork público `cbjuniiior/voltz-studio` disponibiliza o código. Licença e créditos no instalador e no "Sobre".

## 8. Erros e recuperação

| Situação | Comportamento |
|---|---|
| Disco < 5 GB | Faixa de aviso na prévia |
| Disco < 1 GB | Para a gravação de forma limpa (finaliza arquivos e manifesto) e avisa |
| Crash / app fechado gravando | Na próxima abertura, projeto com `status: gravando` → "Recuperar": fecha manifesto, calcula duração pelos arquivos, `status: recuperado` |
| Encoder indisponível | Cadeia de fallback (§5.4); sem GPU para 3 sessões → só versão montada + aviso |
| ISO falha durante gravação | Programa continua; `eventos` registra falha e instante |
| Câmera desconectada | Faixa de aviso; evento no manifesto; gravação continua |
| Windows < 10 2004 | Anotações/mouse queimados na tela crua + aviso (§6.3) |
| Falha ao gravar manifesto | Tenta de novo; se persistir, avisa e mantém cópia em `%APPDATA%\Voltz Studio\recovery\` |

## 9. Testes

- **Unitários (doctest + CTest)**, em `frontend/voltz/tests/`, sobre `frontend/voltz/core/`:
  manifesto (ler/escrever/atomicidade/versão), conversão de coordenadas (multi-monitor, DPI 100/125/150%,
  captura de janela), alinhamento de tempos e pausas, cadeia de fallback de encoder, serialização de traços.
- **Integração (script PowerShell):** inicia `VoltzStudio.exe --portable`, grava 10 s com troca de cena,
  marcador e pausa (via obs-websocket), fecha e valida com `ffprobe`: durações dentro de 1 quadro, 3 faixas de
  áudio, capítulos no MP4, `cursor.jsonl` dentro dos limites, manifesto `status: ok`.
- **Instalador:** roteiro no **Windows Sandbox** — instalar, abrir, gravar, conviver com OBS instalado,
  desinstalar sem resíduos (exceto config, se mantida).
- **CI:** desativar workflows upstream (dependem de segredos do OBS); workflow próprio Windows: build + testes
  unitários + artefato do instalador.
- **Verificação visual:** capturas da casca nas 4 telas comparadas aos mockups aprovados.

## 10. Fora de escopo (etapas futuras)
Editor e exportação composta (3), zoom automático (3), câmera com IA (4), transcrição/legendas (5),
auto-update próprio, assinatura de código, macOS/Linux, gravação vertical/dual-canvas, novos idiomas.

## 11. Riscos e pontos a validar cedo
1. **Esconder `OBSBasic` sem quebrar** — ela inicializa grande parte do app (cenas, docks, hotkeys). Validar
   primeiro um protótipo mínimo: casca com prévia + trocar cena + gravar, com `OBSBasic` oculta.
2. **ISO via `obs_canvas`** — confirmar que um canvas privado renderizando uma única fonte com encoder próprio
   funciona em 32.2.2 sem afetar a saída principal; medir carga com 3 encodes AMF.
3. **Fontes `Voltz Anotações`/`Voltz Mouse` em grupo** — garantir que o ISO capture só a fonte de captura e o
   programa capture o grupo inteiro.
4. **Merges futuros do upstream** — manter mudanças em arquivos do OBS mínimas e documentadas
   (`docs/voltz/upstream-patches.md`).
