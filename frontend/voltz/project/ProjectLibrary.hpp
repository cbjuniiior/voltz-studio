#pragma once

#include <voltz/core/project-manifest.hpp>

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

namespace voltz {

// Uma linha da tela Projetos: projeto (pasta com projeto.voltz) ou vídeo solto na raiz da pasta de gravações.
struct ProjectEntry {
	bool isProject = false;
	bool unreadable = false;  // projeto.voltz ilegível
	bool newerFormat = false; // versao > 1: só leitura
	QString path;             // pasta do projeto ou o próprio arquivo
	QString programFile;      // vídeo a abrir
	QString name;
	QDateTime date;
	qint64 durationMs = -1; // líquida; -1 = desconhecida
	quint64 sizeBytes = 0;  // soma dos arquivos da pasta (sem subpastas)
	ProjectStatus status = ProjectStatus::Ok;
};

// Leitura e manutenção dos projetos de uma pasta de gravações (thread da UI; IO pequeno).
class ProjectLibrary {
public:
	// Projetos e vídeos soltos, mais recentes primeiro.
	static QList<ProjectEntry> scan(const QString &folder);
	// Nomes distintos dos projetos mais recentes (seletor da tela Gravar).
	static QStringList recentNames(const QString &folder, int max = 8);
	static bool readManifest(const QString &projectDir, ProjectManifest &out, bool *newerFormat = nullptr);
	// Troca só o "nome" do manifesto (a pasta mantém o nome: arquivos podem estar abertos em outro programa).
	static bool rename(const QString &projectDir, const QString &newName);
	// Projeto "gravando" órfão (o app caiu): fecha o manifesto com a duração estimada — maior entre a última pulsação
	// e mtime(programa) − criado_em — e status "recuperado". Falso se não for um projeto "gravando" v1.
	static bool recover(const QString &projectDir);
};

} // namespace voltz
