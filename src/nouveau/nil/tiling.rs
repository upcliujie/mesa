// Copyright © 2024 Collabora, Ltd.
// SPDX-License-Identifier: MIT

use crate::extent::{self, nil_extent4d_px_to_el, units, Extent4D};
use crate::format::Format;
use crate::image::{
    self, Image, ImageDim, ImageUsageFlags, SampleLayout, IMAGE_USAGE_2D_VIEW_BIT, IMAGE_USAGE_LINEAR_BIT
};
use crate::ILog2Ceil;

// TODO: There's probably a better solution than this to get pointers for buffers
extern crate libc;
use libc::c_char;

pub const GOB_WIDTH_B: u32 = 64;
pub const GOB_DEPTH: u32 = 1;

pub fn gob_height(gob_height_is_8: bool) -> u32 {
    if gob_height_is_8 {
        8
    } else {
        4
    }
}

#[derive(Clone, Debug, Default, Copy, PartialEq)]
#[repr(C)]
pub struct Tiling {
    pub is_tiled: bool,
    /// Whether the GOB height is 4 or 8
    pub gob_height_is_8: bool,
    /// log2 of the X tile dimension in GOBs
    pub x_log2: u8,
    /// log2 of the Y tile dimension in GOBs
    pub y_log2: u8,
    /// log2 of the z tile dimension in GOBs
    pub z_log2: u8,
}

impl Tiling {
    /// Clamps the tiling to less than 2x the given extent in each dimension.
    ///
    /// This operation is done by the hardware at each LOD.
    pub fn clamp(&self, extent_B: Extent4D<units::Bytes>) -> Self {
        let mut tiling = *self;

        if !self.is_tiled {
            return tiling;
        }

        let tiling_extent_B = self.extent_B();

        if extent_B.width < tiling_extent_B.width
            || extent_B.height < tiling_extent_B.height
            || extent_B.depth < tiling_extent_B.depth
        {
            tiling.x_log2 = 0;
        }

        let extent_GOB = extent_B.to_GOB(tiling.gob_height_is_8);

        let ceil_h = extent_GOB.height.ilog2_ceil() as u8;
        let ceil_d = extent_GOB.depth.ilog2_ceil() as u8;

        tiling.y_log2 = std::cmp::min(tiling.y_log2, ceil_h);
        tiling.z_log2 = std::cmp::min(tiling.z_log2, ceil_d);
        tiling
    }

    pub fn size_B(&self) -> u32 {
        let extent_B = self.extent_B();
        extent_B.width * extent_B.height * extent_B.depth * extent_B.array_len
    }

    #[no_mangle]
    pub extern "C" fn nil_tiling_size_B(&self) -> u32 {
        self.size_B()
    }

    pub fn extent_B(&self) -> Extent4D<units::Bytes> {
        if self.is_tiled {
            Extent4D::new(
                GOB_WIDTH_B << self.x_log2,
                gob_height(self.gob_height_is_8) << self.y_log2,
                GOB_DEPTH << self.z_log2,
                1,
            )
        } else {
            // We handle linear images in Image::new()
            Extent4D::new(1, 1, 1, 1)
        }
    }
}

pub fn sparse_block_extent_el(
    format: Format,
    dim: ImageDim,
) -> Extent4D<units::Elements> {
    let bits = format.el_size_B() * 8;

    // Taken from Vulkan 1.3.279 spec section entitled "Standard Sparse
    // Image Block Shapes".
    match dim {
        ImageDim::_2D => match bits {
            8 => Extent4D::new(256, 256, 1, 1),
            16 => Extent4D::new(256, 128, 1, 1),
            32 => Extent4D::new(128, 128, 1, 1),
            64 => Extent4D::new(128, 64, 1, 1),
            128 => Extent4D::new(64, 64, 1, 1),
            other => panic!("Invalid texel size {other}"),
        },
        ImageDim::_3D => match bits {
            8 => Extent4D::new(64, 32, 32, 1),
            16 => Extent4D::new(32, 32, 32, 1),
            32 => Extent4D::new(32, 32, 16, 1),
            64 => Extent4D::new(32, 16, 16, 1),
            128 => Extent4D::new(16, 16, 16, 1),
            _ => panic!("Invalid texel size"),
        },
        _ => panic!("Invalid sparse image dimension"),
    }
}

pub fn sparse_block_extent_px(
    format: Format,
    dim: ImageDim,
    sample_layout: SampleLayout,
) -> Extent4D<units::Pixels> {
    sparse_block_extent_el(format, dim)
        .to_sa(format)
        .to_px(sample_layout)
}

pub fn sparse_block_extent_B(
    format: Format,
    dim: ImageDim,
) -> Extent4D<units::Bytes> {
    sparse_block_extent_el(format, dim).to_B(format)
}

