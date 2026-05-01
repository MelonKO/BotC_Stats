#pragma once
#include <dictionary.h>
#include <qstringview.h>

class QFile;

namespace botc::utils
{
    // TODO:: explicit IniParser(QFile* in_file);
    class IniParser
    {
    public:
        explicit IniParser(QStringView in_filename);

        ~IniParser();

        bool IsValid() const { return !!m_dictionary; }

        // Sections

        int GetNumberOfSections() const;
        QString GetSectionName(int in_sectionNumber) const;
        int GetNumberOfKeys(QStringView in_sectionName) const;
        QVector<QString> GetSectionKeys(QStringView in_sectionName) const;

        // Getters

        QString GetString(QStringView in_key, QStringView in_default) const;
        int GetInt(QStringView in_key, int in_default) const;
        long GetLong(QStringView in_key, long in_default) const;
        qint64 GetInt64(QStringView in_key, qint64 in_default) const;
        quint64 GetUInt64(QStringView in_key, quint64 in_default) const;
        double GetDouble(QStringView in_key, double in_default) const;
        bool GetBool(QStringView in_key, bool in_default) const;

        // Setters

        void SetEntry(QStringView in_entry, QStringView in_value) const;
        void UnsetEntry(QStringView in_entry) const;

        bool Contains(QStringView in_entry) const;

    private:
        dictionary* m_dictionary;
    };
}
