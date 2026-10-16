#include "DeviceSelector.hpp"

#include <obs-frontend-api.h>

#include "moc_DeviceSelector.cpp"

namespace voltz {

DeviceSelector::DeviceSelector(std::function<OBSSource()> resolveSource, const char *property, QWidget *parent)
	: QComboBox(parent),
	  resolveSource_(std::move(resolveSource)),
	  property_(property)
{
	connect(this, &QComboBox::currentIndexChanged, this, [this]() {
		if (loading_ || currentIndex() < 0) {
			return;
		}
		OBSSource source = resolveSource_();
		if (!source) {
			return;
		}
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_string(settings, property_, currentData().toString().toUtf8().constData());
		obs_source_update(source, settings);
		obs_frontend_save();
	});
	reload();
}

void DeviceSelector::reload()
{
	loading_ = true;
	clear();
	OBSSource source = resolveSource_();
	setEnabled(bool(source));
	if (source) {
		OBSDataAutoRelease settings = obs_source_get_settings(source);
		const QString current = QString::fromUtf8(obs_data_get_string(settings, property_));
		obs_properties_t *props = obs_source_properties(source);
		if (obs_property_t *p = props ? obs_properties_get(props, property_) : nullptr) {
			const size_t count = obs_property_list_item_count(p);
			for (size_t i = 0; i < count; i++) {
				if (obs_property_list_item_disabled(p, i)) {
					continue;
				}
				const char *value = obs_property_list_item_string(p, i);
				addItem(QString::fromUtf8(obs_property_list_item_name(p, i)),
					QString::fromUtf8(value ? value : ""));
			}
		}
		obs_properties_destroy(props);
		const int index = findData(current);
		setCurrentIndex(index >= 0 ? index : -1);
	}
	loading_ = false;
}

} // namespace voltz
