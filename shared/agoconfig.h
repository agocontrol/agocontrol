#ifndef AGOCONFIG_H
#define AGOCONFIG_H

#include <iostream>

#include <boost/filesystem.hpp>

#include <json/json.h>


namespace agocontrol {

    /// Return the full path to the configuration directory, with subpath appended if not NULL
    /// If subpath is set, it attempts to create all parent directories if they do not already exist
    boost::filesystem::path getConfigPath(const boost::filesystem::path &subpath = boost::filesystem::path());

    /// Return the full path to the local-state directory, with subpath appended if not NULL
    /// If subpath is set, it attempts to create all parent directories if they do not already exist
    boost::filesystem::path getLocalStatePath(const boost::filesystem::path &subpath = boost::filesystem::path());

    // XXX: This should preferably also be in AgoClientInternal, but called from global
    // ensureDirExists..
    void initDirectorys();
    class AgoApp;
    class TestAgoConfig;
    class AgoClientInternal {
        // Do not expose to other than AgoApp
        friend class AgoApp;
        friend class TestAgoConfig;
        protected:
        static void setConfigDir(const boost::filesystem::path &dir);
        static void setLocalStateDir(const boost::filesystem::path &dir);

        // For unit-test only
        static void _reset();
    };

    /// Ensures that the directory exists
    /// Note: throws exception if directory creation fails
    boost::filesystem::path ensureDirExists(const boost::filesystem::path &dir);

    /// Ensures that the parent directory for the specified filename exists
    /// Note: throws exception if directory creation fails
    boost::filesystem::path ensureParentDirExists(const boost::filesystem::path &filename);

    bool augeas_init();


    class ExtraConfigNameList;
    /**
     * ConfigNameLists are used to give one or more section or app names
     * to use.
     * Normally, they are implicitly created when a plain string is given,
     * but they can be manually created too.
     */
    class ConfigNameList {
        std::list<std::string> name_list;
    protected:
        bool extra;

    public:
        ConfigNameList() : extra(false) {}
        ConfigNameList(const char* name) ; // Must keep this to allow implicit conversion of "xxx" -> ConfigNameList("xxx")
        ConfigNameList(const std::string& name) ;
        ConfigNameList(const ConfigNameList &names) ;
        ConfigNameList(const ConfigNameList &names1, const ConfigNameList &names2) ;
        ConfigNameList& add(const std::string &name) ;
        ConfigNameList& addAll(const ConfigNameList& names) ;
        int empty() const { return name_list.empty(); }
        int size() const { return name_list.size(); }
        bool isExtra() const { return extra; }
        const std::list<std::string>& names() const { return name_list; }

        friend std::ostream& operator<< (std::ostream& os, const ConfigNameList &list) ;
    };

    /* If this is fed into AgoApps methods, it will add it's own
     * name ahead of all specified names in this class.
     */
    class ExtraConfigNameList : public ConfigNameList {
    public:
        ExtraConfigNameList(const std::string& name)
            : ConfigNameList(name)
        {
            extra = true;
        }

        static ExtraConfigNameList toExtra(const ConfigNameList& existing) {
            ExtraConfigNameList n;
            n.addAll(existing);
            return n;
        }
    private:
        ExtraConfigNameList() {
            extra = true;
        }
    };

    /* Use this for default values */
    extern const ConfigNameList BLANK_CONFIG_NAME_LIST;


    /**
     * Read a config option from the configuration subsystem.
     *
     * The system is based on per-app configuration files, which has sections
     * and options.
     *
     * This is a low-level implementation, please try to use AgoApp's instance version
     * of getConfigOption instead.
     *
     * Note that "option not set" means not set, or empty value!
     *
     * Arguments:
     *  section -- A ConfigNameList section to look for the option in.
     *      Note that a regular string can be passed, it will create an implicit ConfigNameList.
     *
     *  option -- The name of the option to retrieve
     *
     *  defaultValue -- If the option can not be found in any of the specified
     *      sections, fall back to this value.
     *
     *  app -- A ConfigNameList identifying the configuration storage unit to look in.
     *      If omited, it defaults to the section.
     *
     * Lookup order:
     *  For each specified app, we look at each specified section/option. First non-empty
     *  value wins.
     *
     * Returns:
     *  The value found.
     *  If not found, defaultValue is passed through unmodified.
     */
    // keep a distinct char* to avoid ambiguous overload between std::string and fs::path when char* is given
    std::string getConfigSectionOption(const ConfigNameList& section, const std::string& option, const char* defaultValue, const ConfigNameList& app = BLANK_CONFIG_NAME_LIST);
    std::string getConfigSectionOption(const ConfigNameList& section, const std::string& option, const std::string& defaultValue, const ConfigNameList& app = BLANK_CONFIG_NAME_LIST);
    boost::filesystem::path getConfigSectionOption(const ConfigNameList& section, const std::string& option, const boost::filesystem::path& defaultValue, const ConfigNameList& app = BLANK_CONFIG_NAME_LIST);

    /**
     * Read one or more configuration sections, and return all options as a map.
     *
     * Section and app arguments behaves same way as for getConfigSectionOption.
     * If the key is found in multiple locations, the first match is used (same way as getConfigSectionOption would
     * return that specific option).
     *
     * @param section
     * @param app
     * @return
     */
    std::map<std::string, std::string> getConfigSection(const ConfigNameList& section, const ConfigNameList &app);
    Json::Value getConfigTree();

    /**
     * Write a config option to the configuration subsystem.
     *
     * The system is based on per-app configuration files, which has sections
     * and options.
     *
     * This is a low-level implementation, please try to use AgoApp's instance version
     * of setConfigOption instead.
     *
     * Arguments:
     *  section -- A string section in which to store the option in.
     *
     *  option -- The name of the option to set
     *
     *  value -- The value to write
     *
     *  app -- A string identifying the configuration storage unit to store to.
     *      If omited, it defaults to the section.
     *
     * Returns:
     *  true if sucesfully stored, false otherwise.
     *  Please refer to the error log for failure indication.
     */
    bool setConfigSectionOption(const std::string& section, const std::string& option, const std::string& value, const std::string& app = {});
    bool setConfigSectionOption(const std::string& section, const std::string& option, const char* value, const std::string& app = {});
    bool setConfigSectionOption(const std::string& section, const std::string& option, float value, const std::string& app = {});
    bool setConfigSectionOption(const std::string& section, const std::string& option, int value, const std::string& app = {});
    bool setConfigSectionOption(const std::string& section, const std::string& option, bool value, const std::string& app = {});

}
#endif


