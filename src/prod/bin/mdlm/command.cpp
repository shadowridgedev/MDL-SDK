/******************************************************************************
 * Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/
#include "command.h"
#include "archive.h"
#include "util.h"
#include "options.h"
#include "errors.h"
#include "version.h"
#include "search_path.h"
#include <map>
#include <iostream>
using namespace mdlm;
using std::vector;
using std::string;
using std::cout;
using std::endl;
using std::find;
using std::map;
using mi::base::Handle;
using mi::neuraylib::IMdl_compiler;
using mi::neuraylib::IMdl_info;

namespace mdlm
{
    extern mi::neuraylib::INeuray * neuray(); //Application::theApp().neuray(
    extern void report(const std::string & msg); //Application::theApp().report(
}

int Command::execute()
{
    return 0;
}

class Compatibility_api_helper
{
    mi::base::Handle<mi::neuraylib::IMdl_compatibility_api> m_compatibility_api;

private:
    void handle_message(const mi::neuraylib::IMessage * message) const
    {
        mi::base::Message_severity severity(message->get_severity());
        const char* str = message->get_string();
        //mi::Sint32 code(message->get_code());
        if (str)
        {
            Util::log(str, severity);
        }
        for (mi::Size i = 0; i < message->get_notes_count(); i++)
        {
            mi::base::Handle<const mi::neuraylib::IMessage> note(message->get_note(i));
            handle_message(note.get());
        }
    }
    void handle_return_code(const mi::Sint32 & code) const
    {
        if (code == -1)
        {
            Util::log_error("Invalid parameters");
        }
        else if (code == -2)
        {
            Util::log_error("An error occurred during module comparison");
        }
    }
    void handle_return_code_and_messages(
        const mi::Sint32 & code, const mi::neuraylib::IMdl_execution_context * context) const
    {
        handle_return_code(code);

        for (mi::Size i = 0; i < context->get_messages_count(); i++)
        {
            mi::base::Handle<const mi::neuraylib::IMessage> msg(context->get_message(i));
            check_success(msg != NULL);
            handle_message(msg.get());
        }
        for (mi::Size i = 0; i < context->get_error_messages_count(); i++)
        {
            mi::base::Handle<const mi::neuraylib::IMessage> msg(context->get_error_message(i));
            check_success(msg != NULL);
            handle_message(msg.get());
        }
    }

public:
    Compatibility_api_helper() 
    {
        mi::neuraylib::INeuray * neuray(mdlm::neuray());
        mi::base::Handle<mi::neuraylib::IMdl_compatibility_api> compatibility_api
        (
            neuray->get_api_component<mi::neuraylib::IMdl_compatibility_api>()
        );
        check_success(compatibility_api != NULL);
        m_compatibility_api = mi::base::make_handle_dup(compatibility_api.get());
    }
public:
    static bool Test()
    {
        return true;
    }
public:
    mi::Sint32 compare_modules(std::string & m1, std::string & m2)
    {
        mi::neuraylib::INeuray * neuray(mdlm::neuray());
        mi::base::Handle<mi::neuraylib::IMdl_factory> mdl_factory
        (
            neuray->get_api_component<mi::neuraylib::IMdl_factory>()
        );
        mi::base::Handle<mi::neuraylib::IMdl_execution_context> context(
            mdl_factory->create_execution_context());

        mi::Sint32 rtn = m_compatibility_api->compare_modules(
            m1.c_str(), m2.c_str(), NULL /*search_paths*/, context.get());

        handle_return_code_and_messages(rtn, context.get());

        return rtn;
    }

    mi::Sint32 compare_archives(std::string & a1, std::string & a2)
    {
        mi::neuraylib::INeuray * neuray(mdlm::neuray());
        mi::base::Handle<mi::neuraylib::IMdl_factory> mdl_factory
        (
            neuray->get_api_component<mi::neuraylib::IMdl_factory>()
        );
        mi::base::Handle<mi::neuraylib::IMdl_execution_context> context(
            mdl_factory->create_execution_context());

        mi::Sint32 rtn = m_compatibility_api->compare_archives(
            a1.c_str(), a2.c_str(), NULL /*search_paths*/, context.get());

        handle_return_code_and_messages(rtn, context.get());

        return rtn;
    }
};

