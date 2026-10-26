#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTranslator>

namespace botc::i18n
{
    /**
     * Owns the application translators and the active user interface language.
     *
     * Catalogs are compiled from DBConnect/translations/*.ts and embedded into
     * the executable by qt_add_translations under the ":/i18n" resource prefix,
     * so a language is available exactly when its "dbconnect_<code>.qm" file is
     * present there. English is the source language of the user interface, so it
     * never needs a catalog.
     */
    class LocalizationManager final : public QObject
    {
        Q_OBJECT

    public:
        /** Language the source strings are written in. */
        static constexpr auto kSourceLanguage = "en";
        /** Preference that follows the operating system UI language. */
        static constexpr auto kSystemLanguage = "auto";

        static LocalizationManager* instance()
        {
            static auto _instance = LocalizationManager{};
            return &_instance;
        }

        /** Source language first, then every language with an embedded catalog. */
        [[nodiscard]] QStringList availableLanguages() const;

        /** Language applied right now; equals the source language as a fallback. */
        [[nodiscard]] QString currentLanguage() const { return m_currentLanguage; }

        /** Preference requested last; "auto" is kept as it was stored. */
        [[nodiscard]] QString preference() const { return m_preference; }

        /** Turns a stored preference into a language code that can be applied. */
        [[nodiscard]] QString resolveLanguage(const QString& preference) const;

        /** Installs the translators for the preference; false when it falls back. */
        bool applyLanguage(const QString& preference);

        /** Native language name for the language selector; codes are kept as a fallback. */
        [[nodiscard]] static QString displayName(const QString& languageCode);

        /** UI language of the operating system when a catalog exists, source language otherwise. */
        [[nodiscard]] static QString systemLanguage();

    signals:
        void languageChanged(const QString& languageCode);

    private:
        LocalizationManager() = default;

        /** Replaces the installed translators with the ones of the language. */
        bool loadTranslators(const QString& languageCode);

    private:
        QTranslator m_appTranslator;
        QTranslator m_qtTranslator;
        QString m_currentLanguage{QString::fromLatin1(kSourceLanguage)};
        QString m_preference{QString::fromLatin1(kSourceLanguage)};
        bool m_appTranslatorInstalled = false;
        bool m_qtTranslatorInstalled = false;
    };
} // namespace botc::i18n
