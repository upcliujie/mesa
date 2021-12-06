/**************************************************************************
 *
 * Copyright 2022 Emmanuel Gil Peyrot
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

//! This file contains types useful to work with the Glide API.
//!
//! Not all types in the header are exposed yet, this is a work in progress.

use std::fmt;
use std::os::raw::c_void;
use std::ptr::null_mut;

#[derive(Clone, Copy, Default)]
#[repr(transparent)]
pub struct Color(u32);

impl fmt::Debug for Color {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "#{:08x}", self.0)
    }
}

impl Color {
    pub fn to_rgba(&self, format: ColorFormat) -> (u8, u8, u8, u8) {
        let c = self.0;
        let (r, g, b, a) = match format {
            ColorFormat::Argb => (
                (c >> 16) & 0xff,
                (c >> 8) & 0xff,
                (c >> 0) & 0xff,
                (c >> 24) & 0xff,
            ),
            ColorFormat::Abgr => (
                (c >> 0) & 0xff,
                (c >> 8) & 0xff,
                (c >> 16) & 0xff,
                (c >> 24) & 0xff,
            ),
            ColorFormat::Rgba => (
                (c >> 24) & 0xff,
                (c >> 16) & 0xff,
                (c >> 8) & 0xff,
                (c >> 0) & 0xff,
            ),
            ColorFormat::Bgra => (
                (c >> 8) & 0xff,
                (c >> 16) & 0xff,
                (c >> 24) & 0xff,
                (c >> 0) & 0xff,
            ),
        };
        (r as u8, g as u8, b as u8, a as u8)
    }

    pub fn to_rgba_f(&self, format: ColorFormat) -> (f32, f32, f32, f32) {
        let (r, g, b, a) = self.to_rgba(format);
        (
            r as f32 / 255.,
            g as f32 / 255.,
            b as f32 / 255.,
            a as f32 / 255.,
        )
    }
}

#[derive(Clone, Copy, Default)]
#[repr(transparent)]
pub struct Alpha(u8);

impl fmt::Debug for Alpha {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        write!(fmt, "#{:02x}", self.0)
    }
}

impl Alpha {
    pub fn as_u8(&self) -> u8 {
        self.0
    }

    pub fn to_f32(&self) -> f32 {
        (self.0 as f32) / 255.
    }
}

#[derive(Debug)]
#[repr(C)]
struct TmuVertex {
    sow: f32,
    tow: f32,
    oow: f32,
}

#[repr(C)]
pub struct Vertex {
    pub x: f32,
    pub y: f32,
    z: f32,
    pub r: f32,
    pub g: f32,
    pub b: f32,
    ooz: f32,
    pub a: f32,
    oow: f32,
    tmuvtx: [TmuVertex; crate::GLIDE_NUM_TMU],
}

impl fmt::Debug for Vertex {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        let pos = (self.x, self.y);
        let color = (self.r, self.g, self.b, self.a);
        let st = (self.tmuvtx[0].sow, self.tmuvtx[0].tow);
        fmt.debug_struct("Vertex")
            .field("pos", &pos)
            .field("color", &color)
            .field("tex", &st)
            .finish()
    }
}

impl Vertex {
    pub fn s_and_t(&self) -> (f32, f32) {
        let tmuvtx = &self.tmuvtx[0];
        (tmuvtx.sow, tmuvtx.tow)
    }
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum LOD {
    L256 = 0,
    L128 = 1,
    L64 = 2,
    L32 = 3,
    L16 = 4,
    L8 = 5,
    L4 = 6,
    L2 = 7,
    L1 = 8,
}

impl LOD {
    pub fn to_size(self) -> usize {
        match self {
            LOD::L256 => 256,
            LOD::L128 => 128,
            LOD::L64 => 64,
            LOD::L32 => 32,
            LOD::L16 => 16,
            LOD::L8 => 8,
            LOD::L4 => 4,
            LOD::L2 => 2,
            LOD::L1 => 1,
        }
    }

    pub fn to_texture_size(self, aspect: AspectRatio) -> (usize, usize) {
        let size = self.to_size();
        match aspect {
            AspectRatio::A8x1 => (size, size / 8),
            AspectRatio::A4x1 => (size, size / 4),
            AspectRatio::A2x1 => (size, size / 2),
            AspectRatio::A1x1 => (size, size),
            AspectRatio::A1x2 => (size / 2, size),
            AspectRatio::A1x4 => (size / 4, size),
            AspectRatio::A1x8 => (size / 8, size),
        }
    }
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum AspectRatio {
    A8x1 = 0,
    A4x1 = 1,
    A2x1 = 2,
    A1x1 = 3,
    A1x2 = 4,
    A1x4 = 5,
    A1x8 = 6,
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum TextureFormat {
    Rgb332 = 0,
    Yiq422 = 1,
    Alpha8 = 2,
    Intensity8 = 3,
    AlphaIntensity44 = 4,
    P8 = 5,
    Rsvd0 = 6,
    Rsvd1 = 7,
    Argb8332 = 8,
    Ayiq8422 = 9,
    Rgb565 = 10,
    Argb1555 = 11,
    Argb4444 = 12,
    AlphaIntensity88 = 13,
    Ap88 = 14,
    Rsvd2 = 15,
}

impl TextureFormat {
    pub fn bytes_per_pixel(self) -> usize {
        match self {
            TextureFormat::Rgb332
            | TextureFormat::Yiq422
            | TextureFormat::Alpha8
            | TextureFormat::Intensity8
            | TextureFormat::AlphaIntensity44
            | TextureFormat::P8 => 1,
            TextureFormat::Argb8332
            | TextureFormat::Ayiq8422
            | TextureFormat::Rgb565
            | TextureFormat::Argb1555
            | TextureFormat::Argb4444
            | TextureFormat::AlphaIntensity88
            | TextureFormat::Ap88 => 2,
            TextureFormat::Rsvd0 | TextureFormat::Rsvd1 | TextureFormat::Rsvd2 => {
                panic!("Reserved texture format: {:?}", self)
            }
        }
    }
}

#[derive(Debug)]
#[repr(C)]
pub struct TexInfo {
    smallLod: LOD,
    largeLod: LOD,
    aspectRatio: AspectRatio,
    format: TextureFormat,
    data: *mut c_void,
}

impl TexInfo {
    pub fn empty(
        smallLod: LOD,
        largeLod: LOD,
        aspectRatio: AspectRatio,
        format: TextureFormat,
    ) -> TexInfo {
        TexInfo {
            smallLod,
            largeLod,
            aspectRatio,
            format,
            data: null_mut(),
        }
    }

    pub fn get_dimensions(&self) -> (usize, usize) {
        // We ignore mipmaps here.
        self.largeLod.to_texture_size(self.aspectRatio)
    }

    pub fn get_size(&self) -> usize {
        // TODO: maybe don’t ignore mipmaps?
        let (width, height) = self.largeLod.to_texture_size(self.aspectRatio);
        width * height * self.format.bytes_per_pixel()
    }

    pub fn format(&self) -> TextureFormat {
        self.format
    }

    pub fn get_data(&self) -> &mut [u8] {
        let size = self.get_size();
        unsafe { std::slice::from_raw_parts_mut(self.data as *mut u8, size) }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
#[repr(i32)]
pub enum Buffer {
    Front = 0,
    Back = 1,
    Aux = 2,
    Depth = 3,
    Alpha = 4,
    Triple = 5,
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum MipMapMode {
    Disable = 0,
    Nearest = 1,
    NearestDither = 2,
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum AlphaBlendFnc {
    Zero = 0,
    SrcAlpha = 1,
    SrcColor = 2,
    //DstColor = SrcColor,
    DstAlpha = 3,
    One = 4,
    OneMinusSrcAlpha = 5,
    OneMinusSrcColor = 6,
    //OneMinusDstColor = OneMinusSrcColor,
    OneMinusDstAlpha = 7,
    AlphaSaturate = 15,
    //PrefogColor = AlphaSaturate,
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum ChipID {
    Tmu0 = 0,
    Tmu1 = 1,
    Tmu2 = 2,
    Fbi = 3,
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum CombineFunction {
    Zero = 0,
    Local = 1,
    LocalAlpha = 2,
    ScaleOther = 3,
    ScaleOtherAddLocal = 4,
    ScaleOtherAddLocalAlpha = 5,
    ScaleOtherMinusLocal = 6,
    ScaleOtherMinusLocalAddLocal = 7,
    ScaleOtherMinusLocalAddLocalAlpha = 8,
    ScaleMinusLocalAddLocal = 9,
    // TODO: Is it really 0x10 and not 10?
    ScaleMinusLocalAddLocalAlpha = 16,
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum CombineFactor {
    Zero = 0,
    Local = 1,
    OtherAlpha = 2,
    LocalAlpha = 3,
    TextureAlpha = 4,
    TextureRgb = 5,
    // LodFraction = 5
    One = 8,
    OneMinusLocal = 9,
    OneMinusOtherAlpha = 10,
    OneMinusLocalAlpha = 11,
    OneMinusTextureAlpha = 12,
    OneMinusLodFraction = 13,
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum CombineLocal {
    Iterated = 0,
    Constant = 1,
    Depth = 2,
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum CombineOther {
    Iterated = 0,
    Texture = 1,
    Constant = 2,
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum TextureFilterMode {
    PointSampled = 0,
    Bilinear = 1,
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum CmpFnc {
    Never = 0,
    Less = 1,
    Equal = 2,
    LEqual = 3,
    Greater = 4,
    NotEqual = 5,
    GEqual = 6,
    Always = 7,
}

#[derive(Clone, Copy, Debug, PartialEq)]
#[repr(i32)]
pub enum ScreenRefresh {
    R60Hz = 0,
    R70Hz = 1,
    R72Hz = 2,
    R75Hz = 3,
    R80Hz = 4,
    R90Hz = 5,
    R100Hz = 6,
    R85Hz = 7,
    R120Hz = 8,
    None = 0xff,
}

impl From<ScreenRefresh> for usize {
    fn from(refresh: ScreenRefresh) -> usize {
        use self::ScreenRefresh::*;
        match refresh {
            R60Hz => 60,
            R70Hz => 70,
            R72Hz => 72,
            R75Hz => 75,
            R80Hz => 80,
            R90Hz => 90,
            R100Hz => 100,
            R85Hz => 85,
            R120Hz => 120,
            None => unimplemented!("Refresh rate must be set currently"),
        }
    }
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum ScreenResolution {
    R320x200 = 0,
    R320x240 = 1,
    R400x256 = 2,
    R512x384 = 3,
    R640x200 = 4,
    R640x350 = 5,
    R640x400 = 6,
    R640x480 = 7,
    R800x600 = 8,
    R960x720 = 9,
    R856x480 = 10,
    R512x256 = 11,
    R1024x768 = 12,
    R1280x1024 = 13,
    R1600x1200 = 14,
    R400x300 = 15,
    None = 0xff,
}

impl From<ScreenResolution> for (usize, usize) {
    fn from(res: ScreenResolution) -> (usize, usize) {
        use self::ScreenResolution::*;
        match res {
            R320x200 => (320, 200),
            R320x240 => (320, 240),
            R400x256 => (400, 256),
            R512x384 => (512, 384),
            R640x200 => (640, 200),
            R640x350 => (640, 350),
            R640x400 => (640, 400),
            R640x480 => (640, 480),
            R800x600 => (800, 600),
            R960x720 => (960, 720),
            R856x480 => (856, 480),
            R512x256 => (512, 256),
            R1024x768 => (1024, 768),
            R1280x1024 => (1280, 1024),
            R1600x1200 => (1600, 1200),
            R400x300 => (400, 300),
            None => unimplemented!("Using the window’s size requires winsys integration."),
        }
    }
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum ColorFormat {
    Argb = 0,
    Abgr = 1,
    Rgba = 2,
    Bgra = 3,
}

impl Default for ColorFormat {
    fn default() -> ColorFormat {
        ColorFormat::Argb
    }
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum CullMode {
    Disable = 0,
    Negative = 1,
    Positive = 2,
}

impl Default for CullMode {
    fn default() -> CullMode {
        CullMode::Disable
    }
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum OriginLocation {
    UpperLeft = 0,
    LowerLeft = 1,
    Any = 0xff,
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum DepthBufferMode {
    Disable = 0,
    ZBuffer = 1,
    WBuffer = 2,
    ZBufferCompareToBias = 3,
    WBufferCompareToBias = 4,
}

// Compatibility enums for glide.dll

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum LfbBypassMode {
    Disable = 0,
    Enable = 1,
}

#[derive(Clone, Copy, Debug, PartialEq)]
#[repr(i32)]
pub enum LfbWriteMode {
    Rgb565 = 0,
    Rgb555 = 1,
    Argb1555 = 2,
    Rgb888 = 4,
    Argb8888 = 5,
    Rgb565Depth = 12,
    Rgb555Depth = 13,
    Argb1555Depth = 14,
    Za16 = 15,
    Any = 0xff,
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum ChromakeyMode {
    Disable = 0,
    Enable = 1,
}

#[derive(Clone, Copy, Debug, PartialEq)]
#[repr(i32)]
pub enum PassthruMode {
    Vga = 0,
    Sst1 = 1,
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum AlphaSource {
    CcAlpha = 0,
    IteratedAlpha = 1,
    TextureAlpha = 2,
    TextureAlphaTimesIteratedAlpha = 3,
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum ColorCombineFnc {
    Zero = 0,
    Ccrgb = 1,
    Itrgb = 2,
    ItrgbDelta0 = 3,
    DecalTexture = 4,
    TextureTimesCcrgb = 5,
    TextureTimesItrgb = 6,
    TextureTimesItrgbDelta0 = 7,
    TextureTimesItrgbAddAlpha = 8,
    TextureTimesAlpha = 9,
    TextureTimesAlphaAddItrgb = 10,
    TextureAddItrgb = 11,
    TextureSubItrgb = 12,
    CcrgbBlendItrgbOnTexalpha = 13,
    DiffSpecA = 14,
    DiffSpecB = 15,
    One = 16,
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
pub enum TextureCombineFnc {
    Zero = 0,
    Decal = 1,
    Other = 2,
    Add = 3,
    Multiply = 4,
    Subtract = 5,
    Detail = 6,
    DetailOther = 7,
    TrilinearOdd = 8,
    TrilinearEven = 9,
    One = 10,
}
