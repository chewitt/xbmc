/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PlayListPLS.h"

#include "FileItem.h"
#include "PlayListFactory.h"
#include "Util.h"
#include "filesystem/File.h"
#include "music/MusicFileItemClassify.h"
#include "music/tags/MusicInfoTag.h"
#include "utils/CharsetConverter.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include <iostream>
#include <string>
#include <vector>

using namespace XFILE;

#define START_PLAYLIST_MARKER "[playlist]" // may be case-insensitive (equivalent to .ini file on win32)
#define PLAYLIST_NAME     "PlaylistName"

namespace KODI::PLAYLIST
{

/*----------------------------------------------------------------------
[playlist]
PlaylistName=Playlist 001
File1=E:\Program Files\Winamp3\demo.mp3
Title1=demo
Length1=5
File2=E:\Program Files\Winamp3\demo.mp3
Title2=demo
Length2=5
NumberOfEntries=2
Version=2
----------------------------------------------------------------------*/
CPlayListPLS::CPlayListPLS(void) = default;

CPlayListPLS::~CPlayListPLS(void) = default;

bool CPlayListPLS::Load(const std::string &strFile)
{
  //read it from the file
  std::string strFileName(strFile);
  m_strPlayListName = URIUtils::GetFileName(strFileName);

  Clear();

  bool bShoutCast = false;
  if( StringUtils::StartsWithNoCase(strFileName, "shout://") )
  {
    strFileName.replace(0, 8, "http://");
    m_strBasePath = "";
    bShoutCast = true;
  }
  else
    URIUtils::GetParentPath(strFileName, m_strBasePath);

  CFile file;
  if (!file.Open(strFileName) )
  {
    file.Close();
    return false;
  }

  if (file.GetLength() > 1024*1024)
  {
    CLog::Log(LOGWARNING, "{} - File is larger than 1 MB, most likely not a playlist",
              __FUNCTION__);
    return false;
  }

  std::string strLine;
  strLine.reserve(1024);

  // run through looking for the [playlist] marker.
  // if we find another http stream, then load it.
  while (true)
  {
    if (!file.ReadLine(strLine))
    {
      file.Close();
      return size() > 0;
    }
    StringUtils::Trim(strLine);
    if(StringUtils::EqualsNoCase(strLine, START_PLAYLIST_MARKER))
      break;

    // if there is something else before playlist marker, this isn't a pls file
    if(!strLine.empty())
      return false;
  }

  bool bFailed = false;
  while (file.ReadLine(strLine))
  {
    StringUtils::RemoveCRLF(strLine);
    size_t iPosEqual = strLine.find('=');
    if (iPosEqual != std::string::npos)
    {
      std::string strLeft = strLine.substr(0, iPosEqual);
      iPosEqual++;
      std::string strValue = strLine.substr(iPosEqual);
      StringUtils::ToLower(strLeft);
      StringUtils::TrimLeft(strLeft);

      if (strLeft == "numberofentries")
      {
        m_vecItems.reserve(atoi(strValue.c_str()));
      }
      else if (StringUtils::StartsWith(strLeft, "file"))
      {
        std::vector <int>::size_type idx = atoi(strLeft.c_str() + 4);
        if (!Resize(idx))
        {
          bFailed = true;
          break;
        }

        // Skip self - do not load playlist recursively
        if (StringUtils::EqualsNoCase(URIUtils::GetFileName(strValue),
                                      URIUtils::GetFileName(strFileName)))
          continue;

        if (m_vecItems[idx - 1]->GetLabel().empty())
          m_vecItems[idx - 1]->SetLabel(URIUtils::GetFileName(strValue));
        CFileItem item(strValue, false);
        if (bShoutCast && !MUSIC::IsAudio(item))
          strValue.replace(0, 7, "shout://");

        strValue = URIUtils::SubstitutePath(strValue);
        CUtil::GetQualifiedFilename(m_strBasePath, strValue);
        g_charsetConverter.unknownToUTF8(strValue);
        m_vecItems[idx - 1]->SetPath(strValue);
      }
      else if (StringUtils::StartsWith(strLeft, "title"))
      {
        std::vector <int>::size_type idx = atoi(strLeft.c_str() + 5);
        if (!Resize(idx))
        {
          bFailed = true;
          break;
        }
        g_charsetConverter.unknownToUTF8(strValue);
        m_vecItems[idx - 1]->SetLabel(strValue);
      }
      else if (StringUtils::StartsWith(strLeft, "length"))
      {
        std::vector <int>::size_type idx = atoi(strLeft.c_str() + 6);
        if (!Resize(idx))
        {
          bFailed = true;
          break;
        }
        m_vecItems[idx - 1]->GetMusicInfoTag()->SetDuration(atol(strValue.c_str()));
      }
      else if (strLeft == "playlistname")
      {
        m_strPlayListName = strValue;
        g_charsetConverter.unknownToUTF8(m_strPlayListName);
      }
    }
  }
  file.Close();

  if (bFailed)
  {
    CLog::Log(LOGERROR,
              "File {} is not a valid PLS playlist. Location of first file,title or length is not "
              "permitted (eg. File0 should be File1)",
              URIUtils::GetFileName(strFileName));
    return false;
  }

  // check for missing entries
  ivecItems p = m_vecItems.begin();
  while ( p != m_vecItems.end())
  {
    if ((*p)->GetPath().empty())
    {
      p = m_vecItems.erase(p);
    }
    else
    {
      ++p;
    }
  }

  return true;
}

void CPlayListPLS::Save(const std::string& strFileName) const
{
  if (m_vecItems.empty())
    return;
  std::string strPlaylist = CUtil::MakeLegalPath(strFileName);
  CFile file;
  if (!file.OpenForWrite(strPlaylist, true))
  {
    CLog::Log(LOGERROR, "Could not save PLS playlist: [{}]", strPlaylist);
    return;
  }
  std::string write;
  write += StringUtils::Format("{}\n", START_PLAYLIST_MARKER);
  std::string strPlayListName=m_strPlayListName;
  g_charsetConverter.utf8ToStringCharset(strPlayListName);
  write += StringUtils::Format("PlaylistName={}\n", strPlayListName);

  for (int i = 0; i < (int)m_vecItems.size(); ++i)
  {
    CFileItemPtr item = m_vecItems[i];
    std::string strFileName=item->GetPath();
    g_charsetConverter.utf8ToStringCharset(strFileName);
    std::string strDescription=item->GetLabel();
    g_charsetConverter.utf8ToStringCharset(strDescription);
    write += StringUtils::Format("File{}={}\n", i + 1, strFileName);
    write += StringUtils::Format("Title{}={}\n", i + 1, strDescription.c_str());
    write +=
        StringUtils::Format("Length{}={}\n", i + 1, item->GetMusicInfoTag()->GetDuration() / 1000);
  }

  write += StringUtils::Format("NumberOfEntries={0}\n", m_vecItems.size());
  write += StringUtils::Format("Version=2\n");
  file.Write(write.c_str(), write.size());
  file.Close();
}

bool CPlayListPLS::Resize(std::vector <int>::size_type newSize)
{
  if (newSize == 0)
    return false;

  while (m_vecItems.size() < newSize)
  {
    CFileItemPtr fileItem(new CFileItem());
    m_vecItems.push_back(fileItem);
  }
  return true;
}

} // namespace KODI::PLAYLIST