Compatibility::Compatibility(const string & old_archive, const string & new_archive)
    : m_old_archive(Archive::with_extension(old_archive))
    , m_new_archive(Archive::with_extension(new_archive))
    , m_compatible(UNDEFINED)
    , m_test_version_only(false)
{
}

int Compatibility::execute()
{
    m_compatible = UNDEFINED;

    std::string old_archive_path(m_old_archive);
    std::string new_archive_path(m_new_archive);
    if (Util::equivalent(old_archive_path, new_archive_path))
    {
        Util::log_report("compatibility: Same files");
        m_compatible = COMPATIBLE;
        report_compatibility_result();
        return COMPATIBLE;
    }

    Archive oldarch(m_old_archive);
    if (!oldarch.is_valid())
    {
        Util::log_error("Invalid archive: " + Util::normalize(oldarch.full_name()));
        return INVALID_ARCHIVE;
    }
    Util::log_verbose("Archive 1 set: " + Util::normalize(oldarch.full_name()));

    Archive newarch(m_new_archive);
    if (!newarch.is_valid())
    {
        Util::log_error("Invalid archive: " + Util::normalize(newarch.full_name()));
        return INVALID_ARCHIVE;
    }
    Util::log_verbose("Archive 2 set: " + Util::normalize(newarch.full_name()));

    Version oldVersion;
    Version newVersion;
    check_success(0 == oldarch.get_version(oldVersion));
    check_success(0 == newarch.get_version(newVersion));

    Util::log_debug("Test Archive Compatibility");
    Util::log_debug("\t Archive 1 (" + to_string(oldVersion) + "): " + Util::normalize(oldarch.full_name()));
    Util::log_debug("\t Archive 2 (" + to_string(newVersion) + "): " + Util::normalize(newarch.full_name()));

    if (oldarch.base_name() != newarch.base_name())
    {
        Util::log_fatal("Archives are incompatible: different names");
        m_compatible = NOT_COMPATIBLE;
        report_compatibility_result();
        return ARCHIVE_DIFFERENT_NAME;
    }

    Util::log_verbose("Archive 1 version: " + to_string(oldVersion));
    Util::log_verbose("Archive 2 version: " + to_string(newVersion));

    if (oldVersion == newVersion)
    {
        Util::log_debug("Compatible archives: Archives are of the same version: "
            + to_string(newVersion));
        // Note: Even if the version are the same it makes sense to
        // look into each of the archives to check all the modules
        // Therefore continue...
        m_compatible = COMPATIBLE_SAME_VERSION;
    }
    else
    {
        if (newVersion < oldVersion)
        {
            Util::log_debug("Incompatible archives: Archive 1 is newer than archive 2");
            m_compatible = NOT_COMPATIBLE;
            report_compatibility_result();
            return NOT_COMPATIBLE;
        }
        else if (newVersion.major() != oldVersion.major())
        {
            Util::log_debug("Incompatible archives: Major version changed");
            m_compatible = NOT_COMPATIBLE;
            report_compatibility_result();
            return NOT_COMPATIBLE;
        }
        else
        {
            m_compatible = COMPATIBLE;
        }
    }
    if (m_test_version_only)
    {
        // stop here
        report_compatibility_result();
        if (m_compatible == COMPATIBLE || m_compatible == COMPATIBLE_SAME_VERSION)
        {
            return COMPATIBLE;
        }
        return NOT_COMPATIBLE;
    }

    Compatibility_api_helper helper;
    mi::Sint32 rtn = helper.compare_archives(m_old_archive, m_new_archive);

    if (rtn == 0)
    {
        m_compatible = COMPATIBLE;
    }
    else if (rtn == -2)
    {
        m_compatible = NOT_COMPATIBLE;
    }
    else
    {
        m_compatible = UNDEFINED;
    }

    report_compatibility_result();

    if (m_compatible == COMPATIBLE || m_compatible == COMPATIBLE_SAME_VERSION)
    {
        return COMPATIBLE;
    }
    return NOT_COMPATIBLE;
}

