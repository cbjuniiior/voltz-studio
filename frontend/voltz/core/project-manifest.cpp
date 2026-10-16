#include "project-manifest.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <utility>

namespace voltz {

namespace {

using Json = nlohmann::ordered_json;

int64_t getInt(const Json &object, const char *key, int64_t fallback)
{
	const auto it = object.find(key);
	if (it == object.end()) {
		return fallback;
	}
	if (it->is_number_integer()) {
		return it->get<int64_t>();
	}
	if (it->is_number_float()) {
		return static_cast<int64_t>(std::llround(it->get<double>()));
	}
	return fallback;
}

double getDouble(const Json &object, const char *key, double fallback)
{
	const auto it = object.find(key);
	return it != object.end() && it->is_number() ? it->get<double>() : fallback;
}

std::string getString(const Json &object, const char *key)
{
	const auto it = object.find(key);
	return it != object.end() && it->is_string() ? it->get<std::string>() : std::string();
}

// Filho objeto (wantArray = false) ou lista (true); nulo se ausente ou de outro tipo.
const Json *getChild(const Json &object, const char *key, bool wantArray)
{
	const auto it = object.find(key);
	if (it == object.end()) {
		return nullptr;
	}
	if (wantArray ? it->is_array() : it->is_object()) {
		return &*it;
	}
	return nullptr;
}

Json fpsValue(double fps)
{
	const double rounded = std::round(fps * 1000.0) / 1000.0;
	if (std::floor(rounded) == rounded) {
		return Json(static_cast<int64_t>(rounded));
	}
	return Json(rounded);
}

Json originJson(const CaptureOrigin &origin)
{
	Json json = Json::object();
	json["tipo"] = origin.type;
	json["id"] = origin.id;
	json["retangulo"] = Json::array({origin.rect[0], origin.rect[1], origin.rect[2], origin.rect[3]});
	json["escala"] = origin.scale;
	return json;
}

void readOrigin(const Json &json, CaptureOrigin &origin)
{
	origin.type = getString(json, "tipo");
	origin.id = getString(json, "id");
	origin.scale = getDouble(json, "escala", 1.0);
	if (const Json *rect = getChild(json, "retangulo", true)) {
		for (size_t i = 0; i < 4 && i < rect->size(); i++) {
			if ((*rect)[i].is_number()) {
				origin.rect[i] = static_cast<int>((*rect)[i].get<int64_t>());
			}
		}
	}
}

// Objeto `key` dentro de "arquivos", mantendo os campos que esta versão não conhece; cria se ausente/de outro tipo.
Json &fileEntry(Json &files, const char *key)
{
	Json &entry = files[key];
	if (!entry.is_object()) {
		entry = Json::object();
	}
	return entry;
}

// Campos de `item` fora de `known`, como objeto JSON compacto ("" se não houver).
std::string extrasOf(const Json &item, std::initializer_list<const char *> known)
{
	Json extra = Json::object();
	for (auto it = item.begin(); it != item.end(); ++it) {
		const bool isKnown =
			std::any_of(known.begin(), known.end(), [&it](const char *key) { return it.key() == key; });
		if (!isKnown) {
			extra[it.key()] = it.value();
		}
	}
	return extra.empty() ? std::string() : extra.dump(-1, ' ', false, Json::error_handler_t::replace);
}

// Devolve ao item os campos guardados por extrasOf, sem sobrescrever os que o modelo escreveu.
void appendExtras(Json &item, const std::string &extras)
{
	if (extras.empty()) {
		return;
	}
	const Json extra = Json::parse(extras, nullptr, false);
	if (!extra.is_object()) {
		return;
	}
	for (auto it = extra.begin(); it != extra.end(); ++it) {
		if (!item.contains(it.key())) {
			item[it.key()] = it.value();
		}
	}
}

// Funde `fresh` em `target`: objeto com objeto desce um nível; o resto é sobrescrito. Chaves só de `target` ficam.
void mergeInto(Json &target, const Json &fresh)
{
	for (auto it = fresh.begin(); it != fresh.end(); ++it) {
		const auto existing = target.find(it.key());
		if (it->is_object() && existing != target.end() && existing->is_object()) {
			mergeInto(*existing, *it);
		} else {
			target[it.key()] = it.value();
		}
	}
}

void writeIso(Json &files, const char *key, const IsoFile &iso)
{
	if (!iso.present) {
		files.erase(std::string(key));
		return;
	}
	Json &entry = fileEntry(files, key);
	entry["arquivo"] = iso.file;
	entry["largura"] = iso.width;
	entry["altura"] = iso.height;
	entry["fps"] = fpsValue(iso.fps);
	entry["offset_ms"] = iso.offsetMs;
	if (iso.encoder.empty()) {
		entry.erase(std::string("codificador"));
	} else {
		entry["codificador"] = iso.encoder;
	}
	if (iso.hasOrigin) {
		entry["origem"] = originJson(iso.origin);
	} else {
		entry.erase(std::string("origem"));
	}
}

void readIso(const Json &files, const char *key, IsoFile &iso)
{
	const Json *entry = getChild(files, key, false);
	if (!entry) {
		return;
	}
	iso.present = true;
	iso.file = getString(*entry, "arquivo");
	iso.width = static_cast<int>(getInt(*entry, "largura", 0));
	iso.height = static_cast<int>(getInt(*entry, "altura", 0));
	iso.fps = getDouble(*entry, "fps", 0.0);
	iso.offsetMs = getInt(*entry, "offset_ms", 0);
	iso.encoder = getString(*entry, "codificador");
	if (const Json *origin = getChild(*entry, "origem", false)) {
		iso.hasOrigin = true;
		readOrigin(*origin, iso.origin);
	}
}

} // namespace

const char *statusToString(ProjectStatus status)
{
	switch (status) {
	case ProjectStatus::Recording:
		return "gravando";
	case ProjectStatus::Ok:
		return "ok";
	case ProjectStatus::Recovered:
		return "recuperado";
	case ProjectStatus::Failed:
		break;
	}
	return "falhou";
}

ProjectStatus statusFromString(const std::string &text)
{
	if (text == "gravando") {
		return ProjectStatus::Recording;
	}
	if (text == "ok") {
		return ProjectStatus::Ok;
	}
	if (text == "recuperado") {
		return ProjectStatus::Recovered;
	}
	return ProjectStatus::Failed;
}

int64_t manifestNetDurationMs(const ProjectManifest &manifest)
{
	return netDuration(manifest.durationMs, manifest.pauses);
}

std::string serializeManifest(const ProjectManifest &m)
{
	Json root = Json::object();
	if (!m.preservedJson.empty()) {
		Json base = Json::parse(m.preservedJson, nullptr, false);
		if (base.is_object()) {
			root = std::move(base);
		}
	}
	root["formato"] = "voltz-projeto";
	root["versao"] = m.version;
	root["app"] = m.app;
	root["motor"] = m.engine;
	root["nome"] = m.name;
	root["criado_em"] = m.createdAt;
	root["status"] = statusToString(m.status);
	root["relogio"] = "obs_ns";
	root["t0_ns"] = m.t0Ns;
	root["duracao_ms"] = m.durationMs;
	root["duracao_liquida_ms"] = manifestNetDurationMs(m);

	Json pauses = Json::array();
	for (size_t i = 0; i < m.pauses.size(); i++) {
		const PauseSpan &pause = m.pauses[i];
		Json item = Json::object();
		item["inicio_ms"] = pause.startMs;
		item["fim_ms"] = pause.endMs < 0 ? Json(nullptr) : Json(pause.endMs);
		appendExtras(item, i < m.pauseExtras.size() ? m.pauseExtras[i] : std::string());
		pauses.push_back(std::move(item));
	}
	root["pausas"] = std::move(pauses);

	if (!root.contains("arquivos") || !root["arquivos"].is_object()) {
		root["arquivos"] = Json::object();
	}
	Json &files = root["arquivos"];
	if (m.program.present) {
		Json &program = fileEntry(files, "programa");
		program["arquivo"] = m.program.file;
		program["largura"] = m.program.width;
		program["altura"] = m.program.height;
		program["fps"] = fpsValue(m.program.fps);
		program["offset_ms"] = m.program.offsetMs;
		program["faixas_audio"] = m.program.audioTracks;
	} else {
		files.erase(std::string("programa"));
	}
	writeIso(files, kScreenTrack, m.screen);
	writeIso(files, kCameraTrack, m.camera);
	if (m.cursor.present) {
		Json &cursor = fileEntry(files, "cursor");
		cursor["arquivo"] = m.cursor.file;
		cursor["largura"] = m.cursor.width;
		cursor["altura"] = m.cursor.height;
		cursor["origem"] = originJson(m.cursor.origin);
	} else {
		files.erase(std::string("cursor"));
	}

	if (m.hasMouse) {
		const Json fresh = Json::parse(mouseStyleToJson(m.mouse), nullptr, false);
		Json &mouse = root["mouse"];
		if (!mouse.is_object()) {
			mouse = Json::object();
		}
		mergeInto(mouse, fresh);
	}

	Json events = Json::array();
	for (const ManifestEvent &event : m.events) {
		Json item = Json::object();
		item["t_ms"] = event.tMs;
		item["tipo"] = event.type;
		if (!event.track.empty()) {
			item["faixa"] = event.track;
		}
		if (event.code != 0) {
			item["codigo"] = event.code;
		}
		if (!event.reason.empty()) {
			item["motivo"] = event.reason;
		}
		appendExtras(item, event.extraJson);
		events.push_back(std::move(item));
	}
	root["eventos"] = std::move(events);

	return root.dump(2, ' ', false, Json::error_handler_t::replace) + "\n";
}

ManifestParse parseManifest(const std::string &text)
{
	ManifestParse result;
	const Json root = Json::parse(text, nullptr, false);
	if (root.is_discarded() || !root.is_object()) {
		result.error = "JSON invalido";
		return result;
	}
	if (getString(root, "formato") != "voltz-projeto") {
		result.error = "formato desconhecido";
		return result;
	}

	ProjectManifest &m = result.manifest;
	m.version = static_cast<int>(getInt(root, "versao", kManifestVersion));
	result.newerVersion = m.version > kManifestVersion;
	m.app = getString(root, "app");
	m.engine = getString(root, "motor");
	m.name = getString(root, "nome");
	m.createdAt = getString(root, "criado_em");
	m.status = statusFromString(getString(root, "status"));
	m.t0Ns = getInt(root, "t0_ns", 0);
	m.durationMs = getInt(root, "duracao_ms", 0);

	if (const Json *pauses = getChild(root, "pausas", true)) {
		for (const Json &item : *pauses) {
			if (!item.is_object()) {
				continue;
			}
			m.pauses.push_back(PauseSpan{getInt(item, "inicio_ms", 0), getInt(item, "fim_ms", -1)});
			m.pauseExtras.push_back(extrasOf(item, {"inicio_ms", "fim_ms"}));
		}
	}

	if (const Json *files = getChild(root, "arquivos", false)) {
		if (const Json *program = getChild(*files, "programa", false)) {
			m.program.present = true;
			m.program.file = getString(*program, "arquivo");
			m.program.width = static_cast<int>(getInt(*program, "largura", 0));
			m.program.height = static_cast<int>(getInt(*program, "altura", 0));
			m.program.fps = getDouble(*program, "fps", 0.0);
			m.program.offsetMs = getInt(*program, "offset_ms", 0);
			m.program.audioTracks = static_cast<int>(getInt(*program, "faixas_audio", 0));
		}
		if (const Json *cursor = getChild(*files, "cursor", false)) {
			m.cursor.present = true;
			m.cursor.file = getString(*cursor, "arquivo");
			m.cursor.width = static_cast<int>(getInt(*cursor, "largura", 0));
			m.cursor.height = static_cast<int>(getInt(*cursor, "altura", 0));
			if (const Json *origin = getChild(*cursor, "origem", false)) {
				readOrigin(*origin, m.cursor.origin);
			}
		}
		readIso(*files, kScreenTrack, m.screen);
		readIso(*files, kCameraTrack, m.camera);
	}

	if (const Json *events = getChild(root, "eventos", true)) {
		for (const Json &item : *events) {
			if (!item.is_object()) {
				continue;
			}
			m.events.push_back(ManifestEvent{
				getInt(item, "t_ms", 0), getString(item, "tipo"), getString(item, "faixa"),
				static_cast<int>(getInt(item, "codigo", 0)), getString(item, "motivo"),
				extrasOf(item, {"t_ms", "tipo", "faixa", "codigo", "motivo"})});
		}
	}

	if (const Json *mouse = getChild(root, "mouse", false)) {
		m.hasMouse = mouseStyleFromJson(mouse->dump(), m.mouse);
	}

	m.preservedJson = text;
	result.ok = true;
	return result;
}

void recoverManifest(ProjectManifest &manifest, int64_t fileElapsedMs)
{
	const int64_t gross = std::max<int64_t>(manifest.durationMs, std::max<int64_t>(0, fileElapsedMs));
	manifest.durationMs = gross;
	for (PauseSpan &pause : manifest.pauses) {
		if (pause.endMs < 0) {
			pause.endMs = std::max(pause.startMs, gross);
		}
	}
	manifest.status = ProjectStatus::Recovered;
	manifest.events.push_back(ManifestEvent{gross, "recuperado", "", 0});
}

int dropUnusableIsoFiles(ProjectManifest &manifest, const std::function<bool(const std::string &file)> &usable)
{
	int dropped = 0;
	const std::pair<IsoFile *, const char *> entries[] = {{&manifest.screen, kScreenTrack},
							      {&manifest.camera, kCameraTrack}};
	for (const auto &entry : entries) {
		IsoFile &iso = *entry.first;
		if (!iso.present || usable(iso.file)) {
			continue;
		}
		iso = IsoFile{};
		manifest.events.push_back(ManifestEvent{0, "iso_falhou", entry.second, 0, "arquivo_ausente"});
		dropped++;
	}
	return dropped;
}

} // namespace voltz
