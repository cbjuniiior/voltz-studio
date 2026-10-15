#pragma once

#include "PresetBuilder.hpp"

#include <QWidget>

namespace voltz {

// Desenho esquemático do preset (16:9), usando a mesma geometria do PresetBuilder.
class PresetThumbnail : public QWidget {
	Q_OBJECT

public:
	explicit PresetThumbnail(PresetId preset, QWidget *parent = nullptr);
	QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	PresetId preset_;
};

} // namespace voltz
