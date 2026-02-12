================================================================================
OVERLAY IMAGES DIRECTORY
================================================================================

This directory contains overlay images used by the media generation system.

REQUIRED OVERLAY FILES
================================================================================

1. URGENCY BADGES (PNG with transparency)
   - urgency-critical.png    (120x40px) - Red badge for critical urgency
   - urgency-warning.png     (120x40px) - Yellow/amber badge for warning urgency
   - urgency-normal.png      (120x40px) - Green badge for normal status

2. LED DIGIT SEGMENTS (PNG with transparency)
   - led-segment-on.png      (Single lit LED segment)
   - led-segment-off.png     (Single unlit LED segment)
   OR
   - led-digits-sprite.png   (Sprite sheet with all digits 0-9 and colon)

3. LOGO OVERLAYS (PNG with transparency)
   - logo-watermark.png      (150x50px max) - Semi-transparent logo
   - logo-full.png           (300x100px max) - Full color logo

4. BADGE TEMPLATES (PNG with transparency)
   - badge-adopted.png       (200x60px) - "ADOPTED!" celebration badge
   - badge-new.png           (100x40px) - "NEW" badge for recent listings
   - badge-featured.png      (120x40px) - "FEATURED" badge

5. COUNTDOWN OVERLAYS (PNG with transparency)
   - countdown-background.png (400x100px) - Background for countdown timer
   - countdown-urgent.png     (400x100px) - Urgent countdown background

6. SHARE CARD TEMPLATES (PNG)
   - share-card-facebook.png  (1200x630px) - Facebook share template
   - share-card-twitter.png   (1200x675px) - Twitter share template
   - share-card-instagram.png (1080x1080px) - Instagram share template

7. VIDEO INTRO/OUTRO (MP4 or PNG sequence)
   - intro-sequence/         Directory with intro frames
   - outro-sequence/         Directory with outro frames
   - intro.mp4              Pre-rendered intro video
   - outro.mp4              Pre-rendered outro video

DESIGN SPECIFICATIONS
================================================================================

LED Counter Colors:
- On Color: RGB(255, 50, 50) - Bright red
- Off Color: RGB(30, 30, 30) - Dark gray
- Background: RGBA(0, 0, 0, 180) - Semi-transparent black

Urgency Badge Colors:
- Critical: RGB(220, 53, 69) - Bootstrap danger red
- Warning: RGB(255, 193, 7) - Bootstrap warning yellow
- Normal: RGB(40, 167, 69) - Bootstrap success green

Text Colors:
- Primary text: White with black shadow
- Badge text: White on colored background

File Format Requirements:
- All images should be PNG with alpha channel for transparency
- Videos should be MP4 with H.264 codec
- Maximum resolution: 4K (3840x2160)
- Recommended: Use vector graphics (SVG) for source files

USAGE
================================================================================

The ImageProcessor and VideoGenerator classes will look for these files in:
  static/images/overlays/

If files are missing, the system will:
1. Generate overlays programmatically when possible
2. Skip overlays that require external assets
3. Log warnings for missing files

CREATING CUSTOM OVERLAYS
================================================================================

To create custom overlays:

1. Design in vector format (Illustrator, Figma, etc.)
2. Export as PNG with transparency
3. Ensure correct dimensions
4. Place in this directory
5. Update config/media_config.json if using custom filenames

For programmatic overlay generation, the system can create:
- LED segment displays (rendered mathematically)
- Solid color badges with text
- Basic text overlays

External assets are needed for:
- Complex graphics with gradients
- Logos and branded elements
- Photo-realistic elements

================================================================================
Author: Agent 15 - WaitingTheLongest Build System
Date: 2024-01-28
================================================================================
