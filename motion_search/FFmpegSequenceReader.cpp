/*
 Copyright (c) Meta Platforms, Inc. and affiliates.

 This source code is licensed under the BSD3 license found in the
 LICENSE file in the root directory of this source tree.
 */

#include "FFmpegSequenceReader.h"

#ifdef HAVE_FFMPEG

#include "EOFException.h"
#include <cstring>
#include <iostream>

FFmpegSequenceReader::FFmpegSequenceReader(void)
    : format_ctx_(nullptr), codec_ctx_(nullptr), sws_ctx_(nullptr),
      frame_(nullptr), frame_yuv_(nullptr), packet_(nullptr),
      video_stream_idx_(-1), frame_count_(0), eof_(false) {}

FFmpegSequenceReader::~FFmpegSequenceReader(void) { cleanup(); }

void FFmpegSequenceReader::cleanup() {
  // Free frames
  if (frame_yuv_) {
    av_frame_free(&frame_yuv_);
    frame_yuv_ = nullptr;
  }
  if (frame_) {
    av_frame_free(&frame_);
    frame_ = nullptr;
  }

  // Free packet
  if (packet_) {
    av_packet_free(&packet_);
    packet_ = nullptr;
  }

  // Free scaler context
  if (sws_ctx_) {
    sws_freeContext(sws_ctx_);
    sws_ctx_ = nullptr;
  }

  // Free codec context
  if (codec_ctx_) {
    avcodec_free_context(&codec_ctx_);
    codec_ctx_ = nullptr;
  }

  // Close format context
  if (format_ctx_) {
    avformat_close_input(&format_ctx_);
    format_ctx_ = nullptr;
  }

  video_stream_idx_ = -1;
  eof_ = false;
}

bool FFmpegSequenceReader::Open(const std::string &filepath) {
  filename_ = filepath;

  // Open input file
  if (avformat_open_input(&format_ctx_, filepath.c_str(), nullptr, nullptr) <
      0) {
    std::cerr << "FFmpeg: Could not open file: " << filepath << "\n";
    return false;
  }

  // Retrieve stream information
  if (avformat_find_stream_info(format_ctx_, nullptr) < 0) {
    std::cerr << "FFmpeg: Could not find stream information\n";
    cleanup();
    return false;
  }

  // Find video stream
  if (!findVideoStream()) {
    std::cerr << "FFmpeg: Could not find video stream in file\n";
    cleanup();
    return false;
  }

  // Initialize decoder
  if (!initializeDecoder()) {
    std::cerr << "FFmpeg: Failed to initialize decoder\n";
    cleanup();
    return false;
  }

  // Set dimensions and stride
  m_dim.width = codec_ctx_->width;
  m_dim.height = codec_ctx_->height;
  m_stride = codec_ctx_->width;

  // Initialize scaler for YUV420p conversion
  if (!initializeScaler()) {
    std::cerr << "FFmpeg: Failed to initialize scaler\n";
    cleanup();
    return false;
  }

  // Allocate frames
  frame_ = av_frame_alloc();
  frame_yuv_ = av_frame_alloc();
  packet_ = av_packet_alloc();

  if (!frame_ || !frame_yuv_ || !packet_) {
    std::cerr << "FFmpeg: Could not allocate frame or packet\n";
    cleanup();
    return false;
  }

  // Allocate buffer for YUV420p frame
  frame_yuv_->format = AV_PIX_FMT_YUV420P;
  frame_yuv_->width = codec_ctx_->width;
  frame_yuv_->height = codec_ctx_->height;

  if (av_frame_get_buffer(frame_yuv_, 0) < 0) {
    std::cerr << "FFmpeg: Could not allocate frame buffer\n";
    cleanup();
    return false;
  }

  return true;
}

bool FFmpegSequenceReader::findVideoStream() {
  for (unsigned int i = 0; i < format_ctx_->nb_streams; i++) {
    if (format_ctx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      video_stream_idx_ = i;
      return true;
    }
  }
  return false;
}

bool FFmpegSequenceReader::initializeDecoder() {
  AVCodecParameters *codecpar =
      format_ctx_->streams[video_stream_idx_]->codecpar;

  // Find decoder for the video stream
  const AVCodec *codec = avcodec_find_decoder(codecpar->codec_id);
  if (!codec) {
    std::cerr << "FFmpeg: Unsupported codec\n";
    return false;
  }

  // Allocate codec context
  codec_ctx_ = avcodec_alloc_context3(codec);
  if (!codec_ctx_) {
    std::cerr << "FFmpeg: Could not allocate codec context\n";
    return false;
  }

  // Copy codec parameters to context
  if (avcodec_parameters_to_context(codec_ctx_, codecpar) < 0) {
    std::cerr << "FFmpeg: Could not copy codec parameters\n";
    return false;
  }

  // Open codec
  if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) {
    std::cerr << "FFmpeg: Could not open codec\n";
    return false;
  }

  return true;
}

