/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "cores/VideoPlayer/Buffers/VideoBuffer.h"
#include "cores/VideoPlayer/DVDCodecs/Video/DVDVideoCodec.h"

extern "C"
{
#include <libavutil/frame.h>
#include <libavutil/hwcontext_drm.h>
}

namespace DRMPRIME
{

std::string GetColorEncoding(const VideoPicture& picture);
std::string GetColorRange(const VideoPicture& picture);

} // namespace DRMPRIME

class CVideoBufferDRMPRIME : public CVideoBuffer
{
public:
  CVideoBufferDRMPRIME() = delete;
  ~CVideoBufferDRMPRIME() override = default;

  virtual void SetPictureParams(const VideoPicture& picture) { m_picture.SetParams(picture); }
  virtual const VideoPicture& GetPicture() const { return m_picture; }
  virtual uint32_t GetWidth() const { return GetPicture().iWidth; }
  virtual uint32_t GetHeight() const { return GetPicture().iHeight; }
  // position of the picture within the buffer
  virtual uint32_t GetCropX() const { return 0; }
  virtual uint32_t GetCropY() const { return 0; }

  virtual AVDRMFrameDescriptor* GetDescriptor() const = 0;
  virtual bool IsValid() const;
  virtual bool AcquireDescriptor() { return true; }
  virtual void ReleaseDescriptor() {}

protected:
  explicit CVideoBufferDRMPRIME(int id);

  VideoPicture m_picture;
};

class CVideoBufferDRMPRIMEFFmpeg : public CVideoBufferDRMPRIME
{
public:
  CVideoBufferDRMPRIMEFFmpeg(IVideoBufferPool& pool, int id);
  ~CVideoBufferDRMPRIMEFFmpeg() override;
  void SetRef(AVFrame* frame);
  void Unref();

  AVDRMFrameDescriptor* GetDescriptor() const override;
  uint32_t GetCropX() const override { return static_cast<uint32_t>(m_pFrame->crop_left); }
  uint32_t GetCropY() const override { return static_cast<uint32_t>(m_pFrame->crop_top); }
  bool IsValid() const override;
  bool AcquireDescriptor() override;
  void ReleaseDescriptor() override;

protected:
  bool RequiresMapping() const;

  AVFrame* m_pFrame = nullptr;
  AVFrame* m_pMapFrame = nullptr;
};

class CVideoBufferPoolDRMPRIMEFFmpeg : public IVideoBufferPool
{
public:
  ~CVideoBufferPoolDRMPRIMEFFmpeg() override;
  void Return(int id) override;
  CVideoBufferDRMPRIMEFFmpeg* Get() override;

protected:
  CCriticalSection m_critSection;
  std::vector<CVideoBufferDRMPRIMEFFmpeg*> m_all;
  std::deque<int> m_used;
  std::deque<int> m_free;
};