void Compatibility::report_compatibility_result() const
{
    // Report the result of the compatibility check
    // This is logged as log info
    Archive oldarch(m_old_archive);
    Archive newarch(m_new_archive);
    string str;
    str = "Compatibility test ";
    str += (m_test_version_only ? "(version only)" : "(full test)");
    Util::log_info(str);
    Util::log_info("New archive: " + Util::normalize(newarch.full_name()));
    if (m_compatible == COMPATIBLE)
    {
        str = "is compatible with old archive: ";
    }
    else if (m_compatible == COMPATIBLE_SAME_VERSION)
    {
        str = "is compatible (same version) with archive: ";
    }
    else if (m_compatible == NOT_COMPATIBLE)
    {
        str = "is not compatible with archive: ";
    }
    else
    {
        str = "Undefined result for compatibility test: ";
    }
    str += Util::normalize(oldarch.full_name());
    Util::log_info(str);
}

Create_archive::Create_archive(const std::string & mdl_directory, const std::string & archive)
    : m_mdl_directory(mdl_directory)
    , m_archive(Archive::with_extension(archive))
{
    // Convert SYSTEM, USER to real directory locations
    Util::File::convert_symbolic_directory(m_mdl_directory);
}

int Create_archive::execute()
{
    mi::base::Handle<mi::neuraylib::IMdl_archive_api> archive_api
    (
        mdlm::neuray()->get_api_component<mi::neuraylib::IMdl_archive_api>()
    );

    /// \param manifest_fields   A static or dynamic array of structs of type \c "Manifest_field"
    ///                          which holds fields with optional or user-defined keys to be added
    ///                          to the manifest. The struct has two members, \c "key" and
    ///                          \c "value", both of type \c "String". \c NULL is treated like an
    ///                          empty array.
    mi::base::Handle<mi::neuraylib::IDatabase> database
    (
        mdlm::neuray()->get_api_component<mi::neuraylib::IDatabase>()
    );
    mi::base::Handle<mi::neuraylib::IScope> scope(database->get_global_scope());
    mi::base::Handle<mi::neuraylib::ITransaction> transaction(scope->create_transaction());
    mi::base::Handle<mi::IDynamic_array> manifest_fields
    (
        transaction->create<mi::IDynamic_array>("Manifest_field[]")
    );
    manifest_fields->set_length(m_keys.size());

    vector<KeyValuePair>::iterator it;
    mi::Size i;
    for (it = m_keys.begin(), i = 0; it != m_keys.end(); it++, i++)
    {
        mi::base::Handle<mi::IStructure> field(manifest_fields->get_value<mi::IStructure>(i));
        mi::base::Handle<mi::IString> key(field->get_value<mi::IString>("key"));
        key->set_c_str(it->first.c_str());
        mi::base::Handle<mi::IString> value(field->get_value<mi::IString>("value"));
        value->set_c_str(it->second.c_str());
    }

    mi::Sint32 rtn = archive_api->create_archive(
          m_mdl_directory.c_str()
        , m_archive.c_str()
        , manifest_fields.get()
    );

    transaction->commit();

    std::map<int, string> errors = 
    { 
          { -1, "Invalid parameters" }
        , { -2, "archive does not end with \""+ Archive::extension +"\"" }
        , { -3, string("An array element of manifest_fields or a struct member ") +
                       "of one of the array elements has an incorrect type." }
        ,{ -4, "Failed to create the archive" }
    };
    if (rtn != 0)
    {
        if (!errors[rtn].empty())
        {
            Util::log_error(errors[rtn]);
        }
    }
    else
    {
        Util::log_info("Archive successfully created: " + m_archive);
    }
    return rtn == 0 ? SUCCESS : UNSPECIFIED_FAILURE;
}

