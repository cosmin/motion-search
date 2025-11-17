/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "BaseVideoSequenceReader.h"
#include "common.h"

#include <memory>
#include <string>

#ifdef HAVE_FFMPEG

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

/// FFmpegSequenceReader - Reads video files using FFmpeg libraries
///
/// Supports various video formats (MP4, MKV, AVI, WebM) and codecs
/// (H.264, H.265, VP9, AV1). Automatically detects video stream,
/// decodes frames, and converts to YUV420p planar format.
///
/// Usage:
///   FFmpegSequenceReader reader;
///   if (reader.Open("video.mp4")) {
///     uint8_t *y, *u, *v;
///     // ... allocate buffers ...
///     reader.read(y, u, v);
///   }
class FFmpegSequenceReader : public BaseVideoSequenceReader {
public:
  FFmpegSequenceReader(void);
  ~FFmpegSequenceReader(void) override;

  /// Open a video file for reading
  ///
  /// @param filepath Path to video file (MP4, MKV, AVI, WebM, etc.)
  /// @return true if file opened successfully and video stream found
  bool Open(const std::string &filepath);

  // IVideoSequenceReader interface implementation
  bool eof(void) override;
  int nframes(void) override;
  const DIM dim(void) override { return m_dim; }
  ptrdiff_t stride(void) override { return m_stride; }
  bool isOpen(void) override;

protected:
  void readPicture(uint8_t *pY, uint8_t *pU, uint8_t *pV) override;

private:
  // Video dimensions and stride
  DIM m_dim = {0, 0};
  ptrdiff_t m_stride = 0;

  // FFmpeg context structures
  AVFormatContext *format_ctx_ = nullptr;
  AVCodecContext *codec_ctx_ = nullptr;
  SwsContext *sws_ctx_ = nullptr;

  // Frame structures
  AVFrame *frame_ = nullptr;     // Decoded frame (native format)
  AVFrame *frame_yuv_ = nullptr; // Converted frame (YUV420p)
  AVPacket *packet_ = nullptr;   // Encoded packet

  // Video stream information
  int video_stream_idx_ = -1;
  int64_t frame_count_ = 0;
  bool eof_ = false;

  // Filename for error reporting
  std::string filename_;

  // Helper methods
  bool findVideoStream();
  bool initializeDecoder();
  bool initializeScaler();
  void cleanup();
  bool decodeNextFrame();

  // Delete copy constructor and assignment operator
  FFmpegSequenceReader(const FFmpegSequenceReader &) = delete;
  FFmpegSequenceReader &operator=(const FFmpegSequenceReader &) = delete;
};

#endif // HAVE_FFMPEG
