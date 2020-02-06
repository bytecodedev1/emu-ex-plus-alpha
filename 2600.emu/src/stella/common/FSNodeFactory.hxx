//============================================================================
//
//   SSSS    tt          lll  lll
//  SS  SS   tt           ll   ll
//  SS     tttttt  eeee   ll   ll   aaaa
//   SSSS    tt   ee  ee  ll   ll      aa
//      SS   tt   eeeeee  ll   ll   aaaaa  --  "An Atari 2600 VCS Emulator"
//  SS  SS   tt   ee      ll   ll  aa  aa
//   SSSS     ttt  eeeee llll llll  aaaaa
//
// Copyright (c) 1995-2020 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//============================================================================

#ifndef FSNODE_FACTORY_HXX
#define FSNODE_FACTORY_HXX

class AbstractFSNode;

#if defined(ZIP_SUPPORT)
  #include "FSNodeZIP.hxx"
#endif
#if defined(BSPF_UNIX) || defined(BSPF_MACOS)
  #include "FSNodePOSIX.hxx"
#elif defined(BSPF_WINDOWS)
  #include "FSNodeWINDOWS.hxx"
#elif defined(EMU_EX_PLATFORM)
  #include "FSNodeEmuEx.hh"
#elif defined(__LIB_RETRO__)
  #include "FSNodeLIBRETRO.hxx"
#else
  #error Unsupported platform in FSNodeFactory!
#endif

/**
  This class deals with creating the different FSNode implementations.

  @author  Stephen Anthony
*/
class FilesystemNodeFactory
{
  public:
    enum class Type { SYSTEM, ZIP };

  public:
    static unique_ptr<AbstractFSNode> create(const string& path, Type type)
    {
      switch(type)
      {
        case Type::SYSTEM:
        #if defined(BSPF_UNIX) || defined(BSPF_MACOS)
          return make_unique<FilesystemNodePOSIX>(path);
        #elif defined(BSPF_WINDOWS)
          return make_unique<FilesystemNodeWINDOWS>(path);
        #elif defined(EMU_EX_PLATFORM)
          return make_unique<FilesystemNodeEmuEx>(path);
        #elif defined(__LIB_RETRO__)
          return make_unique<FilesystemNodeLIBRETRO>(path);
        #endif
          break;
        case Type::ZIP:
        #if defined(ZIP_SUPPORT)
          return make_unique<FilesystemNodeZIP>(path);
        #endif
          break;
      }
      return nullptr;
    }

  private:
    // Following constructors and assignment operators not supported
    FilesystemNodeFactory() = delete;
    FilesystemNodeFactory(const FilesystemNodeFactory&) = delete;
    FilesystemNodeFactory(FilesystemNodeFactory&&) = delete;
    FilesystemNodeFactory& operator=(const FilesystemNodeFactory&) = delete;
    FilesystemNodeFactory& operator=(FilesystemNodeFactory&&) = delete;
};

#endif