int Create_archive::add_key_value(const std::string & key_value)
{
    size_t pos = key_value.find('=');
    if (pos != std::string::npos)
    {
        KeyValuePair keyvalue(key_value.substr(0, pos), key_value.substr(pos + 1));
        m_keys.push_back(keyvalue);
    }
    else
    {
        Util::log_error(
            "Invalid key/value pair will be ignored (should be key=value): " + key_value);
        return UNSPECIFIED_FAILURE;
    }
    return SUCCESS;
}

Install::Install(const std::string & archive, const std::string & mdl_path)
    : m_archive(Archive::with_extension(archive))
    , m_mdl_directory(mdl_path)
    , m_force(false)
    , m_dryrun(false)
{
    Util::File::convert_symbolic_directory(m_mdl_directory);
}

Compatibility::COMPATIBILITY Install::test_compatibility(const std::string & directory) const
{
    Archive new_archive(m_archive);

    std::string old_archive = Util::path_appends(directory, new_archive.base_name());
    if (Util::file_is_readable(old_archive))
    {
        // Archive already installed in the given directory
        // Invoke compatibility command
        const std::string old_archive_name(old_archive);
        const std::string new_archive_name(new_archive.full_name());
        Compatibility test_compatibilty(old_archive_name, new_archive_name);
        // During installation, we only rely on the archive versions
        test_compatibilty.set_test_version_only(true);
        check_success(test_compatibilty.execute() >= 0);
        Compatibility::COMPATIBILITY compatibility = test_compatibilty.compatibility();

        return compatibility;
    }

    return Compatibility::UNDEFINED;
}

class Compatibility_result
{
public:
    Compatibility_result(const string & archive_file)
        : m_archive(archive_file)
    {
    }
    Archive m_archive;
    Compatibility::COMPATIBILITY m_compatibility;
    typedef enum { higher, lower, same } LEVEL;
    LEVEL m_level;
    string level() const
    {
        return
            (m_level == higher)
            ? ("higher level")
            : ((m_level == lower) ? "lower level" : "same level");
    }
    void report() const
    {
        if (m_compatibility == Compatibility::NOT_COMPATIBLE)
        {
            Util::log_info(
                "Found incompatible archive at " + level() + " : " + Util::normalize(m_archive.full_name()));
        }
        else if (m_compatibility == Compatibility::COMPATIBLE)
        {
            Util::log_info(
                "Found compatible archive at " + level() + " : " + Util::normalize(m_archive.full_name()));
        }
        else if (m_compatibility == Compatibility::COMPATIBLE_SAME_VERSION)
        {
            Util::log_info(
                "Found same version of the archive at " + level() + " : " + Util::normalize(m_archive.full_name()));
        }
        else
        {
            Util::log_info(
                "Did not find archive at " + level() + " : " + Util::normalize(m_archive.full_name()));
        }
    }
};