#[no_mangle]
pub extern "C" fn nil_sparse_block_extent_px(
    format: Format,
    dim: ImageDim,
    sample_layout: SampleLayout,
) -> Extent4D<units::Pixels> {
    sparse_block_extent_px(format, dim, sample_layout)
}

impl Tiling {
    pub fn sparse(format: Format, dim: ImageDim) -> Self {
        let sparse_block_extent_B = sparse_block_extent_B(format, dim);

        assert!(sparse_block_extent_B.width.is_power_of_two());
        assert!(sparse_block_extent_B.height.is_power_of_two());
        assert!(sparse_block_extent_B.depth.is_power_of_two());

        let gob_height_is_8 = true;
        let sparse_block_extent_gob =
            sparse_block_extent_B.to_GOB(gob_height_is_8);

        Self {
            is_tiled: true,
            gob_height_is_8,
            x_log2: sparse_block_extent_gob.width.ilog2().try_into().unwrap(),
            y_log2: sparse_block_extent_gob.height.ilog2().try_into().unwrap(),
            z_log2: sparse_block_extent_gob.depth.ilog2().try_into().unwrap(),
        }
    }

    pub fn choose(
        extent_px: Extent4D<units::Pixels>,
        format: Format,
        sample_layout: SampleLayout,
        usage: ImageUsageFlags,
    ) -> Tiling {
        if (usage & IMAGE_USAGE_LINEAR_BIT) != 0 {
            return Default::default();
        }

        let mut tiling = Tiling {
            is_tiled: true,
            gob_height_is_8: true,
            x_log2: 0,
            y_log2: 5,
            z_log2: 5,
        };

        if (usage & IMAGE_USAGE_2D_VIEW_BIT) != 0 {
            tiling.z_log2 = 0;
        }

        tiling.clamp(extent_px.to_B(format, sample_layout))
    }
}

// This section is dedicated to the internal tiling layout, as well as
// CPU-based tiled memcpy implementations (and helpers) for EXT_Host_Image_Copy
//
// Work here is based on isl_tiled_memcpy, fd6_tiled_memcpy, old work by Rebecca Mckeever,
// and https://fgiesen.wordpress.com/2011/01/17/texture-tiling-and-swizzling/
//
// On NVIDIA, the tiling system is a two-tier one, and images are first tiled in
// a grid of rows of tiles (called "Blocks") with one or more columns:
//
// +----------+----------+----------+----------+
// | Block 0  | Block 1  | Block 2  | Block 3  |
// +----------+----------+----------+----------+
// | Block 4  | Block 5  | Block 6  | Block 7  |
// +----------+----------+----------+----------+
// | Block 8  | Block 9  | Block 10 | Block 11 |
// +----------+----------+----------+----------+
//
// The blocks themselves are ordered linearly as can be seen above, which is
// where the "Block Linear" naming comes from for NVIDIA's tiling scheme.
//
// For 3D images, each block continues in the Z direction such that tiles
// contain multiple Z slices. If the image depth is longer than the
// block depth, there will be more than one layer of blocks, where a layer is
// made up of 1 or more Z slices. For example, if the above tile pattern was
// the first layer of a multilayer arrangement, the second layer would be:
//
// +----------+----------+----------+----------+
// | Block 12 | Block 13 | Block 14 | Block 15 |
// +----------+----------+----------+----------+
// | Block 16 | Block 17 | Block 18 | Block 19 |
// +----------+----------+----------+----------+
// | Block 20 | Block 21 | Block 22 | Block 23 |
// +----------+----------+----------+----------+
//
// The number of rows, columns, and layers of tiles can thus be deduced to be:
//    rows    >= ceiling(image_height / block_height)
//    columns >= ceiling(image_width  / block_width)
//    layers  >= ceiling(image_depth  / block_depth)
//
// Where block_width is a constant 64B (unless for sparse) and block_height
// can be either 8 or 16 GOBs tall (more on GOBs below). For us, block_depth
// is one for now.
//
// The >= is in case the blocks around the edges are partial.
//
// Now comes the second tier. Each block is composed of GOBs (Groups of Bytes)
// arranged in ascending order in a single column:
//
// +---------------------------+
// |           GOB 0           |
// +---------------------------+
// |           GOB 1           |
// +---------------------------+
// |           GOB 2           |
// +---------------------------+
// |           GOB 3           |
// +---------------------------+
//
// The number of GOBs in a full block is
//    block_height * block_depth
//
// An Ampere GOB is 512 bytes, arranged in a 64x8 layout and split into Sectors.
// Each Sector is 32 Bytes, and the Sectors in a GOB are arranged in a 16x2
// layout (i.e., two 16B lines on top of each other). It's then arranged into
// two columns that are 2 sectors by 4, leading to a 4x4 grid of sectors:
//
// +----------+----------+----------+----------+
// | Sector 0 | Sector 1 | Sector 0 | Sector 1 |
// +----------+----------+----------+----------+
// | Sector 2 | Sector 3 | Sector 2 | Sector 3 |
// +----------+----------+----------+----------+
// | Sector 4 | Sector 5 | Sector 4 | Sector 5 |
// +----------+----------+----------+----------+
// | Sector 6 | Sector 7 | Sector 6 | Sector 7 |
// +----------+----------+----------+----------+
//
// From the given pixel address equations in the Orin manual, we arrived at
// the following bit interleave pattern for the pixel address:
//
//      b8 b7 b6 b5 b4 b3 b2 b1 b0
//      --------------------------
//      x5 y2 y1 x4 y0 x3 x2 x1 x0
//
// TODO: Element ordering within a sector
//

