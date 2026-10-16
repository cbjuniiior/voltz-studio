#include "mouse-feed.hpp"
#include "mouse-source.hpp"
#include "test-vendor.hpp"

#include <voltz/core/live-protocol.hpp>

#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("voltz-sources", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Voltz Studio: fontes Voltz (destaque do mouse)";
}

bool obs_module_load(void)
{
	voltz::registerMouseSource();
	voltz::registerMouseProcs(obs_get_proc_handler());
	blog(LOG_INFO, "[voltz-sources] carregado");
	return true;
}

void obs_module_post_load(void)
{
	/* a API de fornecedor do obs-websocket só pode ser usada aqui (depois de todos os módulos carregarem) */
	if (voltz::live::testModeEnabled()) {
		voltz::registerTestVendor();
	}
}
