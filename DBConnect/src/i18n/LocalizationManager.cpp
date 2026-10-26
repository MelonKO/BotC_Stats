#include "LocalizationManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QLocale>

#include <string_view>

namespace
{
    constexpr auto kCatalogResourceDir = ":/i18n";
    constexpr std::string_view kCatalogPrefix{"dbconnect_"};
    constexpr auto kQtTranslationsDir = "/translations";

    /** Reduces "ru_RU", "ru-RU" and "ru" to the same catalog code. */
    QString normalizeLanguageCode(const QString& language)
    {
        QString normalized = language.trimmed();
        normalized.replace('-', '_');
        return normalized.section('_', 0, 0).toLower();
    }
} // namespace

namespace botc::i18n
{
    QStringList LocalizationManager::availableLanguages() const
    {
        QStringList languages{kSourceLanguage};

        const QStringList filters{QStringLiteral("dbconnect_*.qm")};
        const QStringList catalogs = QDir(kCatalogResourceDir).entryList(filters, QDir::Files, QDir::Name);
        for (const QString& catalog : catalogs)
        {
            const QString fileName = QFileInfo{catalog}.completeBaseName();
            const QString code = normalizeLanguageCode(
                fileName.mid(static_cast<int>(kCatalogPrefix.size())));
            if (!code.isEmpty() && !languages.contains(code))
                languages << code;
        }

        return languages;
    }

    QString LocalizationManager::resolveLanguage(const QString& preference) const
    {
        const QString normalizedPreference = preference.trimmed();
        if (normalizedPreference.isEmpty() || normalizedPreference.compare(kSystemLanguage, Qt::CaseInsensitive) == 0)
            return systemLanguage();

        const QString code = normalizeLanguageCode(normalizedPreference);
        if (availableLanguages().contains(code))
            return code;

        qWarning("No translation catalog for language \"%s\"; following the system language instead.",
                 qPrintable(preference));
        return systemLanguage();
    }

    bool LocalizationManager::applyLanguage(const QString& preference)
    {
        m_preference = preference;

        const QString resolvedLanguage = resolveLanguage(preference);
        const bool loaded = loadTranslators(resolvedLanguage);
        const QString appliedLanguage = loaded ? resolvedLanguage : QString::fromLatin1(kSourceLanguage);

        if (appliedLanguage == m_currentLanguage)
            return loaded;

        m_currentLanguage = appliedLanguage;
        emit languageChanged(m_currentLanguage);
        return loaded;
    }

    QString LocalizationManager::displayName(const QString& languageCode)
    {
        const QString code = normalizeLanguageCode(languageCode);
        if (code == QLatin1String(kSourceLanguage))
            return QStringLiteral("English");
        if (code == QLatin1String("ru"))
            return QStringLiteral("Русский");

        const QLocale locale{code};
        const QString nativeName = locale.nativeLanguageName();
        return nativeName.isEmpty() ? languageCode : nativeName;
    }

    QString LocalizationManager::systemLanguage()
    {
        const QStringList available = instance()->availableLanguages();
        for (const QString& uiLanguage : QLocale::system().uiLanguages())
        {
            const QString code = normalizeLanguageCode(uiLanguage);
            if (available.contains(code))
                return code;
        }

        return QString::fromLatin1(kSourceLanguage);
    }

    bool LocalizationManager::loadTranslators(const QString& languageCode)
    {
        if (m_appTranslatorInstalled)
        {
            QCoreApplication::removeTranslator(&m_appTranslator);
            m_appTranslatorInstalled = false;
        }
        if (m_qtTranslatorInstalled)
        {
            QCoreApplication::removeTranslator(&m_qtTranslator);
            m_qtTranslatorInstalled = false;
        }

        if (languageCode.isEmpty() || languageCode == QLatin1String(kSourceLanguage))
            return true;

        const QString catalog = QStringLiteral(":/i18n/dbconnect_%1.qm").arg(languageCode);
        if (!m_appTranslator.load(catalog))
        {
            qWarning("Failed to load the translation catalog \"%s\".", qPrintable(catalog));
            return false;
        }
        QCoreApplication::installTranslator(&m_appTranslator);
        m_appTranslatorInstalled = true;

        // Standard Qt dialogs (file dialog buttons, message box buttons) come
        // from the qtbase catalog. It is optional: without it the application
        // strings are still translated, only the Qt-provided ones stay English.
        const QString qtCatalog = QStringLiteral("qtbase_%1").arg(languageCode);
        const QString deployedTranslations = QCoreApplication::applicationDirPath() + kQtTranslationsDir;
        if (m_qtTranslator.load(qtCatalog, QLibraryInfo::path(QLibraryInfo::TranslationsPath)) ||
            m_qtTranslator.load(qtCatalog, deployedTranslations))
        {
            QCoreApplication::installTranslator(&m_qtTranslator);
            m_qtTranslatorInstalled = true;
        }

        return true;
    }
} // namespace botc::i18n
