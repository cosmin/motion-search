#!/bin/bash

# Script to generate test videos for motion_search testing
# Requires: ffmpeg

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_DATA_DIR="${SCRIPT_DIR}/test_data"

mkdir -p "$TEST_DATA_DIR"

WIDTH=320
HEIGHT=180
FRAMES=10
FRAMERATE=30

echo "Generating test videos (${WIDTH}x${HEIGHT}, ${FRAMES} frames)..."

# 1. Test source pattern (moving gradient with shapes)
echo "  - testsrc.yuv (moving test pattern)"
ffmpeg -y -f lavfi -i testsrc=duration=${FRAMES}d/${FRAMERATE}:size=${WIDTH}x${HEIGHT}:rate=${FRAMERATE} \
  -pix_fmt yuv420p -frames:v ${FRAMES} "${TEST_DATA_DIR}/testsrc.yuv"

# 2. SMPTE color bars (static)
echo "  - smptebars.yuv (color bars - static)"
ffmpeg -y -f lavfi -i smptebars=duration=${FRAMES}d/${FRAMERATE}:size=${WIDTH}x${HEIGHT}:rate=${FRAMERATE} \
  -pix_fmt yuv420p -frames:v ${FRAMES} "${TEST_DATA_DIR}/smptebars.yuv"

# 3. Solid black (for zero-motion testing)
echo "  - black.yuv (solid black - no motion)"
ffmpeg -y -f lavfi -i color=c=black:duration=${FRAMES}d/${FRAMERATE}:size=${WIDTH}x${HEIGHT}:rate=${FRAMERATE} \
  -pix_fmt yuv420p -frames:v ${FRAMES} "${TEST_DATA_DIR}/black.yuv"

# 4. Solid white (for high-variance testing)
echo "  - white.yuv (solid white - no motion)"
ffmpeg -y -f lavfi -i color=c=white:duration=${FRAMES}d/${FRAMERATE}:size=${WIDTH}x${HEIGHT}:rate=${FRAMERATE} \
  -pix_fmt yuv420p -frames:v ${FRAMES} "${TEST_DATA_DIR}/white.yuv"

# 5. Gray pattern (mid-range values)
echo "  - gray.yuv (50% gray - no motion)"
ffmpeg -y -f lavfi -i color=c=gray:duration=${FRAMES}d/${FRAMERATE}:size=${WIDTH}x${HEIGHT}:rate=${FRAMERATE} \
  -pix_fmt yuv420p -frames:v ${FRAMES} "${TEST_DATA_DIR}/gray.yuv"

# 6. Horizontal gradient (for directional testing)
echo "  - hgradient.yuv (horizontal gradient)"
ffmpeg -y -f lavfi -i "color=c=black:s=${WIDTH}x${HEIGHT},format=yuv420p,geq='lum=255*X/W:cb=128:cr=128'" \
  -frames:v ${FRAMES} "${TEST_DATA_DIR}/hgradient.yuv"

# 7. Vertical gradient
echo "  - vgradient.yuv (vertical gradient)"
ffmpeg -y -f lavfi -i "color=c=black:s=${WIDTH}x${HEIGHT},format=yuv420p,geq='lum=255*Y/H:cb=128:cr=128'" \
  -frames:v ${FRAMES} "${TEST_DATA_DIR}/vgradient.yuv"

# 8. Checkerboard pattern (high spatial frequency)
echo "  - checkerboard.yuv (checkerboard pattern)"
ffmpeg -y -f lavfi -i "color=c=black:s=${WIDTH}x${HEIGHT},format=yuv420p,geq='lum=128+127*sin(X*0.2)*sin(Y*0.2):cb=128:cr=128'" \
  -frames:v ${FRAMES} "${TEST_DATA_DIR}/checkerboard.yuv"

# 9. Moving box (known motion vector)
echo "  - moving_box.yuv (white box moving right)"
ffmpeg -y -f lavfi -i "color=c=black:s=${WIDTH}x${HEIGHT}:r=${FRAMERATE}[bg];color=c=white:s=32x32:r=${FRAMERATE}[box];[bg][box]overlay=x='t*30':y=${HEIGHT}/2-16:format=yuv420p" \
  -frames:v ${FRAMES} -pix_fmt yuv420p "${TEST_DATA_DIR}/moving_box.yuv"

# 10. Mandelbrot (complex pattern with slight motion)
echo "  - mandelbrot.yuv (fractal pattern)"
ffmpeg -y -f lavfi -i mandelbrot=size=${WIDTH}x${HEIGHT}:rate=${FRAMERATE} \
  -pix_fmt yuv420p -frames:v ${FRAMES} "${TEST_DATA_DIR}/mandelbrot.yuv"

# 11. Create Y4M version of testsrc for format testing
echo "  - testsrc.y4m (Y4M format)"
ffmpeg -y -f lavfi -i testsrc=duration=${FRAMES}d/${FRAMERATE}:size=${WIDTH}x${HEIGHT}:rate=${FRAMERATE} \
  -pix_fmt yuv420p -frames:v ${FRAMES} "${TEST_DATA_DIR}/testsrc.y4m"

# 12. Single frame (for I-frame testing)
echo "  - single_frame.yuv (single frame)"
ffmpeg -y -f lavfi -i testsrc=duration=1:size=${WIDTH}x${HEIGHT}:rate=1 \
  -pix_fmt yuv420p -frames:v 1 "${TEST_DATA_DIR}/single_frame.yuv"

# 13. Two identical frames (for zero-motion-vector testing)
echo "  - two_identical.yuv (two identical frames)"
ffmpeg -y -f lavfi -i color=c=blue:duration=2:size=${WIDTH}x${HEIGHT}:rate=1 \
  -pix_fmt yuv420p -frames:v 2 "${TEST_DATA_DIR}/two_identical.yuv"

# Calculate file sizes for verification
echo ""
echo "Generated test videos:"
ls -lh "${TEST_DATA_DIR}"/*.yuv "${TEST_DATA_DIR}"/*.y4m 2>/dev/null || true

echo ""
echo "Test data generation complete!"
echo "Location: ${TEST_DATA_DIR}"
