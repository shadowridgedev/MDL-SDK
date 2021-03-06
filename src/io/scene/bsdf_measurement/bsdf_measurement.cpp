/***************************************************************************************************
 * Copyright (c) 2013-2018, NVIDIA CORPORATION. All rights reserved.
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
 **************************************************************************************************/

#include "pch.h"

#include "i_bsdf_measurement.h"

#include <sstream>

#include <mi/neuraylib/bsdf_isotropic_data.h>
#include <mi/neuraylib/ibsdf_isotropic_data.h>
#include <mi/neuraylib/ireader.h>
#include <base/system/main/access_module.h>
#include <base/hal/disk/disk.h>
#include <base/hal/disk/disk_file_reader_writer_impl.h>
#include <base/hal/hal/i_hal_ospath.h>
#include <base/lib/config/config.h>
#include <base/lib/log/i_log_logger.h>
#include <base/lib/log/i_log_assert.h>
#include <base/lib/path/i_path.h>
#include <base/data/serial/i_serializer.h>
#include <base/data/db/i_db_access.h>
#include <base/data/db/i_db_transaction.h>
#include <base/util/registry/i_config_registry.h>
#include <io/scene/scene/i_scene_journal_types.h>
#include <io/scene/mdl_elements/mdl_elements_detail.h>