class Compatibility_result_vector : public vector<Compatibility_result>
{
    bool m_can_install = true;
public:
    void analyze_results()
    {
        // TODO: Replace with a map of:
        // typedef std::pair<Compatibility_result::LEVEL, Compatibility::COMPATIBILITY> KEY;
        // std::map<KEY, bool> comp;
        // comp[KEY(HIGHER, COMPATIBLE)] = false;
        // ...

        // Assume we can install the archive, e.g. the list can be empty
        m_can_install = true;
        const_iterator it;
        for (it = begin(); it != end() && m_can_install; it++)
        {
            it->report();

            if (it->m_level == Compatibility_result::higher)
            {
                // Higher priority path
                // --------------------
                // - Compatible versions: 
                //      "Earlier version of the archive is installed at a higher priority path
                //      No install / error"	
                //  - Identical versions
                //      "This version of the archive is installed at a higher priority path
                //      No install / warning"
                // - Incompatible versions
                //      "More recent version of the archive is installed at a higher priority path
                //      No install / warning"	
                // - No archive installed
                //      Install
                //
                switch (it->m_compatibility)
                {
                case Compatibility::COMPATIBLE:
                    m_can_install = false;
                    Util::log_error(
                        "Earlier version of the archive is installed at a higher priority path:");
                    Util::log_error(Util::normalize(it->m_archive.full_name()));
                    break;
                case Compatibility::COMPATIBLE_SAME_VERSION:
                    m_can_install = false;
                    Util::log_warning(
                        "This version of the archive is installed at a higher priority path:");
                    Util::log_warning(Util::normalize(it->m_archive.full_name()));
                    break;
                case Compatibility::NOT_COMPATIBLE:
                    m_can_install = false;
                    Util::log_warning(
                        string("More recent version of the archive is installed at a ") +
                        "higher priority path:");
                    Util::log_warning(Util::normalize(it->m_archive.full_name()));
                    break;
                case Compatibility::UNDEFINED:
                    m_can_install &= true;
                    break;
                default:
                    break;
                }
            }
            else if (it->m_level == Compatibility_result::same)
            {
                // Destination path
                // --------------------
                // - Compatible versions: 
                //      "Earlier version of the archive is installed in the destination directory
                //      Install / fine"	
                //  - Identical versions
                //      "This version of the archive is installed in the destination directory
                //      No install / warning"
                // - Incompatible versions
                //      "More recent version of the archive is installed in the destination
                //       directory
                //      No install / warning"	
                // - No archive installed
                //      Install
                switch (it->m_compatibility)
                {
                case Compatibility::COMPATIBLE:
                    m_can_install &= true;
                    Util::log_info(
                        "Earlier version of the archive is installed "
                        "in the destination directory:");
                    Util::log_info(Util::normalize(it->m_archive.full_name()));
                    break;
                case Compatibility::COMPATIBLE_SAME_VERSION:
                    m_can_install = false;
                    Util::log_warning(
                        "This version of the archive is installed in the destination directory:");
                    Util::log_warning(Util::normalize(it->m_archive.full_name()));
                    break;
                case Compatibility::NOT_COMPATIBLE:
                    m_can_install = false;
                    Util::log_warning(
                        string("Incompatible version of the archive is installed ") +
                        "in the destination directory:");
                    Util::log_warning(Util::normalize(it->m_archive.full_name()));
                    break;
                case Compatibility::UNDEFINED:
                    m_can_install &= true;
                    break;
                default:
                    break;
                }
            }
            else if (it->m_level == Compatibility_result::lower)
            {
                // Lower priority path
                // --------------------
                // - Compatible versions: 
                //      "Earlier version of the archive is installed at lower priority path.
                //      This will be shadowed by the new archive
                //      No install / warning"
                //  - Identical versions
                //      "This version of the archive is installed at a lower priority path
                //      No install / warning"
                // - Incompatible versions
                //      "More recent version of the archive is installed at a lower priority path
                //      No install / warning"
                // - No archive installed
                //      Install
                switch (it->m_compatibility)
                {
                case Compatibility::COMPATIBLE:
                    m_can_install &= false;
                    Util::log_warning(
                        "Earlier version of the archive is installed at lower priority path:");
                    Util::log_warning(Util::normalize(it->m_archive.full_name()));
                    Util::log_warning("This will be shadowed by the new archive.");
                    break;
                case Compatibility::COMPATIBLE_SAME_VERSION:
                    m_can_install &= false;
                    Util::log_warning(
                        "This version of the archive is installed at a lower priority path:");
                    Util::log_warning(Util::normalize(it->m_archive.full_name()));
                    break;
                case Compatibility::NOT_COMPATIBLE:
                    m_can_install = false;
                    Util::log_warning(
                        string("More recent version of the archive is installed ") +
                        "at a lower priority path:");
                    Util::log_warning(Util::normalize(it->m_archive.full_name()));
                    break;
                case Compatibility::UNDEFINED:
                    m_can_install &= true;
                    break;
                default:
                    break;
                }
            }
        }
    }
    bool can_install()
    {
        return m_can_install;
    }
};

