#pragma once

#include <voltz/core/recording-settings.hpp>

#include <QString>

namespace voltz {

// Lê/escreve as configurações de gravação do perfil do OBS (modo de saída Simples).
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

	// Primeira execução/preset: se RecQuality ainda for o padrão do OBS ("Stream"), passa a "HQ" com o
	// codificador automático e formato MP4 híbrido. Não sobrescreve escolhas já feitas.
	static void applyVoltzDefaults();

private:
	// Salva o perfil e, se não estiver gravando, recria as saídas para valer já.
	static void commit(bool needsOutputReset);
};

} // namespace voltz