namespace MI {

namespace BSDFM {

static const char* magic_header = "NVIDIA ARC MBSDF V1\n";
static const char* magic_data = "MBSDF_DATA=\n";
static const char* magic_reflection = "MBSDF_DATA_REFLECTION=\n";
static const char* magic_transmission = "MBSDF_DATA_TRANSMISSION=\n";

Bsdf_measurement::Bsdf_measurement()
{
}

Bsdf_measurement::Bsdf_measurement( const Bsdf_measurement& other)
  : SCENE::Scene_element<Bsdf_measurement, ID_BSDF_MEASUREMENT>( other)
{
    m_reflection = other.m_reflection;
    m_transmission = other.m_transmission;
    m_original_filename = other.m_original_filename;
    m_resolved_filename = other.m_resolved_filename;
    m_resolved_archive_filename = other.m_resolved_archive_filename;
    m_resolved_archive_membername = other.m_resolved_archive_membername;
    m_mdl_file_path = other.m_mdl_file_path;
}

Bsdf_measurement::~Bsdf_measurement()
{
}

mi::Sint32 Bsdf_measurement::reset_file( const std::string& original_filename)
{
    SYSTEM::Access_module<PATH::Path_module> m_path_module( false);
    std::string resolved_filename
        = m_path_module->search( PATH::RESOURCE, original_filename.c_str());
#if 0
    LOG::mod_log->info( M_SCENE, LOG::Mod_log::C_IO,
        "Configured resource search path:");
    const std::vector<std::string>& search_path
        = m_path_module->get_search_path( PATH::RESOURCE);
    for( mi::Size i = 0; i < search_path.size(); ++i)
         LOG::mod_log->info( M_SCENE, LOG::Mod_log::C_IO,
             "  [%" FMT_MI_SIZE "]: %s", i, search_path[i].c_str());
    LOG::mod_log->info( M_SCENE, LOG::Mod_log::C_IO,
        "Request: %s", original_filename.c_str());
    LOG::mod_log->info( M_SCENE, LOG::Mod_log::C_IO,
        "Result: %s", resolved_filename.empty() ? "(failed)" : resolved_filename.c_str());
#endif

    if( resolved_filename.empty())
        return -2;

    mi::neuraylib::IBsdf_isotropic_data* reflection = 0;
    mi::neuraylib::IBsdf_isotropic_data* transmission = 0;
    bool success = import_from_file( resolved_filename, reflection, transmission);
    if( !success)
        return -3;

    m_reflection = reflection;
    m_transmission = transmission;

    m_original_filename = original_filename;
    m_resolved_filename = resolved_filename;
    m_resolved_archive_filename.clear();
    m_resolved_archive_membername.clear();
    m_mdl_file_path.clear();

    std::ostringstream s;
    s << "Loading BSDF measurement \"" << m_resolved_filename.c_str()
      << "\", reflection: " << dump_data( m_reflection.get())
      << ", transmission: " << dump_data( m_transmission.get());
    LOG::mod_log->info( M_SCENE, LOG::Mod_log::C_IO, "%s", s.str().c_str());

    return 0;
}

mi::Sint32 Bsdf_measurement::reset_reader( mi::neuraylib::IReader* reader)
{
    mi::neuraylib::IBsdf_isotropic_data* reflection = 0;
    mi::neuraylib::IBsdf_isotropic_data* transmission = 0;
    bool success = import_from_reader( reader, reflection, transmission);
    if( !success)
        return -3;

    m_reflection = reflection;
    m_transmission = transmission;

    m_original_filename.clear();
    m_resolved_filename.clear();
    m_resolved_archive_filename.clear();
    m_resolved_archive_membername.clear();
    m_mdl_file_path.clear();

    std::ostringstream s;
    s << "Loading memory-based BSDF measurement"
      << ", reflection: " << dump_data( m_reflection.get())
      << ", transmission: " << dump_data( m_transmission.get());
    LOG::mod_log->info( M_SCENE, LOG::Mod_log::C_IO, "%s", s.str().c_str());

    return 0;
}

mi::Sint32 Bsdf_measurement::reset_file_mdl(
    const std::string& resolved_filename, const std::string& mdl_file_path)
{
    mi::neuraylib::IBsdf_isotropic_data* reflection = 0;
    mi::neuraylib::IBsdf_isotropic_data* transmission = 0;
    bool success = import_from_file( resolved_filename, reflection, transmission);
    if( !success)
        return -3;

    m_reflection = reflection;
    m_transmission = transmission;

    m_original_filename.clear();
    m_resolved_filename = resolved_filename;
    m_resolved_archive_filename.clear();
    m_resolved_archive_membername.clear();
    m_mdl_file_path = mdl_file_path;

    std::ostringstream s;
    s << "Loading BSDF measurement \"" << m_resolved_filename.c_str()
      << "\", reflection: " << dump_data( m_reflection.get())
      << ", transmission: " << dump_data( m_transmission.get());
    LOG::mod_log->info( M_SCENE, LOG::Mod_log::C_IO, "%s", s.str().c_str());

    return 0;
}

mi::Sint32 Bsdf_measurement::reset_archive_mdl(
    mi::neuraylib::IReader* reader,
    const std::string& archive_filename,
    const std::string& archive_membername,
    const std::string& mdl_file_path)
{
    mi::neuraylib::IBsdf_isotropic_data* reflection = 0;
    mi::neuraylib::IBsdf_isotropic_data* transmission = 0;
    bool success = import_from_reader(
        reader, archive_filename, archive_membername, reflection, transmission);
    if( !success)
        return -3;

    m_reflection = reflection;
    m_transmission = transmission;

    m_original_filename.clear();
    m_resolved_filename.clear();
    m_resolved_archive_filename = archive_filename;
    m_resolved_archive_membername = archive_membername;
    m_mdl_file_path = mdl_file_path;

    std::ostringstream s;
    s << "Loading BSDF measurement \"" << archive_membername
      << "\" in \"" << archive_filename
      << "\", reflection: " << dump_data( m_reflection.get())
      << ", transmission: " << dump_data( m_transmission.get());
    LOG::mod_log->info( M_SCENE, LOG::Mod_log::C_IO, "%s", s.str().c_str());

    return 0;
}

void Bsdf_measurement::set_reflection( const mi::neuraylib::IBsdf_isotropic_data* bsdf_data)
{
    m_original_filename.clear();
    m_resolved_filename.clear();
    m_resolved_archive_filename.clear();
    m_resolved_archive_membername.clear();
    m_mdl_file_path.clear();
    m_reflection = make_handle_dup( bsdf_data);
}

const mi::base::IInterface* Bsdf_measurement::get_reflection() const
{
    if( !m_reflection)
        return 0;
    m_reflection->retain();
    return m_reflection.get();
}

void Bsdf_measurement::set_transmission( const mi::neuraylib::IBsdf_isotropic_data* bsdf_data)
{
    m_original_filename.clear();
    m_resolved_filename.clear();
    m_resolved_archive_filename.clear();
    m_resolved_archive_membername.clear();
    m_mdl_file_path.clear();
    m_transmission = make_handle_dup( bsdf_data);
}

const mi::base::IInterface* Bsdf_measurement::get_transmission() const
{
    if( !m_transmission)
        return 0;
    m_transmission->retain();
    return m_transmission.get();
}

const std::string& Bsdf_measurement::get_filename() const
{
    return m_resolved_filename;
}

const std::string& Bsdf_measurement::get_original_filename() const
{
    if( m_original_filename.empty() && !m_mdl_file_path.empty()) {
        SYSTEM::Access_module<CONFIG::Config_module> config_module( false);
        const CONFIG::Config_registry& registry = config_module->get_configuration();
        bool flag = false;
        registry.get_value( "deprecated_mdl_file_path_as_original_filename", flag);
        if( flag)
            return m_mdl_file_path;
    }

    return m_original_filename;
}

const std::string& Bsdf_measurement::get_mdl_file_path() const
{
    return m_mdl_file_path;
}

const SERIAL::Serializable* Bsdf_measurement::serialize( SERIAL::Serializer* serializer) const
{
    Scene_element_base::serialize( serializer);

    serializer->write( serializer->is_remote() ? "" : m_original_filename);
    serializer->write( serializer->is_remote() ? "" : m_resolved_filename);
    serializer->write( serializer->is_remote() ? "" : m_resolved_archive_filename);
    serializer->write( serializer->is_remote() ? "" : m_resolved_archive_membername);
    serializer->write( serializer->is_remote() ? "" : m_mdl_file_path);
    serializer->write( HAL::Ospath::sep());

    serialize_bsdf_data( serializer, m_reflection.get());
    serialize_bsdf_data( serializer, m_transmission.get());

    return this + 1;
}

SERIAL::Serializable* Bsdf_measurement::deserialize( SERIAL::Deserializer* deserializer)
{
    Scene_element_base::deserialize( deserializer);

    deserializer->read( &m_original_filename);
    deserializer->read( &m_resolved_filename);
    deserializer->read( &m_resolved_archive_filename);
    deserializer->read( &m_resolved_archive_membername);
    deserializer->read( &m_mdl_file_path);
    std::string serializer_sep;
    deserializer->read( &serializer_sep);

    m_reflection = deserialize_bsdf_data( deserializer);
    m_transmission = deserialize_bsdf_data( deserializer); //-V656 PVS

    // Adjust m_original_filename and m_resolved_filename for this host.
    if( !m_original_filename.empty()) {

        if( serializer_sep != HAL::Ospath::sep()) {
            m_original_filename
                = HAL::Ospath::convert_to_platform_specific_path( m_original_filename);
            m_resolved_filename
                = HAL::Ospath::convert_to_platform_specific_path( m_resolved_filename);
        }

        // Re-resolve filename if it is not meaningful for this host. If unsuccessful, clear value
        // (no error since we no longer require all resources to be present on all nodes).
        if( !DISK::is_file( m_resolved_filename.c_str())) {
            SYSTEM::Access_module<PATH::Path_module> m_path_module( false);
            m_resolved_filename = m_path_module->search( PATH::MDL, m_original_filename.c_str());
            if( m_resolved_filename.empty())
                m_resolved_filename = m_path_module->search(
                    PATH::RESOURCE, m_original_filename.c_str());
        }
    }

    // Adjust m_resolved_archive_filename and m_resolved_archive_membername for this host.
    if( !m_resolved_archive_filename.empty()) {

        m_resolved_archive_filename
            = HAL::Ospath::convert_to_platform_specific_path( m_resolved_archive_filename);
        m_resolved_archive_membername
            = HAL::Ospath::convert_to_platform_specific_path( m_resolved_archive_membername);
        if( !DISK::is_file( m_resolved_archive_filename.c_str())) {
            m_resolved_archive_filename.clear();
            m_resolved_archive_membername.clear();
        }

    } else
        ASSERT( M_SCENE, m_resolved_archive_membername.empty());

    return this + 1;
}

void Bsdf_measurement::dump() const
{
    std::ostringstream s;

    s << "Original filename: " << m_original_filename << std::endl;
    s << "Resolved filename: " << m_resolved_filename << std::endl;
    s << "Resolved archive filename: " << m_resolved_archive_filename << std::endl;
    s << "Resolved archive membername: " << m_resolved_archive_membername << std::endl;
    s << "MDL file path: " << m_mdl_file_path << std::endl;

    s << "Reflection: " << dump_data( m_reflection.get()) << std::endl;
    s << "Transmission: " << dump_data( m_transmission.get()) << std::endl;

    LOG::mod_log->info( M_SCENE, LOG::Mod_log::C_DATABASE, "%s", s.str().c_str());
}

size_t Bsdf_measurement::get_size() const
{
    size_t result = sizeof( *this)
        + dynamic_memory_consumption( m_original_filename)
        + dynamic_memory_consumption( m_resolved_filename)
        + dynamic_memory_consumption( m_resolved_archive_filename)
        + dynamic_memory_consumption( m_resolved_archive_membername)
        + dynamic_memory_consumption( m_mdl_file_path);

    // For memory-based BSDF measurements we do not include the actual data here since it is not
    // clear whether it should be counted or not (data exclusively owned by us or not).
    if( is_memory_based())
        return result;

    result += sizeof( mi::neuraylib::Bsdf_isotropic_data) + sizeof( mi::neuraylib::Bsdf_buffer);

    mi::Uint32 resolution_theta;
    mi::Uint32 resolution_phi;
    mi::neuraylib::Bsdf_type type;
    mi::Size size;

    resolution_theta = m_reflection ? m_reflection->get_resolution_theta() : 0;
    resolution_phi   = m_reflection ? m_reflection->get_resolution_phi()   : 0;
    type             = m_reflection ? m_reflection->get_type() : mi::neuraylib::BSDF_SCALAR;
    size             = resolution_theta * resolution_theta * resolution_phi;
    if( type == mi::neuraylib::BSDF_RGB)
        size *= 3;
    result += size * sizeof( mi::Float32);

    resolution_theta = m_transmission ? m_transmission->get_resolution_theta() : 0;
    resolution_phi   = m_transmission ? m_transmission->get_resolution_phi()   : 0;
    type             = m_transmission ? m_transmission->get_type() : mi::neuraylib::BSDF_SCALAR;
    size             = resolution_theta * resolution_theta * resolution_phi;
    if( type == mi::neuraylib::BSDF_RGB)
        size *= 3;
    result += size * sizeof( mi::Float32);

    return result;
}

DB::Journal_type Bsdf_measurement::get_journal_flags() const
{
    return DB::Journal_type(
        SCENE::JOURNAL_CHANGE_FIELD.get_type() |
        SCENE::JOURNAL_CHANGE_SHADER_ATTRIBUTE.get_type());
}

Uint Bsdf_measurement::bundle( DB::Tag* results, Uint size) const
{
    return 0;
}

void Bsdf_measurement::get_scene_element_references( DB::Tag_set* result) const
{
}

std::string Bsdf_measurement::dump_data( const mi::neuraylib::IBsdf_isotropic_data* data)
{
    if( data) {
        std::ostringstream s;
        s << "type \"" << (data->get_type() == mi::neuraylib::BSDF_SCALAR ? "Scalar" : "Rgb");
        s << "\", res. theta " << data->get_resolution_theta();
        s << ", res. phi " << data->get_resolution_phi();
        return s.str();
    } else
        return "(none)";
}

void Bsdf_measurement::serialize_bsdf_data(
    SERIAL::Serializer* serializer, const mi::neuraylib::IBsdf_isotropic_data* bsdf_data)
{
    bool exists = bsdf_data != 0;
    serializer->write( exists);
    if( !exists)
        return;

    mi::Uint32 resolution_theta   = bsdf_data->get_resolution_theta();
    mi::Uint32 resolution_phi     = bsdf_data->get_resolution_phi();
    mi::neuraylib::Bsdf_type type = bsdf_data->get_type();

    serializer->write( resolution_theta);
    serializer->write( resolution_phi);
    serializer->write( static_cast<mi::Uint32>( type));

    mi::Size size = resolution_theta * resolution_theta * resolution_phi;
    if( type == mi::neuraylib::BSDF_RGB)
        size *= 3;
    mi::base::Handle<const mi::neuraylib::IBsdf_buffer> buffer( bsdf_data->get_bsdf_buffer());
    const mi::Float32* data = buffer->get_data();
    serializer->write( reinterpret_cast<const char*>( data), size * sizeof( mi::Float32));
}

mi::neuraylib::IBsdf_isotropic_data* Bsdf_measurement::deserialize_bsdf_data(
    SERIAL::Deserializer* deserializer)
{
    bool exists;
    deserializer->read( &exists);
    if( !exists)
        return 0;

    mi::Uint32 resolution_theta;
    mi::Uint32 resolution_phi;
    mi::Uint32 type;

    deserializer->read( &resolution_theta);
    deserializer->read( &resolution_phi);
    deserializer->read( &type);

    mi::neuraylib::Bsdf_isotropic_data* bsdf_data = new mi::neuraylib::Bsdf_isotropic_data(
        resolution_theta, resolution_phi, static_cast<mi::neuraylib::Bsdf_type>( type));
    mi::base::Handle<mi::neuraylib::Bsdf_buffer> bsdf_buffer( bsdf_data->get_bsdf_buffer());
    mi::Float32* data = bsdf_buffer->get_data();

    mi::Size size = resolution_theta * resolution_theta * resolution_phi;
    if( type == mi::neuraylib::BSDF_RGB)
        size *= 3;
    deserializer->read( reinterpret_cast<char*>( data), size * sizeof( mi::Float32));

    return bsdf_data;
}

namespace {

bool read( mi::neuraylib::IReader* reader, char* buffer, Sint64 size)
{
    return reader->read( buffer, size) == size;
}

bool import_data_from_reader(
    mi::neuraylib::IReader* reader, mi::neuraylib::IBsdf_isotropic_data*& bsdf_data_out)
{
    // avoid unclear reference counting semantics
    if( bsdf_data_out)
        return false;

    // type
    mi::Uint32 type_uint32;
    if( !read( reader, reinterpret_cast<char*>( &type_uint32), sizeof( mi::Uint32)))
        return 0;
    if( type_uint32 > 1)
        return false;
    mi::neuraylib::Bsdf_type type
        = type_uint32 == 0 ? mi::neuraylib::BSDF_SCALAR : mi::neuraylib::BSDF_RGB;

    // resolution_theta
    mi::Uint32 resolution_theta;
    if( !read( reader, reinterpret_cast<char*>( &resolution_theta), sizeof( mi::Uint32)))
        return false;
    if( resolution_theta == 0)
        return false;

    // resolution_phi
    mi::Uint32 resolution_phi;
    if( !read( reader, reinterpret_cast<char*>( &resolution_phi), sizeof( mi::Uint32)))
        return false;
    if( resolution_phi == 0)
        return false;

    // data
    mi::base::Handle<mi::neuraylib::Bsdf_isotropic_data> bsdf_data(
        new mi::neuraylib::Bsdf_isotropic_data( resolution_theta, resolution_phi, type));
    mi::base::Handle<mi::neuraylib::Bsdf_buffer> bsdf_buffer( bsdf_data->get_bsdf_buffer());
    mi::Float32* data = bsdf_buffer->get_data();
    mi::Size size = resolution_theta * resolution_theta * resolution_phi;
    if( type == mi::neuraylib::BSDF_RGB)
        size *= 3;
    if( !read( reader, reinterpret_cast<char*>( data), size * sizeof( mi::Float32)))
        return false;

    bsdf_data->retain();
    bsdf_data_out = bsdf_data.get();
    return true;
}

bool import_measurement_from_reader(
    mi::neuraylib::IReader* reader,
    mi::neuraylib::IBsdf_isotropic_data*& reflection,
    mi::neuraylib::IBsdf_isotropic_data*& transmission)
{
    // avoid unclear reference counting semantics
    if( reflection || transmission)
       return false;

    char buffer[1024];
    std::string buffer_str;

    // header
    if( !reader->readline( &buffer[0], sizeof( buffer)))
        return false;
    buffer_str = buffer;
    if( buffer_str != magic_header)
        return false;

    // skip metadata
    do {
        if( !reader->readline( &buffer[0], sizeof( buffer)))
            return false;
        if( reader->eof())
            return true;
        buffer_str = buffer;
    } while(    buffer_str != magic_data
             && buffer_str != magic_reflection
             && buffer_str != magic_transmission);

    // reflection data
    if( buffer_str == magic_data || buffer_str == magic_reflection) {
        bool success = import_data_from_reader( reader, reflection);
        if( !success)
            return false;
        if( !reader->readline( &buffer[0], sizeof( buffer)))
            return false;
        if( reader->eof())
            return true;
        buffer_str = buffer;
    }

    // transmission data
    if( buffer_str == buffer) {
        bool success = import_data_from_reader( reader, transmission);
        if( !success) {
            if( reflection) {
                reflection->release();
                reflection = 0;
            }
            return false;
        }
        if( !reader->readline( &buffer[0], sizeof( buffer)))
            return false;
        if( reader->eof())
            return true;
        buffer_str = buffer;
    }

    return false;
}

} // namespace

bool import_from_file(
    const std::string& filename,
    mi::neuraylib::IBsdf_isotropic_data*& reflection,
    mi::neuraylib::IBsdf_isotropic_data*& transmission)
{
    DISK::File_reader_impl reader;
    if( !reader.open( filename.c_str()))
        return false;

    std::string root, extension;
    HAL::Ospath::splitext( filename, root, extension);
    if( !extension.empty() && extension[0] == '.' )
        extension = extension.substr( 1);
    if( extension != "mbsdf")
        return false;

    return import_measurement_from_reader( &reader, reflection, transmission);
}

bool import_from_reader(
    mi::neuraylib::IReader* reader,
    mi::neuraylib::IBsdf_isotropic_data*& reflection,
    mi::neuraylib::IBsdf_isotropic_data*& transmission)
{
    return import_measurement_from_reader( reader, reflection, transmission);
}

bool import_from_reader(
    mi::neuraylib::IReader* reader,
    const std::string& archive_filename,
    const std::string& archive_membername,
    mi::neuraylib::IBsdf_isotropic_data*& reflection,
    mi::neuraylib::IBsdf_isotropic_data*& transmission)
{
    std::string root, extension;
    HAL::Ospath::splitext( archive_membername, root, extension);
    if( !extension.empty() && extension[0] == '.' )
        extension = extension.substr( 1);
    if( extension != "mbsdf")
        return false;

    return import_measurement_from_reader( reader, reflection, transmission);
}

namespace {

void export_to_file( DISK::File& file, const mi::neuraylib::IBsdf_isotropic_data* bsdf_data)
{
    mi::neuraylib::Bsdf_type type = bsdf_data->get_type();
    mi::Uint32 resolution_theta   = bsdf_data->get_resolution_theta();
    mi::Uint32 resolution_phi     = bsdf_data->get_resolution_phi();
    mi::Uint32 type_uint32        = type == mi::neuraylib::BSDF_SCALAR ? 0 : 1;

    file.write( reinterpret_cast<const char*>( &type_uint32), 4);
    file.write( reinterpret_cast<const char*>( &resolution_theta), 4);
    file.write( reinterpret_cast<const char*>( &resolution_phi), 4);

    mi::Size size = resolution_theta * resolution_theta * resolution_phi;
    if( type == mi::neuraylib::BSDF_RGB)
        size *= 3;
    mi::base::Handle<const mi::neuraylib::IBsdf_buffer> buffer( bsdf_data->get_bsdf_buffer());
    const mi::Float32* data = buffer->get_data();
    file.write( reinterpret_cast<const char*>( data), size * sizeof( mi::Float32));
}

} // namespace

bool export_to_file(
    const mi::neuraylib::IBsdf_isotropic_data* reflection,
    const mi::neuraylib::IBsdf_isotropic_data* transmission,
    const std::string& filename)
{
    DISK::File file;
    if( !file.open( filename, DISK::IFile::M_WRITE))
        return false;

    file.writeline( magic_header);
    if( reflection) {
        file.writeline( magic_reflection);
        export_to_file( file, reflection);
    }
    if( transmission) {
        file.writeline( magic_transmission);
        export_to_file( file, transmission);
    }
    return true;
}

DB::Tag load_mdl_bsdf_measurement(
    DB::Transaction* transaction,
    const std::string& resolved_filename,
    const std::string& mdl_file_path,
    bool shared)
{
    std::string db_name = shared ? "MI_default_" : "";
    db_name += "bsdf_measurement_" + resolved_filename;
    if( !shared)
        db_name = MDL::DETAIL::generate_unique_db_name( transaction, db_name.c_str());

    DB::Tag tag = transaction->name_to_tag( db_name.c_str());
    if( tag)
        return tag;

    Bsdf_measurement* bsdfm = new Bsdf_measurement();
    mi::Sint32 result = bsdfm->reset_file_mdl( resolved_filename, mdl_file_path);
    ASSERT( M_BSDF_MEASUREMENT, result == 0 || result == -3);
    if( result == -3)
        LOG::mod_log->error( M_SCENE, LOG::Mod_log::C_IO,
            "File format error or invalid filename extension in default BSDF measurement \"%s\".",
            resolved_filename.c_str());

    tag = transaction->store_for_reference_counting(
        bsdfm, db_name.c_str(), transaction->get_scope()->get_level());
    return tag;
}

DB::Tag load_mdl_bsdf_measurement(
    DB::Transaction* transaction,
    mi::neuraylib::IReader* reader,
    const std::string& archive_filename,
    const std::string& archive_membername,
    const std::string& mdl_file_path,
    bool shared)
{
    if( !reader)
        return DB::Tag( 0);

    std::string db_name = shared ? "MI_default_" : "";
    db_name += "bsdf_measurement_" + archive_filename + "_" + archive_membername;
    if( !shared)
        db_name = MDL::DETAIL::generate_unique_db_name( transaction, db_name.c_str());

    DB::Tag tag = transaction->name_to_tag( db_name.c_str());
    if( tag)
        return tag;

    Bsdf_measurement* bsdfm = new Bsdf_measurement();
    mi::Sint32 result = bsdfm->reset_archive_mdl(
        reader, archive_filename, archive_membername, mdl_file_path);
    ASSERT( M_BSDF_MEASUREMENT, result == 0 || result == -3);
    if( result == -3)
        LOG::mod_log->error( M_SCENE, LOG::Mod_log::C_IO,
            "File format error or invalid filename extension in default BSDF measurement \"%s\" "
            "in \"%s\".", archive_membername.c_str(), archive_filename.c_str());

    tag = transaction->store_for_reference_counting(
        bsdfm, db_name.c_str(), transaction->get_scope()->get_level());
    return tag;
}

} // namespace BSDFM

} // namespace MI
