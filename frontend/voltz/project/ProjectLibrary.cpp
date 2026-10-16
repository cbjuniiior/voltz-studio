#include "ProjectLibrary.hpp"

#include "ProjectRecorder.hpp" // nativeUtf8Path

#include <voltz/core/atomic-file.hpp>
#include <voltz/core/recording-files.hpp>

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <string>
#include <utility>

namespace voltz {

namespace {

QString manifestPath(const QString &projectDir)
{
	return QDir(projectDir).filePath(QString::fromUtf8(kManifestFileName));
}

} // namespace

bool ProjectLibrary::readManifest(const QString &projectDir, ProjectManifest &out, bool *newerFormat)
{
	std::string text;
	if (!readFileText(nativeUtf8Path(manifestPath(projectDir)), text)) {
		return false;
	}
	ManifestParse parsed = parseManifest(text);
	if (!parsed.ok) {
		return false;
	}
	out = std::move(parsed.manifest);
	if (newerFormat) {
		*newerFormat = parsed.newerVersion;
	}
	return true;
}

QList<ProjectEntry> ProjectLibrary::scan(const QString &folder)
{
	QList<ProjectEntry> entries;
	const QDir root(folder);
	if (folder.isEmpty() || !root.exists()) {
		return entries;
	}

	for (const QFileInfo &dirInfo : root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		const QString dir = dirInfo.absoluteFilePath();
		if (!QFileInfo::exists(manifestPath(dir))) {
			continue;
		}
		ProjectEntry entry;
		entry.isProject = true;
		entry.path = dir;
		ProjectManifest manifest;
		if (readManifest(dir, manifest, &entry.newerFormat)) {
			entry.name = QString::fromStdString(manifest.name);
			entry.date = QDateTime::fromString(QString::fromStdString(manifest.createdAt), Qt::ISODate);
			entry.durationMs = manifestNetDurationMs(manifest);
			entry.status = manifest.status;
			if (manifest.program.present && !manifest.program.file.empty()) {
				entry.programFile = QDir(dir).filePath(QString::fromStdString(manifest.program.file));
			}
		} else {
			entry.unreadable = true;
			entry.status = ProjectStatus::Failed;
		}
		if (entry.name.isEmpty()) {
			entry.name = dirInfo.fileName();
		}
		if (!entry.date.isValid()) {
			entry.date = dirInfo.lastModified();
		}
		for (const QFileInfo &file : QDir(dir).entryInfoList(QDir::Files)) {
			entry.sizeBytes += static_cast<quint64>(file.size());
		}
		entries.append(entry);
	}

	for (const QFileInfo &file : root.entryInfoList(QDir::Files)) {
		if (!isRecordingFile(file.fileName().toStdString())) {
			continue;
		}
		ProjectEntry entry;
		entry.path = file.absoluteFilePath();
		entry.programFile = entry.path;
		entry.name = file.fileName();
		entry.date = file.lastModified();
		entry.sizeBytes = static_cast<quint64>(file.size());
		entries.append(entry);
	}

	std::sort(entries.begin(), entries.end(),
		  [](const ProjectEntry &a, const ProjectEntry &b) { return a.date > b.date; });
	return entries;
}

QStringList ProjectLibrary::recentNames(const QString &folder, int max)
{
	QStringList names;
	for (const ProjectEntry &entry : scan(folder)) {
		if (!entry.isProject || entry.unreadable || names.contains(entry.name)) {
			continue;
		}
		names.append(entry.name);
		if (names.size() >= max) {
			break;
		}
	}
	return names;
}

bool ProjectLibrary::rename(const QString &projectDir, const QString &newName)
{
	const QString trimmed = newName.trimmed();
	ProjectManifest manifest;
	bool newer = false;
	if (trimmed.isEmpty() || !readManifest(projectDir, manifest, &newer) || newer) {
		return false;
	}
	manifest.name = trimmed.toUtf8().toStdString();
	return writeFileAtomically(nativeUtf8Path(manifestPath(projectDir)), serializeManifest(manifest));
}

bool ProjectLibrary::recover(const QString &projectDir)
{
	ProjectManifest manifest;
	bool newer = false;
	if (!readManifest(projectDir, manifest, &newer) || newer || manifest.status != ProjectStatus::Recording) {
		return false;
	}
	const QDir dir(projectDir);
	const QDateTime created = QDateTime::fromString(QString::fromStdString(manifest.createdAt), Qt::ISODate);
	/* Duração estimada pelo arquivo modificado por último (programa ou ISO): todos crescem até a queda. */
	qint64 fileElapsedMs = 0;
	auto elapsedOf = [&dir, &created, &fileElapsedMs](bool present, const std::string &file) {
		if (!present || file.empty() || !created.isValid()) {
			return;
		}
		const QFileInfo info(dir.filePath(QString::fromStdString(file)));
		if (info.exists()) {
			fileElapsedMs = std::max<qint64>(fileElapsedMs, created.msecsTo(info.lastModified()));
		}
	};
	elapsedOf(manifest.program.present, manifest.program.file);
	elapsedOf(manifest.screen.present, manifest.screen.file);
	elapsedOf(manifest.camera.present, manifest.camera.file);
	/* MKV escrito aos poucos sobrevive à queda; ISO sem arquivo (ou vazio) sai do manifesto com iso_falhou. */
	const int dropped = dropUnusableIsoFiles(manifest, [&dir](const std::string &file) {
		const QFileInfo info(dir.filePath(QString::fromStdString(file)));
		return !file.empty() && info.exists() && info.size() > 0;
	});
	recoverManifest(manifest, fileElapsedMs);
	const bool ok = writeFileAtomically(nativeUtf8Path(manifestPath(projectDir)), serializeManifest(manifest));
	blog(LOG_INFO, "[voltz] recuperar projeto (%s, %lld ms, %d ISO sem arquivo): %s", ok ? "ok" : "falhou",
	     static_cast<long long>(manifest.durationMs), dropped, qUtf8Printable(projectDir));
	return ok;
}

} // namespace voltz
