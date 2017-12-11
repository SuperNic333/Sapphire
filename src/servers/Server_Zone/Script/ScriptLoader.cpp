#include "ScriptLoader.h"

#include <Server_Common/Logging/Logger.h>
#include <boost/format.hpp>
#include <boost/filesystem.hpp>

extern Core::Logger g_log;

Core::Scripting::ScriptLoader::ScriptLoader()
{}

const std::string Core::Scripting::ScriptLoader::getModuleExtension()
{
#ifdef _WIN32
   return ".dll";
#elif __APPLE__
   return ".dylib";
#else
   return ".so";
#endif
}

bool Core::Scripting::ScriptLoader::unloadModule( ModuleHandle handle )
{
#ifdef _WIN32
   bool success = FreeLibrary( handle ) != 0;
#else
   bool success = dlclose( handle ) == 0;
#endif

   if( !success )
   {
      g_log.fatal( "Failed to unload module @ 0x" + boost::str( boost::format( "%|08X|" ) % handle ) );

      return false;
   }

   g_log.debug( "Unloaded module @ 0x" + boost::str( boost::format( "%|08X|" ) % handle ) );

   return true;
}

Core::Scripting::ScriptInfo* Core::Scripting::ScriptLoader::loadModule( std::string path )
{
#ifdef _WIN32
   ModuleHandle handle = LoadLibrary( path.c_str() );
#else
   ModuleHandle handle = dlopen( path.c_str(), RTLD_LAZY );
#endif

   if( !handle )
   {
      g_log.error( "Failed to load module from: " + path );

      return nullptr;
   }

   g_log.info( "Loaded module from '" + path + "' @ 0x" + boost::str( boost::format( "%|08X|" ) % handle ) );

   boost::filesystem::path f( path );

   auto info = new ScriptInfo;
   info->handle = handle;
   info->library_name = f.stem().string();

   m_scriptMap.insert( std::make_pair( f.stem().string(), info ) );

   return info;
}

ScriptObject* Core::Scripting::ScriptLoader::getScriptObjectExport( ModuleHandle handle, std::string name )
{
   typedef ScriptObject* (*getScriptObjectType)();
   auto fn = boost::str( boost::format( "get%1%" ) % name );

   g_log.debug( "getting symbol: " + fn  );

#ifdef _WIN32
   getScriptObjectType func = reinterpret_cast< getScriptObjectType >( GetProcAddress( handle, fn.c_str() ) );
#else
   getScriptObjectType func = reinterpret_cast< getScriptObjectType >( dlsym( handle, fn.c_str() ) );
#endif

   if( func )
   {
      auto ptr = func();

      g_log.debug( "got ScriptObject @ 0x" + boost::str( boost::format( "%|08X|" ) % ptr ) );
      g_log.debug( "script info -> name: " + std::string( ptr->getName() ) + ", id: " + std::to_string( ptr->getId() ) );

      return ptr;
   }
   else
      return nullptr;
}

bool Core::Scripting::ScriptLoader::unloadScript( std::string name )
{
   auto info = m_scriptMap.find( name );
   if( info == m_scriptMap.end() )
      return false;

   return unloadScript( info->second->handle );
}

bool Core::Scripting::ScriptLoader::unloadScript( ModuleHandle handle )
{
   for( auto it = m_scriptMap.begin(); it != m_scriptMap.end(); ++it )
   {
      if( it->second->handle == handle )
      {
         delete it->second;
         m_scriptMap.erase( it );

         return unloadModule( handle );
      }
   }

   return false;
}

const std::string& Core::Scripting::ScriptLoader::getModuleNameFromHandle( ModuleHandle handle ) const
{
   for( auto it = m_scriptMap.begin(); it != m_scriptMap.end(); ++it )
   {
      if( it->second->handle == handle )
      {
         return it->first;
      }
   }

   // nb: i'm not sure how this would ever be reached but you know
   return "";
}