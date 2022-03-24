#![feature(once_cell)]

use core::ptr::null;

pub mod parse;

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
enum LOD {
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

impl From<u32> for LOD {
    fn from(value: u32) -> LOD {
        match value {
            0 => LOD::L256,
            1 => LOD::L128,
            2 => LOD::L64,
            3 => LOD::L32,
            4 => LOD::L16,
            5 => LOD::L8,
            6 => LOD::L4,
            7 => LOD::L2,
            8 => LOD::L1,
            _ => panic!("Unknown LOD {value}"),
        }
    }
}

impl LOD {
    fn to_size(self) -> usize {
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

    fn to_texture_size(self, aspect: AspectRatio) -> (usize, usize) {
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

// XXX: changes for glide3!
#[derive(Clone, Copy, Debug)]
#[repr(i32)]
enum AspectRatio {
    A8x1 = 0,
    A4x1 = 1,
    A2x1 = 2,
    A1x1 = 3,
    A1x2 = 4,
    A1x4 = 5,
    A1x8 = 6,
}

impl From<u32> for AspectRatio {
    fn from(value: u32) -> AspectRatio {
        match value {
            0 => AspectRatio::A8x1,
            1 => AspectRatio::A4x1,
            2 => AspectRatio::A2x1,
            3 => AspectRatio::A1x1,
            4 => AspectRatio::A1x2,
            5 => AspectRatio::A1x4,
            6 => AspectRatio::A1x8,
            _ => panic!("Unknown aspect ratio {value}"),
        }
    }
}

#[derive(Clone, Copy, Debug)]
#[repr(i32)]
enum TextureFormat {
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

impl From<u32> for TextureFormat {
    fn from(value: u32) -> TextureFormat {
        match value {
            0 => TextureFormat::Rgb332,
            1 => TextureFormat::Yiq422,
            2 => TextureFormat::Alpha8,
            3 => TextureFormat::Intensity8,
            4 => TextureFormat::AlphaIntensity44,
            5 => TextureFormat::P8,
            6 => TextureFormat::Rsvd0,
            7 => TextureFormat::Rsvd1,
            8 => TextureFormat::Argb8332,
            9 => TextureFormat::Ayiq8422,
            10 => TextureFormat::Rgb565,
            11 => TextureFormat::Argb1555,
            12 => TextureFormat::Argb4444,
            13 => TextureFormat::AlphaIntensity88,
            14 => TextureFormat::Ap88,
            15 => TextureFormat::Rsvd2,
            _ => panic!("Unknown texture format {value}"),
        }
    }
}

impl TextureFormat {
    fn bytes_per_pixel(self) -> usize {
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
    small: LOD,
    large: LOD,
    aspect: AspectRatio,
    format: TextureFormat,
    data: *const u8,
}

impl TexInfo {
    pub fn new(small: u32, large: u32, aspect: u32, format: u32) -> TexInfo {
        TexInfo {
            small: LOD::from(small),
            large: LOD::from(large),
            aspect: AspectRatio::from(aspect),
            format: TextureFormat::from(format),
            data: null(),
        }
    }
}
