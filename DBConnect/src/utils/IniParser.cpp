#include "IniParser.h"

#include <iniparser.h>
#include <qdebug.h>

namespace botc::utils
{
    IniParser::IniParser(const QStringView in_filename)
    {
        m_dictionary = iniparser_load(in_filename.toUtf8().constData());
        if (m_dictionary == nullptr)
        {
            qDebug() << "cannot parse file: " << in_filename;
        }
    }

    IniParser::~IniParser()
    {
        if (m_dictionary)
        {
            iniparser_freedict(m_dictionary);
        }
    }

    int IniParser::GetNumberOfSections() const
    {
        return iniparser_getnsec(m_dictionary);
    }

    QString IniParser::GetSectionName(const int in_sectionNumber) const
    {
        return iniparser_getsecname(m_dictionary, in_sectionNumber);
    }

    int IniParser::GetNumberOfKeys(const QStringView in_sectionName) const
    {
        return iniparser_getsecnkeys(m_dictionary, in_sectionName.toUtf8().constData());
    }

    QVector<QString> IniParser::GetSectionKeys(const QStringView in_sectionName) const
    {
        const int numberOfKeys = GetNumberOfKeys(in_sectionName);
        QVector<const char*> sectionNames{numberOfKeys};
        const char** result = iniparser_getseckeys(m_dictionary, in_sectionName.toUtf8().constData(),
                                                   sectionNames.data());
        if (result == nullptr)
        {
            return QVector<QString>();
        }
        else
        {
            QVector<QString> output;
            std::ranges::copy(sectionNames, std::back_inserter(output));
            return output;
        }
    }

    QString IniParser::GetString(const QStringView in_key, const QStringView in_default) const
    {
        return iniparser_getstring(m_dictionary,
                                   in_key.toUtf8().constData(),
                                   in_default.toUtf8().constData());
    }

    int IniParser::GetInt(const QStringView in_key, const int in_default) const
    {
        return iniparser_getint(m_dictionary, in_key.toUtf8().constData(), in_default);
    }

    long IniParser::GetLong(const QStringView in_key, const long in_default) const
    {
        return iniparser_getlongint(m_dictionary, in_key.toUtf8().constData(), in_default);
    }

    qint64 IniParser::GetInt64(const QStringView in_key, const qint64 in_default) const
    {
        return iniparser_getint64(m_dictionary, in_key.toUtf8().constData(), in_default);
    }

    quint64 IniParser::GetUInt64(const QStringView in_key, const quint64 in_default) const
    {
        return iniparser_getuint64(m_dictionary, in_key.toUtf8().constData(), in_default);
    }

    double IniParser::GetDouble(const QStringView in_key, const double in_default) const
    {
        return iniparser_getdouble(m_dictionary, in_key.toUtf8().constData(), in_default);
    }

    bool IniParser::GetBool(const QStringView in_key, const bool in_default) const
    {
        return iniparser_getboolean(m_dictionary, in_key.toUtf8().constData(), in_default);
    }

    void IniParser::SetEntry(const QStringView in_entry, const QStringView in_value) const
    {
        iniparser_set(m_dictionary, in_entry.toUtf8().constData(), in_value.toUtf8().constData());
    }

    void IniParser::UnsetEntry(const QStringView in_entry) const
    {
        iniparser_unset(m_dictionary, in_entry.toUtf8().constData());
    }

    bool IniParser::Contains(const QStringView in_entry) const
    {
        return iniparser_find_entry(m_dictionary, in_entry.toUtf8().constData());
    }
}