int Install::execute()
{
    Util::log_info("Install Archive");
    Util::log_info("\tArchive: " + Util::normalize(m_archive));
    Util::log_info("\tMDL Path: " + Util::normalize(m_mdl_directory));

    // Input archive
    Archive new_archive(m_archive);
    {
        string src(m_archive);
        string dest(Util::path_appends(m_mdl_directory, new_archive.base_name()));
        // Ensure src and target are not the same
        if (Util::equivalent(src, dest))
        {
            Util::log_error("Archive not installed, same source and target");
            Util::log_error("\tArchive source: " + Util::normalize(src));
            Util::log_error("\tArchive target: " + Util::normalize(dest));
            return SUCCESS;
        }
    }
    {
        // Test input archive
        if (!new_archive.is_valid())
        {
            Util::log_error("Invalid archive: " + m_archive);
            return INVALID_ARCHIVE;
        }
        Util::log_info("Begin instalation of archive: " + Util::normalize(m_archive));
        Util::log_info("Destination directory: " + Util::normalize(m_mdl_directory));
    }
    {
        // Test destination directory
        if (!Util::directory_is_writable(m_mdl_directory))
        {
            Util::log_error("Invalid installation directory: " + Util::normalize(m_mdl_directory));
            return INVALID_MDL_PATH;
        }
    }
    bool do_install = true;

    // Check possible conflicts with installed packages, modules, archives
    if (do_install)
    {
        if (new_archive.conflict(m_mdl_directory))
        {
            Util::log_warning("Archive conflict detected");
            Util::log_warning("\tArchive: " + Util::normalize(m_archive));
            Util::log_warning("\tInstall path: " + Util::normalize(m_mdl_directory));
            do_install = false;

            mdlm::report("Conflict found in target directory");
        }
        else
        {
            Util::log_info("Archive conflict test passed");
        }
    }

    // Check conflicts with same installed archive in any MDL search root
    if (do_install)
    {
        // Add the target MDL path to the MDL roots if necessary
        Search_path sp(mdlm::neuray());
        sp.snapshot();
        if (!sp.find_module_path(m_mdl_directory))
        {
            sp.add_module_path(m_mdl_directory);
            sp.snapshot();
        }

        // Test whether an archive with same name is already installed:
        //      - in the target search root?
        //      - at a higher prioritized search root?
        //      - at a lower prioritized search root?
        std::string install_path(m_mdl_directory);
        vector<string>::const_iterator it;
        Compatibility_result::LEVEL level = Compatibility_result::higher;
        Compatibility_result_vector compatibility_list;
        for (it = sp.paths().begin(); it != sp.paths().end(); it++)
        {
            std::string current(*it);

            // Test compatibility
            const Compatibility::COMPATIBILITY return_code =
                test_compatibility(current);

            std::string archive_dest = Util::path_appends(current, new_archive.base_name());
            Compatibility_result comp(archive_dest);
            comp.m_compatibility = return_code;
            if (Util::equivalent(current, install_path))
            {
                comp.m_level = Compatibility_result::same;
                level = Compatibility_result::lower;// switch to lower level
            }
            else
            {
                comp.m_level = level;
            }

            if (return_code != Compatibility::UNDEFINED)
            {
                compatibility_list.push_back(comp);
            }
        }

        // Analyze the results
        compatibility_list.analyze_results();

        bool can_install = compatibility_list.can_install();

        if (!can_install)
        {
            mdlm::report("Conflict archive found in MDL search path");
        }

        do_install &= can_install;
    }

    // Check dependencies
    if (do_install)
    {
        do_install &= new_archive.all_dependencies_are_installed();
    }

    if (!do_install)
    {
        if (m_force)
        {
            mdlm::report(
                "Installation proceed due to -f|--force");
            do_install = true;
        }
        else
        {
            mdlm::report(
                "Installation canceled (use -f|--force to force)");
        }
    }
    if (do_install)
    {
        std::string archive_src = new_archive.full_name();
        std::string archive_dest = Util::path_appends(m_mdl_directory, new_archive.base_name());
        Util::log_info("Archive ready to be installed");
        Util::log_info("\tArchive: " + Util::normalize(m_archive));
        Util::log_info("\tMDL Path: " + Util::normalize(m_mdl_directory));
        if (m_dryrun)
        {
            mdlm::report("Archive not installed due to --dry-run option");
        }
        else
        {
            if (Util::copy_file(archive_src, archive_dest))
            {
                mdlm::report("Archive " + new_archive.base_name()
                    + " successfully installed in directory: " + Util::normalize(m_mdl_directory));
            }
            else
            {
                mdlm::report("Archive not installed due to unknown error");
            }
        }
    }

    return SUCCESS;
}

