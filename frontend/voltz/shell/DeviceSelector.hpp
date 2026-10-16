#pragma once

#include <obs.hpp>

#include <QComboBox>

#include <functional>

namespace voltz {

// Combobox com as opções de uma propriedade de lista de uma fonte (ex.: "monitor_id", "video_device_id",
// "device_id"). Trocar a opção atualiza a fonte (obs_source_update) e salva a coleção.
class DeviceSelector : public QComboBox {
	Q_OBJECT

public:
	// `resolveSource` devolve a fonte atual (ref forte) a cada recarga — ela pode ser recriada pelo OBS.
	DeviceSelector(std::function<OBSSource()> resolveSource, const char *property, QWidget *parent = nullptr);

	// Recarrega opções e valor atual. Sem fonte: desabilita.
	void reload();

private:
	std::function<OBSSource()> resolveSource_;
	const char *property_;
	bool loading_ = false;
};

} // namespace voltz
