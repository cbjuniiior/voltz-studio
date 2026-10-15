#pragma once

#include <widgets/OBSQTDisplay.hpp>

#include <cstdint>

namespace voltz {

// Mostra a saída principal do OBS (o que será gravado), centralizada e sem distorção.
class VoltzPreview : public OBSQTDisplay {
	Q_OBJECT

public:
	explicit VoltzPreview(QWidget *parent = nullptr);
	~VoltzPreview() override;

private:
	static void draw(void *data, uint32_t cx, uint32_t cy);
};

} // namespace voltz