impl Tiling {
    
    // Utility functions

    /// Get an offset into a pixel inside a GOB
    /// TODO: double-check + account for different bpp, and incorporate GOB address
    fn get_pixel_offset(
        x: usize,
        y: usize,
    ) -> usize {
        (x & 15)       |
        (y & 1)  << 4  |
        (x & 16) << 1  |
        (y & 2)  << 5  |
        (x & 32) << 3
    }

    /// Get a byte offset of a point
    fn get_byte_offset(
        x: usize,
        y: usize,
        z: usize,
        row_stride_B: usize,
        slice_stride_B: usize,
    ) -> usize {
        x +
        y * row_stride_B +
        z * slice_stride_B
    }
    
    /// Get a linear offset into a block from tiled point coordinates
    fn get_block_offset(
        x: usize,
        y: usize,
        z: usize,
        row_stride_tl: usize,
        slice_stride_tl: usize,
        tiling: Self,
    ) -> usize {
        (x >> 6) +
        (y >> (3 + tiling.y_log2)) * row_stride_tl +
        (z >> tiling.z_log2) * slice_stride_tl
    }

    /// Get a linear offset into a GOB from tiled point coordinates
    fn get_gob_offset(
        x: usize,
        y: usize,
        z: usize,
        x_mask: usize,
        y_mask: usize,
        z_mask: usize,
        tiling: Self,
    ) -> usize {
        ((x >> tiling.x_log2) & x_mask) +
        ((y >> 3) & y_mask) +
        ((z & z_mask) << tiling.y_log2)
    }

    // Block/GOB granularity copy functions
    //
    // For the general case, blocks and GOBs are comprised of 9 parts as outlined below:
    //
    //                   x_start   x_whole_tile_start   x_whole_tile_end x_end
    //          
    //         y_start    |---------|--------------------------|---------|
    //                    |         |                          |         |
    // y_whole_tile_start |---------|--------------------------|---------|
    //                    |         |                          |         |
    //                    |         |      whole tile area     |         |
    //                    |         |                          |         |
    //   y_whole_tile_end |---------|--------------------------|---------|
    //                    |         |                          |         |
    //      y_end         |---------|--------------------------|---------|
    //
    // The whole block/GOB areas are fully aligned and can be fast-pathed, while
    // the other unaligned/incomplete areas need dedicated handling

    fn small_unaligned_memcpy(
        start_px: Extent4D<units::Pixels>,
        extent_px: Extent4D<units::Pixels>,
        miplevel: u8, 
        nil: Image,
        pitch: usize,
        slice: usize,
        Bpp: u8,
        src: *const c_char,
        dst: *const c_char,
    ) {
        //TODO: This would handle each individual part alone.
    }

    fn optimized_big_memcpy(
        start_px: Extent4D<units::Pixels>,
        extent_px: Extent4D<units::Pixels>,
        miplevel: u8, 
        nil: Image,
        pitch: usize,
        slice: usize,
        Bpp: u8,
        src: *const c_char,
        dst: *const c_char,
    ) {
        //TODO: This would handle splitting the blocks and handle each of the 9 parts.
    }

    // High level copy functions (buffer/image granularity)