Show_archive::Show_archive(const std::string & archive)
    : m_archive(Archive::with_extension(archive))
{}

int Show_archive::execute()
{
    Archive archive(m_archive);
    if (!archive.is_valid())
    {
        Util::log_error("Invalid archive");
        return INVALID_ARCHIVE;
    }
    Util::log_verbose("Show archive: " + Util::normalize(archive.full_name()));
    Util::log_verbose("Archives set: " + Util::normalize(archive.full_name()));

    mi::base::Handle<mi::neuraylib::IMdl_archive_api>
        archive_api(mdlm::neuray()->get_api_component<mi::neuraylib::IMdl_archive_api>());

    mi::base::Handle<const mi::neuraylib::IManifest> manifest(
        archive_api->get_manifest(m_archive.c_str()));

    check_success(manifest.is_valid_interface());

    for (mi::Size i = 0; i < manifest->get_number_of_fields(); i++)
    {
        const char* key = manifest->get_key(i);
        if (list_field(key))
        {
            const char* value = manifest->get_value(i);
            mdlm::report(key + string(" = \"") + value + string("\""));
        }
    }

    return SUCCESS;
}

List_dependencies::List_dependencies(const std::string & archive)
    : Show_archive(Archive::with_extension(archive))
{
    add_filter_field("dependency");
}

Extract::Extract(const std::string & archive, const std::string & path)
    : m_archive(Archive::with_extension(archive))
    , m_directory(path)
    , m_force(false)
{}

int Extract::execute()
{
    Archive archive(m_archive);
    if (!archive.is_valid())
    {
        Util::log_error("Invalid archive");
        return INVALID_ARCHIVE;
    }

    bool proceed(true);
    Util::File folder(m_directory);
    if (folder.is_directory())
    {
        // Folder exists
        if (! folder.is_empty())
        {
            Util::log_warning("Directory is not empty");
            proceed = false;
        }
    }
    else
    {
        // create folder
        if (!Util::create_directory(m_directory))
        {
            Util::log_error("Can not create directory: " + m_directory);
            return CAN_NOT_CREATE_PATH;
        }
    }
    if (m_force)
    {
        proceed = true;
    }
    if (proceed)
    {
        if (archive.extract_to_directory(m_directory) == 0)
        {
            Util::log_report("Archive extracted");
        }
    }
    return SUCCESS;
}

int Help::execute()
{
    MDLM_option_parser parser;
    parser.output_usage(cout);

    Option_set options = parser.get_known_options().find_options_from_name(m_command);
    if (options.empty())
    {
        Util::log_error("Invalid command: " + m_command);
        return -1;
    }

    Option_set::iterator it;
    for (it = options.begin(); it != options.end(); it++)
    {
        it->output_usage(cout);
    }
    return 0;
}

List::List(const std::string & archive)
    : m_archive_name(Archive::with_extension(archive))
{}

