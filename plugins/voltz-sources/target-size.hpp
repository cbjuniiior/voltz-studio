#pragma once

#include <obs.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace voltz {

// Tamanho da captura "alvo" de uma fonte Voltz (UUID no ajuste live::kTargetSetting): as camadas da Tela Voltz têm
// exatamente o tamanho da Tela, então o pixel (x,y) da captura cai no mesmo lugar em todas. A Tela pode carregar depois
// da camada: tick() tenta achá-la de novo a cada 60 tiques. setUuid na thread de quem atualiza a fonte; tick na thread
// gráfica; width/height em qualquer thread.
class TargetSize {
public:
	TargetSize() = default;
	~TargetSize();
	TargetSize(const TargetSize &) = delete;
	TargetSize &operator=(const TargetSize &) = delete;

	void setUuid(const std::string &uuid);
	void tick();
	uint32_t width() const { return width_.load(); }
	uint32_t height() const { return height_.load(); }

private:
	void resolve(); // com mutex_ preso

	std::mutex mutex_;
	std::string uuid_;
	obs_weak_source_t *target_ = nullptr;
	int retryTicks_ = 0;
	std::atomic<uint32_t> width_{0};
	std::atomic<uint32_t> height_{0};
};

} // namespace voltz
