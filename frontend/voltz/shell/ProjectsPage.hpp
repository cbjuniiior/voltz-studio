#pragma once

#include <QWidget>

class QFrame;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace voltz {

class FrontendBridge;
class ProjectRecorder;

// Tela "Projetos" (spec §4.3): projetos (pastas com projeto.voltz) e vídeos soltos da pasta de gravações, mais
// recentes primeiro. Abrir, mostrar na pasta, renomear (nome do manifesto), recuperar gravação interrompida e mover
// para a lixeira (pasta inteira). Atualiza ao aparecer e quando um projeto começa/termina.
class ProjectsPage : public QWidget {
	Q_OBJECT

public:
	explicit ProjectsPage(FrontendBridge *bridge, ProjectRecorder *recorder, QWidget *parent = nullptr);

protected:
	void showEvent(QShowEvent *event) override;

private:
	void refresh();
	QTreeWidgetItem *selectedItem() const;
	void updateButtons();
	void openSelected();
	void showSelectedInFolder();
	void renameSelected();
	void recoverSelected();
	void recoverAll();
	void trashSelected();

	ProjectRecorder *recorder_;
	QString folder_;
	QLabel *folderLabel_ = nullptr;
	QLabel *empty_ = nullptr;
	QFrame *recoverBanner_ = nullptr;
	QLabel *recoverText_ = nullptr;
	QTreeWidget *list_ = nullptr;
	QPushButton *openButton_ = nullptr;
	QPushButton *showButton_ = nullptr;
	QPushButton *renameButton_ = nullptr;
	QPushButton *recoverButton_ = nullptr;
	QPushButton *trashButton_ = nullptr;
};

} // namespace voltz
