#pragma once

#include <voltz/core/recording-settings.hpp>

#include <QString>

#include <cstdint>
#include <string>
#include <vector>

namespace voltz {

// Lê/escreve as configurações de gravação do perfil do OBS (modo de saída Simples) e as regras de pasta/faixas da
// gravação em projeto. Não é UI: o ProjectRecorder também usa.
class RecordingConfig {
public:
	static bool isSimpleMode();
	static QString folder();
	static void setFolder(const QString &path);
	static RecordingFormat format();
	static void setFormat(RecordingFormat format);
	static RecordingQuality quality();
	static void setQuality(RecordingQuality quality);
	// RecEncoder escolhido automaticamente (AMD → NVIDIA → Intel → x264) entre os encoders registrados.
	static std::string automaticEncoder();
	static std::string currentEncoder();
	// RecQuality "Stream" (padrão do OBS): a gravação reusa o codificador da transmissão (StreamEncoder).
	static bool recordsWithStreamEncoder();
	static std::string streamEncoder();

	// Primeira execução/preset: se RecQuality ainda for o padrão do OBS ("Stream"), passa a "HQ" com o
	// codificador automático e formato MP4 híbrido. Não sobrescreve escolhas já feitas.
	static void applyVoltzDefaults();

	// Chave do perfil com a pasta de gravação no modo atual (Simples: SimpleOutput/FilePath; Avançado: AdvOut/
	// RecFilePath ou, com FFmpeg personalizado gravando em arquivo, AdvOut/FFFilePath). `section` nulo: FFmpeg para
	// URL (sem pasta, sem projeto).
	struct OutputPathKey {
		const char *section = nullptr;
		const char *key = nullptr;
	};
	static OutputPathKey outputPathKey();
	// Pasta de gravação em uso (mesma regra do OBSBasic::GetCurrentOutputPath; vazio com FFmpeg para URL).
	static QString outputFolder();
	// Faixas de áudio da próxima gravação: 1 com qualidade "Stream" ou FLV; senão os bits de RecTracks.
	static int recordingAudioTracks();
	// Padrões da gravação em projeto (spec §5.1), uma vez por perfil e sem sobrescrever escolhas: pasta
	// "Vídeos\Voltz Studio" onde SimpleOutput/FilePath e AdvOut/RecFilePath ainda estão no padrão do OBS, e 3 faixas
	// (mix, mic, sistema) se SimpleOutput/RecTracks ainda é o padrão. Cria a pasta Voltz quando ela é a pasta em uso.
	static void applyProjectDefaults();
	// Estimativa de bytes por minuto com a configuração atual (antes de haver medição real).
	static uint64_t estimatedBytesPerMinute();

private:
	// Salva o perfil e, se não estiver gravando, recria as saídas para valer já.
	static void commit(bool needsOutputReset);
};

// Troca temporária de valores do perfil do OBS. restore() devolve exatamente o estado anterior: regrava o valor
// anterior ou, se a chave não existia, remove-a (não cria valor novo). Não salva o perfil em disco.
class ProfileOverride {
public:
	ProfileOverride() = default;
	ProfileOverride(const ProfileOverride &) = delete;
	ProfileOverride &operator=(const ProfileOverride &) = delete;
	~ProfileOverride() { restore(); }

	void set(const char *section, const char *key, const std::string &value);
	void restore();
	bool active() const { return !saved_.empty(); }

private:
	struct Saved {
		std::string section;
		std::string key;
		std::string value;
		bool hadUserValue = false;
	};
	std::vector<Saved> saved_;
};

} // namespace voltz