    /// Copy a region bound by start_px and extent_px to a buffer arranged 
    /// in a tiled layout.
    pub fn linear_to_tiled(
        start_px: Extent4D<units::Pixels>,
        extent_px: Extent4D<units::Pixels>,
        miplevel: u8, 
        nil: Image,
        pitch: usize,
        slice: usize,
        Bpp: u8,
        src: *const c_char,
        dst: *const c_char,
    ) {
        // TODO: Write the memcpy() equivalent. There are multiple ways in Rust
        // but unsure of the best yet

        // Temporary constant to specify whether we use the naive implementation
        // or the optimized
        const NAIVE_COPY_MTHD: bool = true;

        let start_B = start_px.to_B(nil.format, nil.sample_layout);
        let extent_B = extent_px.to_B(nil.format, nil.sample_layout);

        let tiling: Self = nil.levels[miplevel as usize].tiling;
        let start_tl = start_px.to_tl(tiling, nil.format, nil.sample_layout);
        let extent_tl = extent_px.to_tl(tiling, nil.format, nil.sample_layout);

        let row_stride_tl = extent_tl.width;
        let slice_stride_tl = extent_tl.width * extent_tl.height;

        let start_gob = start_B.to_GOB((tiling.gob_height_is_8));
        let extent_gob = extent_B.to_GOB((tiling.gob_height_is_8));
        
        let x_mask = (1 << tiling.x_log2) - 1;
        let y_mask = (1 << tiling.y_log2) - 1;
        let z_mask = (1 << tiling.z_log2) - 1;

        if NAIVE_COPY_MTHD {
            for z_px in start_px.depth..extent_px.depth {
                for y_px in start_px.height..extent_px.height {
                    for x_px in start_px.width..extent_px.width {
                        
                        let gob_addr_B = src +
                        (y_px / (8 * block_height)) * 512 * block_height * image_width_in_gobs +
                        (x_px * Bpp / 64) * 512 * block_height +
                        (y_px % (8 * block_height) / 8) * 512;
                        
                        let px_addr_B = gob_addr_B +
                        ((x_px * Bpp % 64) / 32) * 256 + ((y_px % 8) / 2) * 64 +
                        ((x_px * Bpp % 32) / 16) * 32 + (y_px % 2) * 16 + (x_px * Bpp % 16);

                        let linear_addr_B = x_B + y_B * pitch + z_B * slice;

                        // rust_memcpy(linear_addr_B, tiled_addr_B, px_size_B);
                    }
                }
            }
        } else {
            //TODO: use optimized copy functions above
            for z_tl in start_tl.depth..extent_tl.depth {
                for y_tl in start_tl.height..extent_tl.height {
                    for x_tl in start_tl.width..extent_tl.width {
                        // TODO: Walk GOBs, and enter copy function for each GOB
                    }
                }
            }
        }
    }

    /// Copy a region bound by start_px and extent_px to a buffer arranged 
    /// in a linear layout.
    pub fn tiled_to_linear(
        start_px: Extent4D<units::Pixels>,
        extent_px: Extent4D<units::Pixels>,
        miplevel: u8, 
        nil: Image,
        src: *const c_char,
        dst: *const c_char,
    ) {
        // Temporary constant to specify whether we use the naive implementation
        // or the optimized
        const NAIVE_COPY_MTHD: bool = true;

        let start_B = start_px.to_B(nil.format, nil.sample_layout);
        let extent_B = extent_px.to_B(nil.format, nil.sample_layout);

        let tiling: Self = nil.levels[miplevel as usize].tiling;
        let start_tl = start_px.to_tl(tiling, nil.format, nil.sample_layout);
        let extent_tl = extent_px.to_tl(tiling, nil.format, nil.sample_layout);

        let tile_size_B = tiling.size_B();   

        let row_stride_tl = extent_tl.width;
        let slice_stride_tl = extent_tl.width * extent_tl.height;
        
        let x_mask = (1 << tiling.x_log2) - 1;
        let y_mask = (1 << tiling.y_log2) - 1;
        let z_mask = (1 << tiling.z_log2) - 1;

        if NAIVE_COPY_MTHD {
            for z_px in start_px.depth..extent_px.depth {
                for y_px in start_px.height..extent_px.height {
                    for x_px in start_px.width..extent_px.width {
                        
                        let gob_addr_B = src +
                        (y_px / (8 * block_height)) * 512 * block_height * image_width_in_gobs +
                        (x_px * Bpp / 64) * 512 * block_height +
                        (y_px % (8 * block_height) / 8) * 512;
                        
                        let px_addr_B = gob_addr_B +
                        ((x_px * Bpp % 64) / 32) * 256 + ((y_px % 8) / 2) * 64 +
                        ((x_px * Bpp % 32) / 16) * 32 + (y_px % 2) * 16 + (x_px * Bpp % 16);

                        let linear_addr_B = x_B + y_B * pitch + z_B * slice;

                        // rust_memcpy(tiled_addr_B, linear_addr_B, px_size_B);
                    }
                }
            }
        } else {
            //TODO: use optimized copy functions above
            for z_tl in start_tl.depth..extent_tl.depth {
                for y_tl in start_tl.height..extent_tl.height {
                    for x_tl in start_tl.width..extent_tl.width {
                        
                    }
                }
            }
        }
    }
}