bool FFmpegSequenceReader::initializeScaler() {
  // Create scaler context for converting to YUV420p
  sws_ctx_ =
      sws_getContext(codec_ctx_->width, codec_ctx_->height, codec_ctx_->pix_fmt,
                     codec_ctx_->width, codec_ctx_->height, AV_PIX_FMT_YUV420P,
                     SWS_BILINEAR, nullptr, nullptr, nullptr);

  if (!sws_ctx_) {
    std::cerr << "FFmpeg: Could not initialize scaler context\n";
    return false;
  }

  return true;
}

bool FFmpegSequenceReader::decodeNextFrame() {
  while (true) {
    // Read packet from file
    int ret = av_read_frame(format_ctx_, packet_);
    if (ret < 0) {
      if (ret == AVERROR_EOF) {
        eof_ = true;
        // Flush decoder
        avcodec_send_packet(codec_ctx_, nullptr);
        ret = avcodec_receive_frame(codec_ctx_, frame_);
        if (ret == 0) {
          return true;
        }
      }
      return false;
    }

    // Check if packet is from video stream
    if (packet_->stream_index == video_stream_idx_) {
      // Send packet to decoder
      ret = avcodec_send_packet(codec_ctx_, packet_);
      av_packet_unref(packet_);

      if (ret < 0) {
        std::cerr << "FFmpeg: Error sending packet to decoder\n";
        return false;
      }

      // Receive decoded frame
      ret = avcodec_receive_frame(codec_ctx_, frame_);
      if (ret == 0) {
        // Successfully decoded a frame
        return true;
      } else if (ret == AVERROR(EAGAIN)) {
        // Need more packets to produce a frame
        continue;
      } else if (ret == AVERROR_EOF) {
        eof_ = true;
        return false;
      } else {
        std::cerr << "FFmpeg: Error receiving frame from decoder\n";
        return false;
      }
    } else {
      // Not a video packet, skip
      av_packet_unref(packet_);
    }
  }
}

void FFmpegSequenceReader::readPicture(uint8_t *pY, uint8_t *pU, uint8_t *pV) {
  if (eof_) {
    throw EOFException();
  }

  // Decode next frame
  if (!decodeNextFrame()) {
    eof_ = true;
    throw EOFException();
  }

  // Convert frame to YUV420p
  sws_scale(sws_ctx_, frame_->data, frame_->linesize, 0, codec_ctx_->height,
            frame_yuv_->data, frame_yuv_->linesize);

  // Copy Y plane
  for (int y = 0; y < m_dim.height; y++) {
    memcpy(pY + y * m_stride, frame_yuv_->data[0] + y * frame_yuv_->linesize[0],
           m_dim.width);
  }

  // Copy U plane (half resolution)
  int uv_width = m_dim.width / 2;
  int uv_height = m_dim.height / 2;
  ptrdiff_t uv_stride = m_stride / 2;

  for (int y = 0; y < uv_height; y++) {
    memcpy(pU + y * uv_stride,
           frame_yuv_->data[1] + y * frame_yuv_->linesize[1], uv_width);
    memcpy(pV + y * uv_stride,
           frame_yuv_->data[2] + y * frame_yuv_->linesize[2], uv_width);
  }

  frame_count_++;
}

bool FFmpegSequenceReader::eof(void) { return eof_; }

int FFmpegSequenceReader::nframes(void) {
  if (!format_ctx_ || video_stream_idx_ < 0) {
    return 0;
  }

  AVStream *video_stream = format_ctx_->streams[video_stream_idx_];
  if (video_stream->nb_frames > 0) {
    return static_cast<int>(video_stream->nb_frames);
  }

  // If nb_frames is not available, estimate from duration
  if (video_stream->duration != AV_NOPTS_VALUE) {
    AVRational time_base = video_stream->time_base;
    AVRational frame_rate = video_stream->avg_frame_rate;
    if (frame_rate.num > 0 && frame_rate.den > 0) {
      double duration_sec = video_stream->duration * av_q2d(time_base);
      double fps = av_q2d(frame_rate);
      return static_cast<int>(duration_sec * fps);
    }
  }

  // Unknown frame count
  return 0;
}

bool FFmpegSequenceReader::isOpen(void) {
  return format_ctx_ != nullptr && codec_ctx_ != nullptr &&
         video_stream_idx_ >= 0;
}

#endif // HAVE_FFMPEG
