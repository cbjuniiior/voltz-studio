#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QTreeWidget;

namespace voltz {

class FrontendBridge;

// Tela "Projetos": gravações na pasta de saída atual (mais recentes primeiro), com abrir, mostrar na pasta e
// mover para a lixeira. Atualiza ao aparecer e ao terminar uma gravação. (No Plano 3 vira lista de projetos.)
class ProjectsPage : public QWidget {
	Q_OBJECT

public:
	explicit ProjectsPage(FrontendBridge *bridge, QWidget *parent = nullptr);

protected:
	void showEvent(QShowEvent *event) override;

private:
	void refresh();
	QString selectedPath() const;
	void updateButtons();
	void openSelected();
	void showSelectedInFolder();
	void trashSelected();

	QString folder_;
	QLabel *folderLabel_ = nullptr;
	QLabel *empty_ = nullptr;
	QTreeWidget *list_ = nullptr;
	QPushButton *openButton_ = nullptr;
	QPushButton *showButton_ = nullptr;
	QPushButton *trashButton_ = nullptr;
};

} // namespace voltz