int List::execute()
{
    // Initialize
    m_result = List_result();

    // 
    if (m_archive_name.empty())
    {
        // List all archives installed
        Util::log_warning("TODO Implement...");
        return 0;
    }

    Search_path sp(mdlm::neuray());
    sp.snapshot();
    bool found(false);
    for (auto& directory : sp.paths())
    {
        std::string archive = Util::path_appends(directory, m_archive_name);
        Util::File file(archive);

        if (file.exist())
        {
            string archive_file_name = archive;
            if (found == true)
            {
                // Already found one installed archive report a warning
                Util::log_warning("Found shadowed archive (inconsistent installation): " + 
                    Util::normalize(archive_file_name));
            }
            found = true;
            m_result.m_archives.push_back(Archive(archive_file_name));
            Util::log_verbose("Found archive: " + Util::normalize(archive_file_name));

            Util::log_report("Found archive: " + Util::normalize(archive_file_name));
        }
    }
    return 0;
}

Command * Command_factory::build_command(const Option_set & option)
{
    Option_set_type::const_iterator it;
    for (it = option.begin(); it != option.end(); it++)
    {
        if (it->is_valid() && it->get_is_command())
        {
            // Check the number of arguments for the command
            if (it->get_number_of_parameters() != it->value().size())
            {
                break;
            }

            if (it->id() == MDLM_option_parser::COMPATIBILITY)
            {
                return new Compatibility(it->value()[0], it->value()[1]);
            }
            else if (it->id() == MDLM_option_parser::HELP_CMD)
            {
                return new Help(it->value()[0]);
            }
            else if (it->id() == MDLM_option_parser::LIST)
            {
                return new List(it->value()[0]);
            }
            else if (it->id() == MDLM_option_parser::LIST_ALL)
            {
                return new List("");
            }
            else if (it->id() == MDLM_option_parser::INSTALL_ARCHIVE)
            {
                string archive = it->value()[0];
                string directory = it->value()[1];
                Install * cmd = new Install(archive, directory);
                Option_parser * command_options = it->get_options();
                if (command_options)
                {
                    Option_set dummy;
                    if (command_options->is_set(MDLM_option_parser::FORCE, dummy))
                    {
                        cmd->set_force_installation(true);
                    }
                    if (command_options->is_set(MDLM_option_parser::DRY_RUN, dummy))
                    {
                        cmd->set_dryrun(true);
                    }
                }
                return cmd;
            }
            else if (it->id() == MDLM_option_parser::CREATE_ARCHIVE)
            {
                Create_archive * create = new Create_archive(it->value()[0], it->value()[1]);
                Option_parser * command_options = it->get_options();
                if (command_options)
                {
                    Option_set setkey;
                    if (command_options->is_set(MDLM_option_parser::SET_KEY, setkey))
                    {
                        Option opt;
                        Option_set_type::iterator it;
                        for (it = setkey.begin(); it < setkey.end(); it++)
                        {
                            opt = (*it);
                            if (!it->value().empty())
                            {
                                create->add_key_value(it->value()[0]);
                            }
                        }
                    }
                }
                return create;
            }
            else if (it->id() == MDLM_option_parser::SHOW_ARCHIVE)
            {
                return new Show_archive(it->value()[0]);
            }
            else if (it->id() == MDLM_option_parser::DEPENDS)
            {
                return new List_dependencies(it->value()[0]);
            }
            else if (it->id() == MDLM_option_parser::EXTRACT)
            {
                string archive = it->value()[0];
                string directory = it->value()[1];
                Extract * cmd = new Extract(archive, directory);
                Option_parser * command_options = it->get_options();
                if (command_options)
                {
                    Option_set dummy;
                    if (command_options->is_set(MDLM_option_parser::FORCE, dummy))
                    {
                        cmd->set_force_extract(true);
                    }
                }
                return cmd;

            }
            else if (it->id() == MDLM_option_parser::REMOVE)
            {
                Util::log_warning("Command not implemented...");
            }
        }
    }

    return NULL;
}
